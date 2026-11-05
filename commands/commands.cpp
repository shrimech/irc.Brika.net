#include "commands.hpp"
#include "messaging.hpp"
#include "parser.hpp"
#include "../Bot/Bot.hpp"
#include "channel/ChannelCommands.hpp"
#include "../channel/ChannelRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

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

static void executeCommands(Client& client, std::map<int, Client>& clientBuffers,
    const Command& command, ChannelRegistry& channels, QuizBot& bot);

void handleCommand(int fd, std::map<int, Client>& clientBuffers, const std::string& serverPassword,
    const std::string& commandLine, ChannelRegistry& channels, QuizBot& bot) {
    const Command command = parseCommand(commandLine);
    Client& client = clientBuffers[fd];
    client.setFd(fd);
    std::cout << client.isAuthenticated() << "  " << command.command << std::endl;
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

static void executeCommands(Client& client, std::map<int, Client>& clientBuffers,
    const Command& command, ChannelRegistry& channels, QuizBot& bot) {
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
        handlePrivmsg(client, clientBuffers, command, channels, bot);
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
	else {
        std::cerr << "Unknown command: " << command.command << std::endl;
    }
}
