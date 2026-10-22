#ifndef AUTHENTIFICATION_HPP
#define AUTHENTIFICATION_HPP

#include <map>
#include <string>

class Client;
class Authentification
{
protected:
    bool        _authenticated;
    std::string _password;
    std::string _nickname;
    std::string _username;

public:
    Authentification();
    virtual ~Authentification();
    
    std::string getPassword() const;
    void setPassword(const std::string &password);
    
    std::string getNickname() const;
    void setNickname(const std::string &nickname);
    
    std::string getUsername() const;
    void setUsername(const std::string &username);

    bool isAuthenticated() const;
    void setAuthenticated();

    void NICK(int fd, std::map<int, Client>& clientBuffers, const std::string& nickname);
    void USER(int fd, std::map<int, Client>& clientBuffers, const std::string& username);
    void PASS(int fd, const std::string &password, const std::string &serv_pass);

};


#endif