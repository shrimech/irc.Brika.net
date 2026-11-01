#include "commands.hpp"
#include "../Bot/Bot.hpp"
#include "../channel/ChannelCommands.hpp"
#include "../channel/Channel.hpp"
#include "../channel/ChannelRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sys/socket.h>

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
        client.sendMessage(":irc.Brika.net CAP " + target + " LS :\r\n");
    else if (subcommand == "LIST")
        client.sendMessage(":irc.Brika.net CAP " + target + " LIST :\r\n");
    else if (subcommand == "REQ") {
        const std::string requested = command.params.size() > 1 ? command.params[1] : "";
        client.sendMessage(":irc.Brika.net CAP " + target + " NAK :" + requested + "\r\n");
    } else if (subcommand != "END")
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

void HandleCommand(int fd, std::map<int, Client>& clientBuffers, const std::string& serv_pass, const std::string& commandLine, ChannelRegistry& channels, QuizBot& bot) {
    const Command command = parseCommand(commandLine);
    Client& client = clientBuffers[fd];
    client.setFd(fd);
    std::cout << client.isAuthenticated() << "  " << command.command << std::endl;
    if (handleClientNegotiation(client, command))
        return;
    if (!client.isAuthenticated()) {
        const bool wasAuthenticated = client.isAuthenticated();
        authenticateClient(client, clientBuffers, serv_pass, command);
        if (!wasAuthenticated && client.isAuthenticated())
            sendWelcome(client);
        return;
    }
    executeCommands(client, clientBuffers, command, channels, bot);
}

Command parseCommand(const std::string& commandLine) {
    Command command;
    size_t start = commandLine.find_first_not_of(" \t");
    size_t end = commandLine.find_first_of(" \t", start);

    if (start != std::string::npos) {
        command.command = commandLine.substr(start, end - start);
        std::ranges::transform(command.command, command.command.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        start = commandLine.find_first_not_of(" \t", end);

        while (start != std::string::npos) {
            if (commandLine[start] == ':') {
                end = commandLine.length();
                start++;
            }
            else {
                end = commandLine.find_first_of(" \t", start);
            }
            command.params.push_back(commandLine.substr(start, end - start));
            start = commandLine.find_first_not_of(" \t", end);
        }
    }
    return command;
}

void PRIVMSG(int fd, const std::map<int, Client>& clientBuffers, Client& client, const Command& command, ChannelRegistry& channels, QuizBot& bot) {
    
    // hadchi ana li zedto bash itparse l Bot
    if (command.params.size() == 2) {
        const std::string& target = command.params[0];
        
        if (target == "QuizBot" || target == "!bot" || target == bot.getName()) {
            std::string botMsg = command.params[1];
            std::string botReply = bot.handleCommand(fd, client.getNickname(), botMsg);
            send(fd, botReply.c_str(), botReply.length(), 0);
            std::cout << "[BOT] Replied to " << client.getNickname() << ": " << botMsg << std::endl;
            return;
        }
    }

    if (command.params.empty()) {
        std::string reply = "411 " + client.getNickname() + " :No recipient given (PRIVMSG)\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
        return;
    }
    if (command.params.size() != 2) {
        std::string reply = "412 " + client.getNickname() + " :No text to send\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
        return;
    }

    const std::string& recipient = command.params[0];
    const std::string& message = command.params[1];

    if (recipient.empty() || message.empty()) {
        std::string reply = "412 " + client.getNickname() + " :No text to send\r\n";
        send(fd,reply.c_str(),reply.length(),0);
        std::cerr << "PRIVMSG <recipient> <message>." << std::endl;
        return;
    }

    if (recipient[0] == '#') {
        Channel* channel = channels.findChannel(recipient);
        if (!channel) {
            std::string reply = "403 " + client.getNickname() + " " + recipient + " :No such channel\r\n";
            send(fd, reply.c_str(), reply.length(), 0);
            std::cerr << "Channel " << recipient << " does not exist." << std::endl;
            return;
        }
        if (!channel->isMember(client.getFd())) {
            std::string reply = "404 " + client.getNickname() + " " + recipient + " :Cannot send to channel\r\n";
            send(fd, reply.c_str(), reply.length(), 0);
            std::cerr << "Client " << client.getFd() << " is not a member of channel " << recipient << "." << std::endl;
            return;
        }
        std::string fullMessage = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PRIVMSG " + recipient + " :" + message + "\r\n";
        channel->broadCast(fullMessage, client.getFd());
        std::cout << "Message sent to channel " << recipient << ": " << message << std::endl;
        return;
    }


    // Find the recipient in the clientBuffers
    for (std::map<int, Client>::const_iterator pair = clientBuffers.begin(); pair != clientBuffers.end(); ++pair) {
        if(pair->second.getNickname() == recipient) {
            // Recipient found, send the message
            std::string fullMessage = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PRIVMSG " + recipient + " :" + message + "\r\n";
            send(pair->second.getFd(), fullMessage.c_str(), fullMessage.length(), 0);
            std::cout << "Message sent to " << recipient << ": " << message << std::endl;
            return;
        }
    }

    // If recipient not found
    std::string errorReply = "401 " + client.getNickname() + " " + recipient + " :No such nick/channel\r\n";
    send(fd, errorReply.c_str(), errorReply.length(), 0);
    std::cerr << "Recipient " << recipient << " not found." << std::endl;
}


void executeCommands(Client& client, std::map<int, Client>& clientBuffers, const Command& command, ChannelRegistry& channels, QuizBot& bot) {
    if(command.command == "PASS"|| command.command == "USER")
    {
        client.sendMessage(":irc.Brika.net 462 " + client.getNickname()
            + " :You may not reregister\r\n");
        return;
    }
    else if (command.command == "NICK" && command.params.size() == 1) {
        client.NICK(client.getFd(),clientBuffers, command.params[0]);
    }
    else if (command.command == "PRIVMSG") {
        PRIVMSG(client.getFd(),clientBuffers,client, command, channels, bot);
    }
	else if (command.command == "JOIN")
		joinHandler(channels, client, command.params);
	else if (command.command == "TOPIC")
		topicHandler(channels, client, command.params);
	else if (command.command == "INVITE")
		inviteHandler(channels, client, command.params, clientBuffers);
	else if (command.command == "KICK")
		kickHandler(channels, clientBuffers, client, command.params);
	else if (command.command == "MODE")
		modeHandler(channels, clientBuffers, client, command.params);
	else {
        std::cerr << "Unknown command: " << command.command << std::endl;
    }
}
