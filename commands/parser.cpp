#include "parser.hpp"
#include <algorithm>
#include <cctype>

Command parseCommand(const std::string& commandLine) {
    Command command;
    std::size_t start = commandLine.find_first_not_of(" \t");
    if (start == std::string::npos)
        return command;

    const std::size_t commandEnd = commandLine.find_first_of(" \t", start);
    command.command = commandLine.substr(start, commandEnd - start);
    std::ranges::transform(command.command, command.command.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    start = commandLine.find_first_not_of(" \t", commandEnd);
    while (start != std::string::npos) {
        if (commandLine[start] == ':') {
            command.params.push_back(commandLine.substr(start + 1));
            break;
        }

        const std::size_t end = commandLine.find_first_of(" \t", start);
        command.params.push_back(commandLine.substr(start, end - start));
        start = commandLine.find_first_not_of(" \t", end);
    }

    return command;
}
