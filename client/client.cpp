#include "includes.hpp"

Client::Client() : fd(-1)  {Authentification();}

Client::~Client() {}

void Client::setFd(int fd) {
    this->fd = fd;
}

int Client::getFd() const {
    return fd;
}

std::string& Client::getInputBuffer() {
    return inputBuffer;
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

        if (bytes > 0)
            totalSent += bytes;
        else
			std::cerr << "send() failed: " << std::endl;
            break;
    }
}
