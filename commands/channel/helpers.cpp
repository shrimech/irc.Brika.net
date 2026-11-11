#include "ChannelCommands.hpp"
#include <algorithm>

static std::string ircFoldText(const std::string& text) {
    std::string folded;
    folded.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch == '[') folded.push_back('{');
        else if (ch == ']') folded.push_back('}');
        else if (ch == '\\') folded.push_back('|');
        else if (ch == '^') folded.push_back('~');
        else if (ch >= 'A' && ch <= 'Z') folded.push_back(static_cast<char>(ch - 'A' + 'a'));
        else folded.push_back(static_cast<char>(ch));
    }
    return folded;
}

void replyMsg(const std::string& rcode, const std::string& msg, Client& client)
{
	std::string nickName = client.getNickname();
	client.sendMessage(":irc.Brika.net" + rcode + nickName + " " + msg  + "\r\n");
}

std::string clientPrefix(const Client& client) {
    return ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
}

Client* findClientByNick(std::map<int, Client>& clients, const std::string& nick)
{
	const std::string foldedNick = ircFoldText(nick);
	const auto it = std::ranges::find_if(clients, [&foldedNick](auto& entry) {
		return ircFoldText(entry.second.getNickname()) == foldedNick;
	});
	return it == clients.end() ? nullptr : &it->second;
}