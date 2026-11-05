#include "ChannelCommands.hpp"
#include "../../channel/Channel.hpp"
#include "../../channel/ChannelRegistry.hpp"

void partHandler(ChannelRegistry& channels, Client& client,
    const std::vector<std::string>& params) {
    if (params.empty())
        return replyMsg(" 461 ", "PART :Not enough parameters", client);

    const std::string& channelName = params[0];
    if (channelName.size() < 2 || channelName.front() != '#')
        return replyMsg(" 403 ", channelName + " :No such channel", client);

    Channel* channel = channels.findChannel(channelName);
    if (!channel)
        return replyMsg(" 403 ", channelName + " :No such channel", client);

    if (!channel->isMember(client.getFd()))
        return replyMsg(" 442 ", channelName + " :You're not on that channel", client);

    const std::string reason = params.size() > 1 ? params[1] : client.getNickname();
    const std::string message = clientPrefix(client) + " PART " + channelName
        + " :" + reason + "\r\n";
    channel->broadCast(message);
    channel->removeMember(client.getFd());

    if (channel->getMemberCount() == 0)
        channels.removeChannel(channelName);
}
