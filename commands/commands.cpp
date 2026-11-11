#include "commands.hpp"
#include "messaging.hpp"
#include "parser.hpp"
#include "../Bot/Bot.hpp"
#include "channel/ChannelCommands.hpp"
#include "../channel/ChannelRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unistd.h>

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

static void authenticateClient(Client& client, std::map<int, Client>& clientBuffers,
    const std::string& serv_pass, const Command& command) {
    if (command.command == "NICK") {
        if (command.params.empty())
            client.sendMessage(":irc.Brika.net 431 * :No nickname given\r\n");
        else
            client.NICK(client.getFd(), clientBuffers, command.params[0]);
    } else if (command.command == "USER") {
        if (command.params.size() != 4)
            client.sendMessage(":irc.Brika.net 461 * USER :Not enough parameters\r\n");
        else
            client.USER(client.getFd(), clientBuffers, command.params[0]);
    } else if (command.command == "PASS") {
        if (command.params.size() != 1)
            client.sendMessage(":irc.Brika.net 461 * PASS :Not enough parameters\r\n");
        else
            client.PASS(client.getFd(), clientBuffers, command.params[0], serv_pass);
    } else {
        client.sendMessage(":irc.Brika.net 451 * :You have not registered\r\n");
    }
}

static bool handleClientNegotiation(Client& client, const Command& command) {
    if (command.command == "PONG") {
        if (!command.params.empty())
            client.acknowledgePing(command.params.back());
        return true;
    }
    if (command.command == "PING") {
        if (command.params.empty()) {
            client.sendMessage(":irc.Brika.net 409 " +
                (client.getNickname().empty() ? "*" : client.getNickname()) +
                " :No origin specified\r\n");
        } else {
            client.sendMessage(":irc.Brika.net PONG irc.Brika.net :" +
                command.params.front() + "\r\n");
        }
        return true;
    }
    if (command.command == "QUIT")
        return false;
    if (command.command != "CAP")
        return false;

    const std::string target = client.getNickname().empty() ? "*" : client.getNickname();
    if (command.params.empty()) {
        client.sendMessage(":irc.Brika.net 461 " + target + " CAP :Not enough parameters\r\n");
        return true;
    }
    std::string subcommand = command.params[0];
    std::ranges::transform(subcommand, subcommand.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (subcommand == "LS")
        client.sendMessage(":irc.Brika.net CAP * LS :\r\n");
    else if (subcommand == "LIST")
        client.sendMessage(":irc.Brika.net CAP * LIST :\r\n");
    else if (subcommand == "REQ") {
        const std::string requested = command.params.size() > 1 ? command.params[1] : "";
        client.sendMessage(":irc.Brika.net CAP * NAK :" + requested + "\r\n");
    } else if (subcommand == "END") {
        return true;
    } else
        client.sendMessage(":irc.Brika.net 410 " + target + " " + command.params[0]
            + " :Invalid CAP subcommand\r\n");
    return true;
}

static void sendWelcome(Client& client) {
    const std::string& nick = client.getNickname();
    const std::string& user = client.getUsername();
    std::string reply;
    reply += ":irc.Brika.net 001 " + nick + " :Welcome to the Internet Relay Network "
        + nick + "!" + user + "@localhost\r\n";
    reply += ":irc.Brika.net 002 " + nick
        + " :Your host is irc.Brika.net, running version 1.0\r\n";
    reply += ":irc.Brika.net 003 " + nick + " :This server was created today\r\n";
    reply += ":irc.Brika.net 004 " + nick + " irc.Brika.net 1.0 o itklo\r\n";
    reply += ":irc.Brika.net 005 " + nick
        + " CHANTYPES=# NICKLEN=30 PREFIX=(o)@ CHANMODES=,k,l,it :are supported by this server\r\n";
    reply += ":irc.Brika.net 422 " + nick + " :MOTD File is missing\r\n";
    client.sendMessage(reply);
    std::cout << "Client " << client.getFd() << " = " << user
        << " authenticated successfully!" << std::endl;
}

static void handleQuit(Client& client, std::map<int, Client>& clientBuffers,
    ChannelRegistry& channels, const Command& command) {
    std::string reason = command.params.empty() ? "Client Quit" : command.params[0];
    std::string quitMessage = ":" + client.getNickname() + "!" + client.getUsername()
        + "@localhost QUIT :" + reason + "\r\n";

    const std::vector<std::string> channelNames = channels.listChannelNames();
    for (const auto& channelName : channelNames) {
        Channel* channel = channels.findChannel(channelName);
        if (!channel || !channel->isMember(client.getFd()))
            continue;
        for (auto& [memberFd, member] : clientBuffers) {
            if (memberFd != client.getFd() && channel->isMember(memberFd))
                member.sendMessage(quitMessage);
        }
        channel->removeMember(client.getFd());
        if (channel->getMemberCount() == 0)
            channels.removeChannel(channelName);
    }

    const int fd = client.getFd();
    if (fd >= 0) {
        ::close(fd);
        clientBuffers.erase(fd);
    }
}

static void handleNames(Client& client, std::map<int, Client>& clientBuffers,
    ChannelRegistry& channels, const std::vector<std::string>& params) {
    (void)clientBuffers;
    if (params.empty()) {
        for (const auto& channelName : channels.listChannelNames()) {
            Channel* channel = channels.findChannel(channelName);
            if (!channel || channel->getMemberCount() == 0)
                continue;
            client.sendMessage(":irc.Brika.net 353 " + client.getNickname() + " = "
                + channel->getName() + " :" + channel->getMembersName() + "\r\n");
            client.sendMessage(":irc.Brika.net 366 " + client.getNickname() + " "
                + channel->getName() + " :End of /NAMES list.\r\n");
        }
        return;
    }
    for (const auto& target : params) {
        Channel* channel = channels.findChannel(target);
        if (!channel) {
            client.sendMessage(":irc.Brika.net 366 " + client.getNickname() + " "
                + target + " :End of /NAMES list.\r\n");
            continue;
        }
        client.sendMessage(":irc.Brika.net 353 " + client.getNickname() + " = "
            + channel->getName() + " :" + channel->getMembersName() + "\r\n");
        client.sendMessage(":irc.Brika.net 366 " + client.getNickname() + " "
            + channel->getName() + " :End of /NAMES list.\r\n");
    }
}

static void handleList(Client& client, ChannelRegistry& channels, const std::vector<std::string>& params) {
    client.sendMessage(":irc.Brika.net 321 " + client.getNickname() + " Channel :Users Name\r\n");
    if (params.empty()) {
        for (const auto& channelName : channels.listChannelNames()) {
            Channel* channel = channels.findChannel(channelName);
            if (!channel)
                continue;
            std::string topic = channel->hasTopic() ? channel->getTopic() : "";
            client.sendMessage(":irc.Brika.net 322 " + client.getNickname() + " "
                + channel->getName() + " " + std::to_string(channel->getMemberCount()) + " :" + topic + "\r\n");
        }
    } else {
        for (const auto& target : params) {
            Channel* channel = channels.findChannel(target);
            if (!channel)
                continue;
            std::string topic = channel->hasTopic() ? channel->getTopic() : "";
            client.sendMessage(":irc.Brika.net 322 " + client.getNickname() + " "
                + channel->getName() + " " + std::to_string(channel->getMemberCount()) + " :" + topic + "\r\n");
        }
    }
    client.sendMessage(":irc.Brika.net 323 " + client.getNickname() + " :End of /LIST\r\n");
}

static void handleWho(Client& client, std::map<int, Client>& clientBuffers,
    ChannelRegistry& channels, const std::vector<std::string>& params) {
    if (params.empty()) {
        for (const auto& [fd, peer] : clientBuffers) {
            (void)fd;
            if (peer.getNickname().empty())
                continue;
            client.sendMessage(":irc.Brika.net 352 " + client.getNickname() + " * " + peer.getUsername()
                + " localhost irc.Brika.net " + peer.getNickname() + " " + (peer.isAway() ? "G" : "H")
                + " :0 " + peer.getNickname() + "\r\n");
        }
        client.sendMessage(":irc.Brika.net 315 " + client.getNickname() + " * :End of /WHO list\r\n");
        return;
    }
    const std::string target = params[0];
    if (target.front() == '#') {
        Channel* channel = channels.findChannel(target);
        if (!channel) {
            client.sendMessage(":irc.Brika.net 315 " + client.getNickname() + " " + target + " :End of /WHO list\r\n");
            return;
        }
        for (const auto& [fd, peer] : clientBuffers) {
            (void)fd;
            if (channel->isMember(peer.getFd())) {
                client.sendMessage(":irc.Brika.net 352 " + client.getNickname() + " " + target + " " + peer.getUsername()
                    + " localhost irc.Brika.net " + peer.getNickname() + " " + (peer.isAway() ? "G" : "H")
                    + " :0 " + peer.getNickname() + "\r\n");
            }
        }
        client.sendMessage(":irc.Brika.net 315 " + client.getNickname() + " " + target + " :End of /WHO list\r\n");
        return;
    }
    for (const auto& [fd, peer] : clientBuffers) {
        (void)fd;
        if (normalizeNickForCompare(peer.getNickname()) == normalizeNickForCompare(target)) {
            client.sendMessage(":irc.Brika.net 352 " + client.getNickname() + " * " + peer.getUsername()
                + " localhost irc.Brika.net " + peer.getNickname() + " " + (peer.isAway() ? "G" : "H")
                + " :0 " + peer.getNickname() + "\r\n");
        }
    }
    client.sendMessage(":irc.Brika.net 315 " + client.getNickname() + " " + target + " :End of /WHO list\r\n");
}

static void handleWhois(Client& client, std::map<int, Client>& clientBuffers,
    const std::vector<std::string>& params) {
    if (params.empty()) {
        client.sendMessage(":irc.Brika.net 461 " + client.getNickname() + " WHOIS :Not enough parameters\r\n");
        return;
    }
    const std::string targetNick = params[0];
    Client* target = nullptr;
    for (auto& [fd, candidate] : clientBuffers) {
        (void)fd;
        if (normalizeNickForCompare(candidate.getNickname()) == normalizeNickForCompare(targetNick)) {
            target = &candidate;
            break;
        }
    }
    if (!target) {
        client.sendMessage(":irc.Brika.net 401 " + client.getNickname() + " " + targetNick + " :No such nick/channel\r\n");
        return;
    }
    client.sendMessage(":irc.Brika.net 311 " + client.getNickname() + " " + target->getNickname()
        + " " + target->getUsername() + " localhost * :" + target->getNickname() + "\r\n");
    if (target->isAway())
        client.sendMessage(":irc.Brika.net 301 " + client.getNickname() + " " + target->getNickname() + " :" + target->getAwayMessage() + "\r\n");
    client.sendMessage(":irc.Brika.net 318 " + client.getNickname() + " " + target->getNickname() + " :End of /WHOIS list\r\n");
}

static void handleAway(Client& client, const std::vector<std::string>& params) {
    if (params.empty()) {
        client.setAwayMessage("");
        client.sendMessage(":irc.Brika.net 305 " + client.getNickname() + " :You are no longer marked as away\r\n");
        return;
    }
    std::string message;
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0)
            message += " ";
        message += params[i];
    }
    client.setAwayMessage(message);
    client.sendMessage(":irc.Brika.net 306 " + client.getNickname() + " :You have been marked as away\r\n");
}

