#include "ChannelCommands.hpp"
#include "ChannelRegistry.hpp"
#include <sstream>

static std::string currentModeString(Channel* channel, bool isOp)
{
	std::string modes = "+";
	std::string params;

	if (channel->isInviteOnly())
		modes += "i";
	if (channel->isTopicRestricted())
		modes += "t";
	if (channel->hasKey()) {
		modes += "k";
		if (isOp)
			params += " " + channel->getKey();
	}
	if (channel->hasUserLimit()) {
		std::ostringstream oss;
		oss << channel->getUserLimit();
		modes += "l";
		params += " " + oss.str();
	}
	return modes + params;
}

void modeHandler(ChannelRegistry& channels,
	std::map<int, Client>& clients,
	Client& client,
	const std::vector<std::string>& params)
{
	if (params.empty())
		return replyMsg(" 461 ", "MODE :Not enough parameters", client);

	std::string channelName = params[0];

	if (channelName.size() < 2 || channelName[0] != '#')
		return;

	Channel* channel = channels.findChannel(channelName);
	if (!channel)
		return replyMsg(" 403 ", channelName + " :No such channel", client);

	int fd = client.getFd();

	if (!channel->isMember(fd))
		return replyMsg(" 442 ", channelName + " :You're not on that channel", client);

	if (params.size() < 2)
		return replyMsg(" 324 ", channelName + " " + currentModeString(channel, channel->isOperator(fd)), client);

	if (!channel->isOperator(fd))
		return replyMsg(" 482 ", channelName + " :You're not a channel operator", client);

	std::string modeString = params[1];

	if (modeString.size() != 2 || (modeString[0] != '+' && modeString[0] != '-'))
		return replyMsg("", modeString + " :multiple mode flags are not supported " + channelName, client);

	bool adding = modeString[0] == '+';
	char c = modeString[1];

	switch (c) {
		case 'i':
			channel->setInvitedOnly(adding);
			break;

		case 't':
			channel->setTopicRestricted(adding);
			break;

		case 'k':
			if (adding) {
				if (params.size() < 3)
					return replyMsg(" 461 ", "MODE :Not enough parameters", client);

				channel->setKey(params[2]);
			}
			else
				channel->removeKey();
			break;

		case 'l':
			if (adding) {
				if (params.size() < 3)
					return replyMsg(" 461 ", "MODE :Not enough parameters", client);
				std::istringstream iss(params[2]);
				int limit = 0;
				iss >> limit;
				channel->setUserLimit(limit);
			}
			else
				channel->removeUserLimit();
			break;

		case 'o': {
			if (params.size() < 3)
				return replyMsg(" 461 ", "MODE :Not enough parameters", client);
			Client* target = findClientByNick(clients, params[2]);
			if (!target || !channel->isMember(target->getFd()))
				return replyMsg(" 441 ", params[2] + " " + channelName + " :They aren't on that channel", client);
			if (adding)
				channel->addOperator(target->getFd());
			else
				channel->removeOperator(target->getFd());
			break;
		}
		default:
			return replyMsg(" 472 ", std::string(1, c) + " :is unknown mode char to me for " + channelName, client);
	}
	std::string msg = clientPrefix(client) + " MODE " + channelName + " " + modeString;
	if (c == 'k' && adding)
		msg += " " + params[2];
	else if (c == 'l' && adding)
		msg += " " + params[2];
	else if (c == 'o')
		msg += " " + params[2];
	msg += "\r\n";
	channel->broadCast(msg);
}
