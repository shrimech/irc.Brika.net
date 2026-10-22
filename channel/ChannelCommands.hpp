#pragma once

#include "Channel.hpp"
#include "ChannelRegistry.hpp"
#include <vector>

class Client;
class ChannelRegistry;

std::string clientPrefix(Client& client);
void replyMsg(const std::string& rcode, const std::string& msg, Client& client);
Client* findClientByNick(std::map<int, Client>& clients, const std::string& nick);

void joinHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params);
void topicHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params);
void inviteHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params, std::map<int, Client>& clients);
void kickHandler(ChannelRegistry& channels, std::map<int, Client>& clients, Client& client, const std::vector<std::string>& params);
void modeHandler(ChannelRegistry& channels, std::map<int, Client>& clients, Client& client, const std::vector<std::string>& params);
