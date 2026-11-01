#include "authentification.hpp"
#include "client.hpp"
#include <algorithm>

Authentification::Authentification() : _authenticated(false) , _password(""), _nickname(""), _username("") {}

Authentification::~Authentification() = default;
//--------------------------------------------------------

std::string Authentification::getPassword() const {
    return _password;
}

void Authentification::setPassword(const std::string &password) {
    _password = password;
}
//--------------------------------------------------------

std::string Authentification::getNickname() const {
    return _nickname;
}

void Authentification::setNickname(const std::string &nickname) {
    _nickname = nickname;
}
//--------------------------------------------------------

std::string Authentification::getUsername() const {
    return _username;
}

void Authentification::setUsername(const std::string &username) {
    _username = username;
}
//--------------------------------------------------------
bool Authentification::isAuthenticated() const {
    return _authenticated;
}

void Authentification::setAuthenticated() {
    if (!_nickname.empty() && !_username.empty() && !_password.empty()) {
        _authenticated = true;
    } else {
        _authenticated = false;
    }
}
// ------------- authentication commands ---------------------

static void sendNumeric(Client& client, const std::string& code, const std::string& target,
    const std::string& params) {
    client.sendMessage(":irc.Brika.net " + code + " " + target + " " + params + "\r\n");
}

static bool isValidNickname(const std::string& nickname) {
    if (nickname.empty() || nickname.size() > 30)
        return false;
    const auto isSpecial = [](unsigned char ch) {
        return ch == '[' || ch == ']' || ch == '\\' || ch == '`'
            || ch == '_' || ch == '^' || ch == '{' || ch == '|'
            || ch == '}';
    };
    const auto isAsciiLetter = [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    };
    const auto isAsciiDigit = [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    };
    const auto first = static_cast<unsigned char>(nickname.front());
    if (!isAsciiLetter(first) && !isSpecial(first))
        return false;
    return std::ranges::all_of(nickname, [&](unsigned char ch) {
        return isAsciiLetter(ch) || isAsciiDigit(ch) || isSpecial(ch) || ch == '-';
    });
}

static unsigned char foldNicknameCharacter(unsigned char ch) {
    if (ch == '[')
        return '{';
    if (ch == ']')
        return '}';
    if (ch == '\\')
        return '|';
    if (ch == '^')
        return '~';
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<unsigned char>(ch - 'A' + 'a');
    return ch;
}

static bool sameNickname(const std::string& left, const std::string& right) {
    return left.size() == right.size()
        && std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
            return foldNicknameCharacter(a) == foldNicknameCharacter(b);
        });
}

void Authentification::NICK(int fd, std::map<int, Client>& clientBuffers, const std::string &nickname) {
    Client& self = clientBuffers[fd];
    const std::string target = self.getNickname().empty() ? "*" : self.getNickname();
    if (!isValidNickname(nickname)) {
        sendNumeric(self, "432", target, nickname + " :Erroneous nickname");
        return;
    }

    const auto duplicate = std::ranges::find_if(clientBuffers, [&](const auto& entry) {
        return entry.first != fd && sameNickname(entry.second.getNickname(), nickname);
    });
    if (duplicate != clientBuffers.end()) {
        sendNumeric(self, "433", target, nickname + " :Nickname is already in use");
        return;
    }

    if (self.isAuthenticated()) {
        const std::string oldPrefix = ":" + self.getNickname() + "!"
            + self.getUsername() + "@localhost";
        setNickname(nickname);
        self.sendMessage(oldPrefix + " NICK :" + nickname + "\r\n");
        return;
    }

    setNickname(nickname);
    setAuthenticated();
}

void Authentification::USER(int fd, std::map<int, Client>& clientBuffers,
    const std::string& username) {
    Client& self = clientBuffers[fd];
    if (self.isAuthenticated()) {
        sendNumeric(self, "462", self.getNickname(), ":You may not reregister");
        return;
    }
    if (username.empty()) {
        sendNumeric(self, "461", "*", "USER :Not enough parameters");
        return;
    }
    setUsername(username);
    setAuthenticated();
}

void Authentification::PASS(int fd, std::map<int, Client>& clientBuffers,
    const std::string& password, const std::string& serv_pass) {
    Client& self = clientBuffers[fd];
    if (self.isAuthenticated()) {
        sendNumeric(self, "462", self.getNickname(), ":You may not reregister");
        return;
    }
    if (password.empty()) {
        sendNumeric(self, "461", "*", "PASS :Not enough parameters");
        return;
    }
    if (password != serv_pass) {
        sendNumeric(self, "464", "*", ":Password incorrect");
        return;
    }
    setPassword(password);
    setAuthenticated();
}
