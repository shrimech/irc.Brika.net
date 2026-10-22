#ifndef CHANNEL_COMMANDS_HPP
#define CHANNEL_COMMANDS_HPP

#include "../client/client.hpp"
#include <map>
#include <string>
#include <vector>

class ChannelRegistry;

std::string clientPrefix(const Client& client);
void replyMsg(const std::string& rcode, const std::string& msg, Client& client);
Client* findClientByNick(std::map<int, Client>& clients, const std::string& nick);

void joinHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params);
void topicHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params);
void inviteHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params, std::map<int, Client>& clients);
void kickHandler(ChannelRegistry& channels, std::map<int, Client>& clients, Client& client, const std::vector<std::string>& params);
void modeHandler(ChannelRegistry& channels, std::map<int, Client>& clients, Client& client, const std::vector<std::string>& params);

#endif
