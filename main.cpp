#include "Server/Server.hpp"
#include <charconv>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>

static bool isValidPort(const char* str, int& port) {
    if (!str || *str == '\0')
        return false;

    const std::string_view input(str);
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), port);
    return error == std::errc{} && end == input.data() + input.size()
        && port >= 1 && port <= 65535;
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
