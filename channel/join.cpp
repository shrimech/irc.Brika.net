#include "ChannelCommands.hpp"

void joinHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params)
{
	if (params.empty())
		return replyMsg(" 461 ", "JOIN :Not enough parameters", client);
	std::string channelName = params[0];
	if (channelName.size() < 2 || (channelName[0] != '#'))
		return replyMsg(" 403 ", channelName + " :No such channel", client);
	Channel *channel = channels.findOrCreate(channelName);
	int fd = client.getFd();
	if (channel->isMember(fd))
		return;
	if (channel->hasUserLimit() && static_cast<size_t>(channel->getUserLimit()) <= channel->getMemberCount())
		return replyMsg(" 471 ", channelName + " :Cannot join channel (+l)", client);
	if (channel->isInviteOnly() && !channel->isInvited(fd))
		return replyMsg(" 473 ", channelName + " :Cannot join channel (+i)", client);
	if (channel->hasKey())
	{
		std::string key = params.size() > 1 ? params[1] : "";
		if (key != channel->getKey())
			return replyMsg(" 475 ", channelName + " :Cannot join channel (+k)", client);
	}
	if (channel->getMemberCount() == 0)
		channel->addOperator(fd);
	channel->addMember(fd, &client);
	std::string msg = clientPrefix(client) + " JOIN " + channelName + "\r\n";
	channel->broadCast(msg);
	replyMsg(" 353 ", "= " + channelName + " :" + channel->getMembersName(), client);
	replyMsg(" 366 ", channelName + " :End of /Names list.", client);
	if (!channel->hasTopic())
		replyMsg(" 331 ", channelName + " :No topic is set.", client);
	else
		replyMsg(" 332 ", channelName + " :" + channel->getTopic(), client);
}
