#include "Server.hpp"
#include "../client/commands.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

static volatile std::sig_atomic_t g_shutdown_requested = 0;

void Server::signalHandler(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

Server::Server(int port, std::string password) : _port(port), _serverSocket(-1), _serverName("irc.Brika.net"), _password(password) {}

std::string Server::getName() const {
    return _serverName;
}

Server::~Server() {
    bool serverSocketClosed = false;
    for (size_t i = 0; i < _pollfds.size(); i++) {
        close(_pollfds[i].fd);
        if (_pollfds[i].fd == _serverSocket)
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
        int ready = poll(&_pollfds[0], _pollfds.size(), -1);
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
    }
}

bool Server::hasPollFd(int fd) const {
    for (size_t i = 0; i < _pollfds.size(); ++i) {
        if (_pollfds[i].fd == fd)
            return true;
    }
    return false;
}

void Server::AcceptNewClient() {
    int client_fd = accept(_serverSocket, NULL, NULL);
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
        client.appendInput(buffer, static_cast<std::size_t>(bytes));
        std::string command;
        while (client.extractLine(command))
            HandleCommand(fd, _clientBuffers, _password, command, channels, _bot);
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
