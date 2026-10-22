#ifndef SERVER_HPP
#define SERVER_HPP

#include "../client/client.hpp"
#include "../channel/ChannelRegistry.hpp"
#include "../Bot/Bot.hpp"
#include <csignal>
#include <map>
#include <poll.h>
#include <string>
#include <vector>

class Server {
private:
    int _port;
    int _serverSocket;
    std::string _serverName;
    std::string _password;
    std::vector<struct pollfd> _pollfds;
    std::map<int, Client> _clientBuffers;
    QuizBot _bot;

    void AcceptNewClient();
    void ReceiveNewData(int fd, ChannelRegistry& channels);
    void ClearClient(int fd, ChannelRegistry& channels);
    bool hasPollFd(int fd) const;

public:
    Server(int port, std::string password);
    std::string getName() const;
    ~Server();

    void Init();
    void Run();
    static void signalHandler(int sig);
};



#endif