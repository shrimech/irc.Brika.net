#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "includes.hpp"

class Client : public Authentification
{
    protected:
        int fd;
        std::string inputBuffer;
        
    public:
        Client();
        ~Client();

        void setFd(int fd);
        int getFd() const;
        std::string& getInputBuffer();
        void sendMessage(const std::string &message);
};

#endif