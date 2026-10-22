#include "../client/includes.hpp"

void Server::signalHandler(int sig) {
    (void)sig;
    g_shutdown_requested = true;
}

Server::Server(int port, std::string password) : _port(port), _serverSocket(-1), _serverName("irc.Brika.net"), _password(password) {}

std::string Server::getName() const {
    return _serverName;
}

Server::~Server() {
    for (size_t i = 0; i < _pollfds.size(); i++) {
        close(_pollfds[i].fd);
    }
    if (_serverSocket != -1) {
        close(_serverSocket);
    }
}

void Server::Init() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) throw std::runtime_error("Failed to create socket");

    int opt = 1;
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(_serverSocket, F_SETFL, O_NONBLOCK); // ??

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
        if (poll(&_pollfds[0], _pollfds.size(), -1) < 0)
            continue;

        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents == 0) 
                continue;

            if (_pollfds[i].revents & POLLIN) {
                if (_pollfds[i].fd == _serverSocket)
                    AcceptNewClient();
                else
                    ReceiveNewData(_pollfds[i].fd, channels);
            }

            if (_pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                ClearClient(_pollfds[i].fd, channels);
                i--;
            }
        }
    }
}

void Server::AcceptNewClient() {
    int client_fd = accept(_serverSocket, NULL, NULL);
    if (client_fd != -1) {
        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        
        struct pollfd cli;
        cli.fd = client_fd;
        cli.events = POLLIN;
        cli.revents = 0;
        _pollfds.push_back(cli);
        
        std::cout << "New client connected: " << client_fd << std::endl;
    }
	else
		std::cerr << "[SERVER]: fail to accept new client" << std::endl;
}

void Server::ReceiveNewData(int fd, ChannelRegistry& channels) {
    std::vector<char> buffer(1024, '\0');
    ssize_t bytes = recv(fd, &buffer[0], buffer.size() - 1, 0);

    if (bytes > 0) {
		
        _clientBuffers[fd].getInputBuffer().append(&buffer[0], bytes);
        
        size_t pos;
        while ((pos = _clientBuffers[fd].getInputBuffer().find("\n")) != std::string::npos) {
            std::string command = _clientBuffers[fd].getInputBuffer().substr(0, pos);

            _clientBuffers[fd].getInputBuffer().erase(0, pos + 1);

            if (!command.empty() && command[command.length() - 1] == '\r')
                command.erase(command.length() - 1);
				HandleCommand(fd, _clientBuffers, _password, command, channels, this->_bot);
			}
    } else if (bytes == 0) {
        std::cout << "Client " << fd << " disconnected." << std::endl;
        ClearClient(fd, channels);
    }
}

void Server::ClearClient(int fd, ChannelRegistry& channels) {
    channels.removeClientFromAllChannels(fd);
    close(fd);
    _clientBuffers.erase(fd);
	_bot.removeClient(fd);
    std::vector<struct pollfd>::iterator it = _pollfds.begin();
    
    while (it != _pollfds.end()) {
        if (it->fd == fd)
            it = _pollfds.erase(it);
        else
            ++it;
    }
}
