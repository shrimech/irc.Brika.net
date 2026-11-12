#include "messaging.hpp"
#include "../Bot/Bot.hpp"
#include "../channel/Channel.hpp"
#include "../channel/ChannelRegistry.hpp"
#include "../client/client.hpp"
#include "../commands/channel/ChannelCommands.hpp"
#include <iostream>

static std::string normalizeNickForCompare(const std::string& value) {
    std::string folded;
    folded.reserve(value.size());
    for (unsigned char ch : value) {
        if (ch == '[') folded.push_back('{');
        else if (ch == ']') folded.push_back('}');
        else if (ch == '\\') folded.push_back('|');
        else if (ch == '^') folded.push_back('~');
        else if (ch >= 'A' && ch <= 'Z') folded.push_back(static_cast<char>(ch - 'A' + 'a'));
        else folded.push_back(static_cast<char>(ch));
    }
    return folded;
}

static bool deliverMessage(Client& client, std::map<int, Client>& clients,
    ChannelRegistry& channels, const std::string& recipient, const std::string& message,
    const std::string& verb, bool silentOnFailure) {
    if (recipient.empty())
        return false;

    if (recipient.front() == '#') {
        Channel* channel = channels.findChannel(recipient);
        if (!channel) {
            if (!silentOnFailure)
                client.sendMessage(":irc.Brika.net 403 " + client.getNickname() + " " + recipient + " :No such channel\r\n");
            return false;
        }
        if (!channel->isMember(client.getFd())) {
            if (!silentOnFailure)
                client.sendMessage(":irc.Brika.net 404 " + client.getNickname() + " " + recipient + " :Cannot send to channel\r\n");
            return false;
        }
        const std::string wireMessage = ":" + client.getNickname() + "!"
            + client.getUsername() + "@localhost " + verb + " " + recipient + " :"
            + message + "\r\n";
        channel->broadCast(wireMessage, client.getFd());
        return true;
    }

    const std::string normalizedRecipient = normalizeNickForCompare(recipient);
    for (auto& entry : clients) {
        Client& target = entry.second;
        if (normalizeNickForCompare(target.getNickname()) == normalizedRecipient) {
            const std::string wireMessage = ":" + client.getNickname() + "!"
                + client.getUsername() + "@localhost " + verb + " " + recipient + " :"
                + message + "\r\n";
            target.sendMessage(wireMessage);
            return true;
        }
    }

    if (!silentOnFailure)
        client.sendMessage(":irc.Brika.net 401 " + client.getNickname() + " " + recipient + " :No such nick/channel\r\n");
    return false;
}

void handlePrivmsg(Client& client, std::map<int, Client>& clients, const Command& command,
    ChannelRegistry& channels, QuizBot& bot) {
    if (command.params.size() == 2) {
        const std::string target = command.params[0];
        const std::string normalizedTarget = normalizeNickForCompare(target);
        const std::string botName = normalizeNickForCompare(bot.getName());
        if (normalizedTarget == normalizeNickForCompare("QuizBot")
            || normalizedTarget == normalizeNickForCompare("!bot")
            || normalizedTarget == botName) {
            const std::string botReply = bot.handleCommand(
                client.getFd(), client.getNickname(), command.params[1]);
            client.sendMessage(botReply);
            return;
        }
    }

    if (command.params.empty()) {
        client.sendMessage(":irc.Brika.net 411 " + client.getNickname()
            + " :No recipient given (PRIVMSG)\r\n");
        return;
    }
    if (command.params.size() != 2 || command.params[1].empty()) {
        client.sendMessage(":irc.Brika.net 412 " + client.getNickname()
            + " :No text to send\r\n");
        return;
    }

    deliverMessage(client, clients, channels, command.params[0], command.params[1], "PRIVMSG", false);
}

void handleNotice(Client& client, std::map<int, Client>& clients, const Command& command,
    ChannelRegistry& channels) {
    if (command.params.size() < 2 || command.params[0].empty())
        return;
    if (command.params[1].empty())
        return;
    deliverMessage(client, clients, channels, command.params[0], command.params[1], "NOTICE", true);
}
