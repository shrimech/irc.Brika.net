#ifndef BOT_HPP
#define BOT_HPP

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct Question {
    std::string text;
    std::string options;
    int correctAnswer;
};

class QuizBot {
private:
    std::string _name;
    std::vector<Question> _questions;
    std::map<int, size_t> _userProgress;

void initQuestions() {
        Question q1 = {
            "What is the capital of Morocco?", 
            "1) Casablanca | 2) Rabat | 3) Marrakech", 
            2
        };
        _questions.push_back(q1);

        Question q2 = {
            "Which system call watches multiple file descriptors without blocking?", 
            "1) fork() | 2) poll() | 3) execve()", 
            2
        };
        _questions.push_back(q2);

        Question q3 = {
            "What year was the C++98 standard released?", 
            "1) 1998 | 2) 2003 | 3) 1995", 
            1
        };
        _questions.push_back(q3);

        Question q4 = {
            "Which RFC defines the standard Internet Relay Chat (IRC) client-server protocol used in ft_irc?", 
            "1) RFC 1422 | 2) RFC 2812 | 3) RFC 2616", 
            2
        };
        _questions.push_back(q4);

        Question q5 = {
            "What is the standard transport layer protocol used by IRC connections?", 
            "1) TCP | 2) UDP | 3) ICMP", 
            1
        };
        _questions.push_back(q5);

        Question q6 = {
            "Which of the following keywords is STRICTLY FORBIDDEN in C++98 (and fails 42 grading)?", 
            "1) virtual | 2) explicit | 3) constexpr", 
            3
        };
        _questions.push_back(q6);

        Question q7 = {
            "Which flag used with setsockopt() prevents the 'Address already in use' error after restarting your server?", 
            "1) SO_BROADCAST | 2) SO_REUSEADDR | 3) SO_KEEPALIVE", 
            2
        };
        _questions.push_back(q7);

        Question q8 = {
            "What is the return value of a successful listen() system call in C?", 
            "1) 0 | 2) 1 | 3) The socket file descriptor", 
            1
        };
        _questions.push_back(q8);

        Question q9 = {
            "What is the size of a standard IPv4 address in bytes?", 
            "1) 4 bytes (32 bits) | 2) 8 bytes (64 bits) | 3) 16 bytes (128 bits)", 
            1
        };
        _questions.push_back(q9);

        Question q10 = {
            "Which IRC command is sent by a client to check the latency or see if the server is still alive?", 
            "1) ECHO | 2) PING | 3) STATUS", 
            2
        };
        _questions.push_back(q10);
    }

public:
    QuizBot() : _name("QuizBot") {
        initQuestions();
    }
    ~QuizBot() {}

    const std::string& getName() const { return _name; }

    std::string handleCommand(int client_fd, const std::string& senderNick, const std::string& msg) {
        std::string prefix = ":" + _name + "!bot@ircserv PRIVMSG " + senderNick + " :";

        if (msg == "!help") {
            return prefix + "Commands: !quiz (next question), !answer <1-3> (submit answer)\r\n";
        }
        else if (msg == "!quiz") {
            size_t idx = _userProgress[client_fd];
            if (idx >= _questions.size()) {
                return prefix + "You have completed all trivia questions! Excellent job!\r\n";
            }
            std::string q = prefix + "Q: " + _questions[idx].text + "\r\n";
            std::string opts = prefix + _questions[idx].options + "\r\n";
            return q + opts;
        }
        else if (msg.find("!answer ") == 0) {
            if (_userProgress.find(client_fd) == _userProgress.end() || _userProgress[client_fd] >= _questions.size()) {
                return prefix + "Type !quiz to start a question first!\r\n";
            }

            int guess = std::atoi(msg.substr(8).c_str());
            size_t idx = _userProgress[client_fd];

            if (guess == _questions[idx].correctAnswer) {
                _userProgress[client_fd]++;
                return prefix + "CORRECT! Type !quiz for the next question.\r\n";
            } else {
                return prefix + "Wrong answer! Try again with '!answer <number>'.\r\n";
            }
        }
        return prefix + "Unknown bot command. Type !help for info.\r\n";
    }

    void removeClient(int client_fd) {
        std::map<int, size_t>::iterator it = _userProgress.find(client_fd);
        if (it != _userProgress.end()) {
            _userProgress.erase(it);
            std::cout << "[BOT] Cleaned up progress memory for disconnected FD: " << client_fd << std::endl;
        }
    }
};

#endif

//PRIVMSG !bot :!quiz (next or to bring the quiz(firstly))
//PRIVMSG !bot :!answer <number>
//PRIVMSG !bot :!help