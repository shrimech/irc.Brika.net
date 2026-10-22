#include "ChannelCommands.hpp"

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
	for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); it++)
	{
		if (it->second.getNickname() == nick)
			return &it->second;
	}
	return NULL;
}