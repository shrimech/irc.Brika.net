# irc.Brika.net

> **Want to contribute?** Contributions are welcome! Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting issues or pull requests.

A lightweight IRC server built with **C++20**, designed for real-time communication over TCP/IP.

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

- C++ compiler with C++20 support
- `make`
- Python 3 to run the integration tests
- An IRC client such as `irssi`

### Build

```bash
git clone https://github.com/shrimech/irc.Brika.net.git
cd irc.Brika.net
sh scripts/install-dependencies.sh
make
```

### Build targets

- `make` or `make all` builds `ircserv`.
- `make test` builds the server and runs the integration tests.
- `make clean` removes intermediate build files.
- `make fclean` removes intermediate files and the server executable.
- `make re` performs a clean rebuild.

### Start the server

```bash
./ircserv <port> <password>
```

Example:

```bash
./ircserv 6667 mypassword
```

### Client registration

Clients register by sending a valid `PASS`, `NICK`, and `USER` command. The
commands may be sent in any order; registration completes after all three have
been accepted:

```irc
PASS mypassword
NICK mynickname
USER myusername 0 * :My Real Name
```

Nicknames must be 1–30 ASCII characters, begin with a letter or an IRC
nickname-special character (`[]\`_^{}|`), and may contain letters, digits,
hyphens, and those special characters after the first character. Nicknames are
unique using IRC case mapping. The username must be non-empty, and `USER` must
include its four standard parameters.

Irssi can send these registration commands automatically when connecting:

```text
/connect <host> <port> <password>
```

`PASS` is sent over plain TCP. The server does not currently provide TLS or
SASL, so do not use this password over an untrusted network.

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

Run `make test` to build the server and execute the integration test suite. You can also connect multiple IRC clients to test registration, authentication, private messaging, nickname handling, and bot functionality.
