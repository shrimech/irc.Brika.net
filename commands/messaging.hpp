#ifndef COMMAND_MESSAGING_HPP
#define COMMAND_MESSAGING_HPP

#include "parser.hpp"
#include <map>

class Client;
class ChannelRegistry;
class QuizBot;

void handlePrivmsg(Client& client, std::map<int, Client>& clients, const Command& command,
    ChannelRegistry& channels, QuizBot& bot);
void handleNotice(Client& client, std::map<int, Client>& clients, const Command& command,
    ChannelRegistry& channels);

#endif
