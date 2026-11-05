#include "messaging.hpp"
#include "../Bot/Bot.hpp"
#include "../channel/Channel.hpp"
#include "../channel/ChannelRegistry.hpp"
#include "../client/client.hpp"
#include <iostream>

void handlePrivmsg(Client& client, std::map<int, Client>& clients, const Command& command,
    ChannelRegistry& channels, QuizBot& bot) {
    if (command.params.size() == 2) {
        const std::string& target = command.params[0];
        if (target == "QuizBot" || target == "!bot" || target == bot.getName()) {
            const std::string botReply = bot.handleCommand(
                client.getFd(), client.getNickname(), command.params[1]);
            client.sendMessage(botReply);
            std::cout << "[BOT] Replied to " << client.getNickname()
                << ": " << command.params[1] << std::endl;
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

    const std::string& recipient = command.params[0];
    const std::string& message = command.params[1];
    if (recipient.empty()) {
        client.sendMessage(":irc.Brika.net 412 " + client.getNickname()
            + " :No text to send\r\n");
        return;
    }

    if (recipient.front() == '#') {
        Channel* channel = channels.findChannel(recipient);
        if (!channel) {
            client.sendMessage(":irc.Brika.net 403 " + client.getNickname() + " "
                + recipient + " :No such channel\r\n");
            std::cerr << "Channel " << recipient << " does not exist." << std::endl;
            return;
        }
        if (!channel->isMember(client.getFd())) {
            client.sendMessage(":irc.Brika.net 404 " + client.getNickname() + " "
                + recipient + " :Cannot send to channel\r\n");
            std::cerr << "Client " << client.getFd() << " is not a member of channel "
                << recipient << "." << std::endl;
            return;
        }

        const std::string wireMessage = ":" + client.getNickname() + "!"
            + client.getUsername() + "@localhost PRIVMSG " + recipient + " :"
            + message + "\r\n";
        channel->broadCast(wireMessage, client.getFd());
        std::cout << "Message sent to channel " << recipient << ": " << message << std::endl;
        return;
    }

    for (auto& entry : clients) {
        Client& target = entry.second;
        if (target.getNickname() == recipient) {
            const std::string wireMessage = ":" + client.getNickname() + "!"
                + client.getUsername() + "@localhost PRIVMSG " + recipient + " :"
                + message + "\r\n";
            target.sendMessage(wireMessage);
            std::cout << "Message sent to " << recipient << ": " << message << std::endl;
            return;
        }
    }

    client.sendMessage(":irc.Brika.net 401 " + client.getNickname() + " "
        + recipient + " :No such nick/channel\r\n");
    std::cerr << "Recipient " << recipient << " not found." << std::endl;
}
