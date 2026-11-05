#include "ChannelCommands.hpp"
#include "../../channel/Channel.hpp"
#include "../../channel/ChannelRegistry.hpp"
#include <string_view>

static void partChannel(ChannelRegistry& channels, Client& client,
    std::string_view channelName, const std::vector<std::string>& params) {
    const std::string channelTarget(channelName);
    if (channelName.size() < 2 || channelName.front() != '#') {
        replyMsg(" 403 ", channelTarget + " :No such channel", client);
        return;
    }

    Channel* channel = channels.findChannel(channelTarget);
    if (!channel) {
        replyMsg(" 403 ", channelTarget + " :No such channel", client);
        return;
    }

    if (!channel->isMember(client.getFd())) {
        replyMsg(" 442 ", channelTarget + " :You're not on that channel", client);
        return;
    }

    std::string message = clientPrefix(client) + " PART " + channelTarget;
    if (params.size() > 1)
        message += " :" + params[1];
    message += "\r\n";
    channel->broadCast(message);
    channel->removeMember(client.getFd());

    if (channel->getMemberCount() == 0)
        channels.removeChannel(channelTarget);
}

void partHandler(ChannelRegistry& channels, Client& client,
    const std::vector<std::string>& params) {
    if (params.empty() || params.front().empty()) {
        replyMsg(" 461 ", "PART :Not enough parameters", client);
        return;
    }

    const std::string& channelList = params.front();
    std::size_t start = 0;
    while (start <= channelList.size()) {
        const std::size_t end = channelList.find(',', start);
        const std::size_t length = end == std::string::npos
            ? channelList.size() - start
            : end - start;
        partChannel(channels, client,
            std::string_view(channelList).substr(start, length), params);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
}
