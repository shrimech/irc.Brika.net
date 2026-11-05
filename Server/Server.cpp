#include "Server.hpp"
#include "../commands/commands.hpp"
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <utility>

static volatile std::sig_atomic_t g_shutdown_requested = 0;

static std::chrono::seconds readDurationSetting(const char* name, int defaultSeconds) {
    const char* value = std::getenv(name);
    if (!value)
        return std::chrono::seconds(defaultSeconds);

    int seconds = 0;
    const std::string input(value);
    const auto [end, error] = std::from_chars(
        input.data(), input.data() + input.size(), seconds);
    if (error != std::errc{} || end != input.data() + input.size() || seconds <= 0)
        throw std::invalid_argument(std::string(name) + " must be a positive number of seconds");
    return std::chrono::seconds(seconds);
}

void Server::signalHandler(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

Server::Server(int port, std::string password)
    : _port(port),
      _serverSocket(-1),
      _serverName("irc.Brika.net"),
      _password(std::move(password)),
      _pingInterval(readDurationSetting("IRC_PING_INTERVAL_SECONDS", 60)),
      _pingTimeout(readDurationSetting("IRC_PING_TIMEOUT_SECONDS", 30)),
      _nextPingId(0) {}

std::string Server::getName() const {
    return _serverName;
}

Server::~Server() {
    bool serverSocketClosed = false;
    for (const auto& descriptor : _pollfds) {
        close(descriptor.fd);
        if (descriptor.fd == _serverSocket)
            serverSocketClosed = true;
    }
    if (_serverSocket != -1 && !serverSocketClosed) {
        close(_serverSocket);
    }
    _serverSocket = -1;
}

void Server::Init() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) throw std::runtime_error("Failed to create socket");

    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("Failed to configure server socket");
    int flags = fcntl(_serverSocket, F_GETFL, 0);
    if (flags < 0 || fcntl(_serverSocket, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Failed to set server socket non-blocking");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_serverSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("Failed to bind");

    if (listen(_serverSocket, SOMAXCONN) < 0)
        throw std::runtime_error("Failed to listen");

    struct pollfd srv;
    srv.fd = _serverSocket;
    srv.events = POLLIN;
    srv.revents = 0;
    _pollfds.push_back(srv);

    std::cout << "Server listening on port " << _port << std::endl;
}

void Server::Run() {
    ChannelRegistry channels;
    while (!g_shutdown_requested) {
        int ready = poll(_pollfds.data(), _pollfds.size(), GetPollTimeout());
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("Failed to poll sockets");
        }
        for (size_t i = 0; i < _pollfds.size();) {
            const int fd = _pollfds[i].fd;
            const short events = _pollfds[i].revents;
            if (events == 0) {
                ++i;
                continue;
            }

            if (fd == _serverSocket) {
                if (events & POLLIN)
                    AcceptNewClient();
                if (events & (POLLERR | POLLHUP | POLLNVAL))
                    throw std::runtime_error("Server socket became unavailable");
                ++i;
                continue;
            }

            if (events & POLLIN)
                ReceiveNewData(fd, channels);
            if (!hasPollFd(fd))
                continue;
            if (events & (POLLERR | POLLHUP | POLLNVAL)) {
                ClearClient(fd, channels);
                continue;
            }
            ++i;
        }
        CheckClientLiveness(channels);
    }
}

bool Server::hasPollFd(int fd) const {
    return std::ranges::any_of(_pollfds, [fd](const pollfd& descriptor) {
        return descriptor.fd == fd;
    });
}

int Server::GetPollTimeout() const {
    if (_clientBuffers.empty())
        return -1;

    const auto now = std::chrono::steady_clock::now();
    auto nextDeadline = std::chrono::steady_clock::time_point::max();
    for (const auto& [fd, client] : _clientBuffers) {
        (void)fd;
        nextDeadline = std::min(nextDeadline, client.nextLivenessDeadline(_pingInterval));
    }
    if (nextDeadline <= now)
        return 0;

    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        nextDeadline - now);
    return static_cast<int>(std::min<std::int64_t>(remaining.count() + 1, INT_MAX));
}

void Server::CheckClientLiveness(ChannelRegistry& channels) {
    const auto now = std::chrono::steady_clock::now();
    std::vector<int> timedOut;
    for (auto& [fd, client] : _clientBuffers) {
        switch (client.checkLiveness(now, _pingInterval)) {
            case Client::LivenessAction::disconnect:
                std::cerr << "Client " << fd << " timed out waiting for PONG." << std::endl;
                timedOut.push_back(fd);
                break;
            case Client::LivenessAction::sendPing: {
                const std::string token = _serverName + "-" + std::to_string(++_nextPingId);
                client.startPing(token, now + _pingTimeout);
                client.sendMessage(":" + _serverName + " PING :" + token + "\r\n");
                break;
            }
            case Client::LivenessAction::none:
                break;
        }
    }

    for (const int fd : timedOut)
        ClearClient(fd, channels);
}

void Server::AcceptNewClient() {
    int client_fd = accept(_serverSocket, nullptr, nullptr);
    if (client_fd != -1) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(client_fd);
            throw std::runtime_error("Failed to set client socket non-blocking");
        }
        
        struct pollfd cli;
        cli.fd = client_fd;
        cli.events = POLLIN;
        cli.revents = 0;
        _pollfds.push_back(cli);
        _clientBuffers[client_fd].setFd(client_fd);
        
        std::cout << "New client connected: " << client_fd << std::endl;
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        std::cerr << "[SERVER]: accept failed" << std::endl;
}

void Server::ReceiveNewData(int fd, ChannelRegistry& channels) {
    char buffer[1024];
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes > 0) {
        Client& client = _clientBuffers[fd];
        client.noteActivity();
        client.appendInput(buffer, static_cast<std::size_t>(bytes));
        std::string command;
        while (client.extractLine(command))
            handleCommand(fd, _clientBuffers, _password, command, channels, _bot);
    } else if (bytes == 0) {
        std::cout << "Client " << fd << " disconnected." << std::endl;
        ClearClient(fd, channels);
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        ClearClient(fd, channels);
    }
}

void Server::ClearClient(int fd, ChannelRegistry& channels) {
    std::vector<struct pollfd>::iterator it = _pollfds.begin();
    while (it != _pollfds.end() && it->fd != fd)
        ++it;
    if (it == _pollfds.end())
        return;

    _pollfds.erase(it);
    channels.removeClientFromAllChannels(fd);
    close(fd);
    _clientBuffers.erase(fd);
    _bot.removeClient(fd);
}
