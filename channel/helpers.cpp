#include "ChannelCommands.hpp"
#include <algorithm>

void replyMsg(const std::string& rcode, const std::string& msg, Client& client)
{
	std::string nickName = client.getNickname();
	client.sendMessage(":irc.Brika.net" + rcode + nickName + " " + msg  + "\r\n");
}

std::string clientPrefix(const Client& client) {
    return ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
}

Client* findClientByNick(std::map<int, Client>& clients, const std::string& nick)
{
	const auto it = std::ranges::find_if(clients, [&nick](auto& entry) {
		return entry.second.getNickname() == nick;
	});
	return it == clients.end() ? nullptr : &it->second;
}