static void handleIson(Client& client, std::map<int, Client>& clientBuffers,
    const std::vector<std::string>& params) {
    std::vector<std::string> online;
    for (const auto& nick : params) {
        for (const auto& [fd, peer] : clientBuffers) {
            (void)fd;
            if (normalizeNickForCompare(peer.getNickname()) == normalizeNickForCompare(nick)) {
                online.push_back(peer.getNickname());
                break;
            }
        }
    }
    std::string response;
    for (std::size_t i = 0; i < online.size(); ++i) {
        if (i > 0)
            response += " ";
        response += online[i];
    }
    client.sendMessage(":irc.Brika.net 303 " + client.getNickname() + " :" + response + "\r\n");
}

static void handleUserhost(Client& client, std::map<int, Client>& clientBuffers,
    const std::vector<std::string>& params) {
    if (params.empty()) {
        client.sendMessage(":irc.Brika.net 461 " + client.getNickname() + " USERHOST :Not enough parameters\r\n");
        return;
    }
    std::string response;
    for (std::size_t i = 0; i < params.size() && i < 5; ++i) {
        const std::string target = params[i];
        for (const auto& [fd, peer] : clientBuffers) {
            (void)fd;
            if (normalizeNickForCompare(peer.getNickname()) == normalizeNickForCompare(target)) {
                if (!response.empty())
                    response += " ";
                response += peer.getNickname() + "=" + peer.getUsername() + "@localhost";
                break;
            }
        }
    }
    client.sendMessage(":irc.Brika.net 302 " + client.getNickname() + " :" + response + "\r\n");
}

