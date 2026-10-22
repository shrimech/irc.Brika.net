#include "authentification.hpp"
#include "client.hpp"
#include <iostream>
#include <sys/socket.h>

Authentification::Authentification() : _authenticated(false) , _password(""), _nickname(""), _username("") {}

Authentification::~Authentification() {}
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

typedef std::string (Client::*IdentifierGetter)() const;

static std::string makeUniqueIdentifier(int fd, const std::string& identifier,
    const std::map<int, Client>& clientBuffers, IdentifierGetter getter) {
    std::string candidate = identifier;

    while (true) {
        bool taken = false;
        for (std::map<int, Client>::const_iterator it = clientBuffers.begin();
             it != clientBuffers.end(); ++it) {
            if (it->first != fd && (it->second.*getter)() == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken)
            return candidate;
        candidate += "_";
    }
}

void Authentification::NICK(int fd, std::map<int, Client>& clientBuffers, const std::string &nickname) {
    std::string uniqueNickname = makeUniqueIdentifier(
        fd, nickname, clientBuffers, &Client::getNickname);

    Client& self = clientBuffers[fd];
    std::string oldPrefix = ":" + self.getNickname() + "!" + self.getUsername() + "@localhost";

    setNickname(uniqueNickname);

    if (isAuthenticated()) {
        std::string msg = oldPrefix + " NICK :" + uniqueNickname + "\r\n";
        self.sendMessage(msg);
        return;
    }

    std::string reply = ":irc.Brika.net NOTICE * :Nickname set to " + uniqueNickname + "\r\n";
    self.sendMessage(reply);

    setAuthenticated();
    if (!isAuthenticated()) {
        std::string reply2 = ":irc.Brika.net NOTICE * :Continue with authentication (NICK/USER/PASS)\r\n";
        self.sendMessage(reply2);
    }
}

void Authentification::USER(int fd,std::map<int, Client>& clientBuffers, const std::string &username) {
    std::string uniqueUsername = makeUniqueIdentifier(
        fd, username, clientBuffers, &Client::getUsername);
    setUsername(uniqueUsername);
    if (isAuthenticated()) {
        std::string reply = "you are now known as: " + uniqueUsername + "\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
        return;
    }
    std::string reply = "Username set to: " + uniqueUsername + "\r\n";
    send(fd, reply.c_str(), reply.length(), 0);
    setAuthenticated();
    if(!isAuthenticated()) {
        std::string reply = "Continue with the authentication process NICK/USER/PASS.\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
    }
}

void Authentification::PASS(int fd, const std::string &password, const std::string &serv_pass) {
    if (password == serv_pass) {
        _password = password;
        setAuthenticated();
    }
    if (isAuthenticated() && _password == serv_pass) {
        std::string reply = "Password accepted. You are now authenticated.\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
    }
    else if (!isAuthenticated() && _password == serv_pass) {
        std::string reply = "password Accepted, continue ur authentication process.\r\n";
        send(fd, reply.c_str(), reply.length(), 0);
    } else {
        std::string reply = "464 ERR_PASSWDMISMATCH\r\n";
        std::cout << "User: " << fd << " set an Incorrect password or try to brute force the server." << std::endl;
        send(fd, reply.c_str(), reply.length(), 0);
    }
            
}
