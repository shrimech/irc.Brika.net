#include "client.hpp"
#include <iostream>
#include <sys/socket.h>

Client::Client() : Authentification(), fd(-1) {}

Client::~Client() {}

void Client::setFd(int fd) {
    this->fd = fd;
}

int Client::getFd() const {
    return fd;
}

void Client::appendInput(const char* data, std::size_t size) {
    inputBuffer.append(data, size);
}

bool Client::extractLine(std::string& line) {
    const std::string::size_type newline = inputBuffer.find('\n');
    if (newline == std::string::npos)
        return false;

    line = inputBuffer.substr(0, newline);
    inputBuffer.erase(0, newline + 1);
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    return true;
}

void Client::sendMessage(const std::string& message)
{
    size_t totalSent = 0;

    while (totalSent < message.size())
    {
        ssize_t bytes = send(fd,
            message.c_str() + totalSent,
            message.size() - totalSent,
            MSG_NOSIGNAL);

        if (bytes > 0) {
            totalSent += bytes;
        } else {
            std::cerr << "send() failed" << std::endl;
            break;
        }
    }
}
