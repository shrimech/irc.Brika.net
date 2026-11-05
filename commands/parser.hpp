#ifndef COMMAND_PARSER_HPP
#define COMMAND_PARSER_HPP

#include <string>
#include <vector>

struct Command {
    std::string command;
    std::vector<std::string> params;
};

Command parseCommand(const std::string& commandLine);

#endif
