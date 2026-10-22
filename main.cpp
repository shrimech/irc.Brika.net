#include "Server/Server.hpp"
#include <csignal>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <stdexcept>

static bool isValidPort(const char* str, int& port) {
    if (!str || !str[0])
        return false;
    
    for (int i = 0; str[i]; i++) {
        if (!std::isdigit(str[i]))
            return false;
    }
    
    port = std::atoi(str);
    return port >= 1 && port <= 65535;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./server <port> <password>" << std::endl;
        return 1;
    }

    int port;
    if (!isValidPort(argv[1], port)) {
        std::cerr << "Error: Port must be a number between 1 and 65535" << std::endl;
        return 1;
    }

    std::signal(SIGINT, Server::signalHandler);
    std::signal(SIGTERM, Server::signalHandler);

    try {
        Server irc_server(port, argv[2]);
        irc_server.Init();
        irc_server.Run();
    } 
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server shutdown gracefully." << std::endl;
    return 0;
}
