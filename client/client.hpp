#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "authentification.hpp"
#include <cstddef>
#include <string>

class Client : public Authentification
{
    private:
        int fd;
        std::string inputBuffer;
        
    public:
        Client();
        ~Client();

        void setFd(int fd);
        int getFd() const;
        void appendInput(const char* data, std::size_t size);
        bool extractLine(std::string& line);
        void sendMessage(const std::string &message);
};

#endif