static void handleMotd(Client& client, const std::vector<std::string>& params) {
    (void)params;
    client.sendMessage(":irc.Brika.net 422 " + client.getNickname() + " :MOTD File is missing\r\n");
}

static void handleUsers(Client& client, std::map<int, Client>& clientBuffers) {
    std::size_t visibleUsers = 0;
    for (const auto& [fd, peer] : clientBuffers) {
        (void)fd;
        if (!peer.getNickname().empty())
            ++visibleUsers;
    }
    client.sendMessage(":irc.Brika.net 251 " + client.getNickname() + " :There are "
        + std::to_string(visibleUsers) + " users and 0 services on 1 server\r\n");
    client.sendMessage(":irc.Brika.net 255 " + client.getNickname() + " :I have "
        + std::to_string(visibleUsers) + " clients and 0 servers\r\n");
}

static void executeCommands(Client& client, std::map<int, Client>& clientBuffers,
    const Command& command, ChannelRegistry& channels, QuizBot& bot) {
    if (command.command == "PASS" || command.command == "USER") {
        client.sendMessage(":irc.Brika.net 462 " + client.getNickname() + " :You may not reregister\r\n");
        return;
    }
    else if (command.command == "NICK" && command.params.size() == 1) {
        client.NICK(client.getFd(), clientBuffers, command.params[0]);
    }
    else if (command.command == "PRIVMSG") {
        handlePrivmsg(client, clientBuffers, command, channels, bot);
    }
    else if (command.command == "NOTICE") {
        handleNotice(client, clientBuffers, command, channels);
    }
    else if (command.command == "QUIT") {
        handleQuit(client, clientBuffers, channels, command);
    }
    else if (command.command == "JOIN")
        joinHandler(channels, client, command.params);
    else if (command.command == "PART")
        partHandler(channels, client, command.params);
    else if (command.command == "TOPIC")
        topicHandler(channels, client, command.params);
    else if (command.command == "INVITE")
        inviteHandler(channels, client, command.params, clientBuffers);
    else if (command.command == "KICK")
        kickHandler(channels, clientBuffers, client, command.params);
    else if (command.command == "MODE")
        modeHandler(channels, clientBuffers, client, command.params);
    else if (command.command == "NAMES")
        handleNames(client, clientBuffers, channels, command.params);
    else if (command.command == "LIST")
        handleList(client, channels, command.params);
    else if (command.command == "WHO")
        handleWho(client, clientBuffers, channels, command.params);
    else if (command.command == "WHOIS")
        handleWhois(client, clientBuffers, command.params);
    else if (command.command == "AWAY")
        handleAway(client, command.params);
    else if (command.command == "ISON")
        handleIson(client, clientBuffers, command.params);
    else if (command.command == "USERHOST")
        handleUserhost(client, clientBuffers, command.params);
    else if (command.command == "MOTD")
        handleMotd(client, command.params);
    else if (command.command == "LUSERS")
        handleUsers(client, clientBuffers);
    else {
        client.sendMessage(":irc.Brika.net 421 " + (client.getNickname().empty() ? "*" : client.getNickname())
            + " " + command.command + " :Unknown command\r\n");
    }
}

void handleCommand(int fd, std::map<int, Client>& clientBuffers, const std::string& serverPassword,
    const std::string& commandLine, ChannelRegistry& channels, QuizBot& bot) {
    const Command command = parseCommand(commandLine);
    Client& client = clientBuffers[fd];
    client.setFd(fd);
    std::cout << client.isAuthenticated() << "  " << command.command << std::endl;
    if (command.command == "QUIT") {
        handleQuit(client, clientBuffers, channels, command);
        return;
    }
    if (handleClientNegotiation(client, command))
        return;
    if (!client.isAuthenticated()) {
        const bool wasAuthenticated = client.isAuthenticated();
        authenticateClient(client, clientBuffers, serverPassword, command);
        if (!wasAuthenticated && client.isAuthenticated())
            sendWelcome(client);
        return;
    }
    executeCommands(client, clientBuffers, command, channels, bot);
}
