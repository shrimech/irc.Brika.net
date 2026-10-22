#include "ChannelCommands.hpp"
#include "ChannelRegistry.hpp"

void topicHandler(ChannelRegistry& channels, Client& client, const std::vector<std::string>& params)
{
	if (params.empty())
		return replyMsg(" 461 ", "TOPIC :Not enough parameters", client);
	std::string channelName = params[0];
	if (channelName.size() < 2 || (channelName[0] != '#'))
		return replyMsg(" 403 ", channelName + " :No such channel", client);
	Channel* channel = channels.findChannel(channelName);
	if (!channel)
		return replyMsg(" 403 ", channelName + " :No such channel", client);
	int fd = client.getFd();
	if (!channel->isMember(fd))
		return replyMsg(" 442 ", channelName + " :You're not on that channel", client);
	if (params.size() < 2)
	{
		if (!channel->hasTopic())
			return replyMsg(" 331 ", channelName + " :No topic is set.", client);
		else
			return replyMsg(" 332 ", channelName + " :" + channel->getTopic(), client);
	}
	if (channel->isTopicRestricted() && !channel->isOperator(fd))
		return replyMsg(" 482 ", channelName + " :You're not a channel operator", client);
	channel->setTopic(params[1]);
	std::string msg = clientPrefix(client) + " TOPIC " + channelName + " :" + channel->getTopic() + "\r\n";
	channel->broadCast(msg);
}
