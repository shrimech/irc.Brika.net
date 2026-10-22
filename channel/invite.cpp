#include "ChannelCommands.hpp"

void inviteHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params, std::map<int, Client>& clients)
{
	if (params.size() < 2)
		return replyMsg(" 461 ", "INVITE :Not enough parameters", client);
	std::string targetNick = params[0];
	std::string channelName = params[1];
	if (channelName.size() < 2 || (channelName[0] != '#'))
			return replyMsg(" 403 ", channelName + " :No such channel", client);
		Channel* channel = channels.findChannel(channelName);
		if (!channel)
			return replyMsg(" 403 ", channelName + " :No such channel", client);
		int fd = client.getFd();
		if (!channel->isMember(fd))
			return replyMsg(" 442 ", channelName + " :You're not on that channel", client);
		Client* target = findClientByNick(clients, targetNick);
		if (!target)
			return replyMsg(" 401 ", targetNick + " :No such nick/channel", client);
		if (channel->isMember(target->getFd()))
			return replyMsg(" 443 ", targetNick + " " + channelName + " :is already on channel", client);
		if (!channel->isOperator(fd))
			return replyMsg(" 482 ", channelName + " :You're not a channel operator", client);
		channel->addInvite(target->getFd());
		target->sendMessage(clientPrefix(client) + " INVITE " + targetNick + " :" + channelName + "\r\n");
		replyMsg(" 341 ", targetNick + " " + channelName, client);
}
