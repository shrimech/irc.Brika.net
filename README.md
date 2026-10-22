# irc.Brika.net

A lightweight IRC server built with **C++98**, designed for real-time communication over TCP/IP.

## Overview

**irc.Brika.net** is an Internet Relay Chat (IRC) server providing client connectivity, user registration, authentication, private messaging, and bot interactions through the IRC protocol.

## Features

- TCP/IP client connections
- Password-based authentication
- User registration with `NICK` and `USER`
- Nickname uniqueness
- Private messaging with `PRIVMSG`
- Connection keepalive with `PING` and `PONG`
- Welcome messages
- Custom `QuizBot`

## Getting Started

### Requirements

- C++ compiler with C++98 support
- `make`
- An IRC client such as `irssi`

### Build

```bash
git clone https://github.com/shrimech/ft_irc.git
cd ft_irc
make
```

### Start the server

```bash
./ircserv <port> <password>
```

Example:

```bash
./ircserv 6667 mypassword
```

### Connect

If the server is running locally:

```text
/connect 127.0.0.1 6667 mypassword
```

If `irc.Brika.net` is configured in DNS and the server is reachable externally:

```text
/connect irc.Brika.net 6667 mypassword
```

Replace the port and password with your server's actual configuration.

## QuizBot

The server includes a custom IRC bot named `QuizBot`, with interactions through supported bot commands.

## Testing

Connect multiple IRC clients to test registration, authentication, private messaging, nickname handling, and bot functionality.
