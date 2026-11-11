#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "authentification.hpp"
#include <chrono>
#include <cstddef>
#include <string>

class Client : public Authentification
{
    private:
        int fd;
        std::string inputBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        std::string pendingPingToken;
        std::chrono::steady_clock::time_point pingDeadline;
        std::string awayMessage;
        
    public:
        enum class LivenessAction {
            none,
            sendPing,
            disconnect
        };

        Client();
        ~Client();

        void setFd(int fd);
        int getFd() const;
        void appendInput(const char* data, std::size_t size);
        bool extractLine(std::string& line);
        void sendMessage(const std::string &message);
        void noteActivity();
        LivenessAction checkLiveness(std::chrono::steady_clock::time_point now,
            std::chrono::seconds interval) const;
        void startPing(std::string token, std::chrono::steady_clock::time_point deadline);
        bool acknowledgePing(const std::string& token);
        std::chrono::steady_clock::time_point nextLivenessDeadline(
            std::chrono::seconds interval) const;

        void setAwayMessage(const std::string& reason);
        std::string getAwayMessage() const;
        bool isAway() const;
};

#endif