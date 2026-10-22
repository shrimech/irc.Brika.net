#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include "client.hpp"
#include <map>
#include <string>
#include <vector>

class ChannelRegistry;
class QuizBot;

struct Command {
    std::string command;
    std::vector<std::string> params;
};

void executeCommands(Client& client, std::map<int, Client>& clientBuffers, const Command& command, ChannelRegistry& channels, QuizBot& bot);
void HandleCommand(int fd, std::map<int, Client>& clientBuffers, const std::string& serv_pass, const std::string& commandLine, ChannelRegistry& channels, QuizBot& bot);
Command parseCommand(const std::string& commandLine);

// ---------- commands ------------------------------
void PRIVMSG(int fd, const std::map<int, Client>& clientBuffers, Client& client, const Command& command, ChannelRegistry& channels, QuizBot& bot);

#endif