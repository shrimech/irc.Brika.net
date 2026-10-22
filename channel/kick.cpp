#include "ChannelCommands.hpp"
#include "ChannelRegistry.hpp"

void kickHandler(ChannelRegistry& channels, std::map<int, Client>& clients, Client& client, const std::vector<std::string>& params)
{
	if (params.size() < 2)
		return replyMsg(" 461 ", "KICK :Not enough parameters", client);
	std::string channelName = params[0];
	std::string targetNick = params[1];
	if (channelName.size() < 2 || (channelName[0] != '#'))
		return replyMsg(" 403 ", channelName + " :No such channel", client);
	Channel* channel = channels.findChannel(channelName);
	if (!channel)
		return replyMsg(" 403 ", channelName + " :No such channel", client);
	int fd = client.getFd();
	if (!channel->isMember(fd))
		return replyMsg(" 442 ", channelName + " :You're not on that channel", client);
	if (!channel->isOperator(fd))
		return replyMsg(" 482 ", channelName + " :You're not a channel operator", client);
	Client* target = findClientByNick(clients, targetNick);
	if (!target)
		return replyMsg(" 401 ", targetNick + " :No such nick/channel", client);
	if (!channel->isMember(target->getFd()))
		return replyMsg(" 441 ", targetNick + " " + channelName + " :They aren't on that channel", client);
	std::string comment = params.size() > 2 ? params[2] : client.getNickname();
	std::string msg = clientPrefix(client) + " KICK " + channelName + " " + targetNick + " :" + comment + "\r\n";
	channel->broadCast(msg);
	channel->removeMember(target->getFd());
	if (channel->getMemberCount() == 0)
		channels.removeChannel(channelName);
}
