#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <map>
#include <string>

class Client;
class ChannelRegistry;
class QuizBot;

void handleCommand(int fd, std::map<int, Client>& clients, const std::string& serverPassword,
    const std::string& commandLine, ChannelRegistry& channels, QuizBot& bot);

#endif