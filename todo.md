# IRC Server TODO and Command Roadmap

This file is the updated backlog for the current `irc.Brika.net` project. It reflects the code that is actually present in the repository today and focuses on what still needs to be implemented, improved, or cleaned up.

## 1. Current project status

The project already has a working base for a lightweight IRC server with:

- TCP listener and event loop
- Client authentication (`PASS`, `NICK`, `USER`)
- Basic registration and welcome flow
- `PING` / `PONG` keepalive handling
- Channel management (`JOIN`, `PART`, `TOPIC`, `INVITE`, `KICK`, `MODE`)
- Private messaging (`PRIVMSG`)
- Basic bot interaction (`!help`, `!quiz`, `!answer`)

The current code is a solid starter implementation, but several standard IRC commands and protocol behaviors are still missing or incomplete.

## 2. Command coverage status

| Command / feature | Status | Notes |
| --- | --- | --- |
| `PASS` | Implemented | Registration command accepted and validated. |
| `NICK` | Implemented | Unique nickname handling exists; should be validated more consistently with IRC case-mapping rules. |
| `USER` | Implemented | Registration command exists; real-name and additional user fields are not fully modeled. |
| `PING` | Implemented | Server replies with `PONG` and manages idle-client timeout. |
| `PONG` | Implemented | Validates matching tokens for active pings. |
| `CAP` | Partial | Basic negotiation handlers exist but capabilities are effectively empty / not tracked. |
| `PRIVMSG` | Implemented | Works for direct user and channel messages through the current server logic. |
| `NOTICE` | Not implemented | Missing although standard for automated/system messages. |
| `JOIN` | Implemented | Basic channel join is supported; no multi-channel/key list support yet. |
| `PART` | Implemented | Supports channel departure and cleanup behavior. |
| `TOPIC` | Implemented | Basic topic read/set for channel members. |
| `INVITE` | Implemented | Operator/admin checks exist; invite state should be fully cleaned up on join. |
| `KICK` | Implemented | Operator removal is supported, but edge cases still need tightening. |
| `MODE` | Partial | Basic channel modes exist but validation and supported mode advertisement need cleanup. |
| `QUIT` | Not implemented | No graceful disconnect flow with broadcasted quit notifications. |
| `NAMES` | Not implemented | Needed for standard client discovery of members. |
| `LIST` | Not implemented | Missing channel-directory listing support. |
| `WHO` | Not implemented | Needed for user lookup and channel membership queries. |
| `WHOIS` | Not implemented | Missing user info query support. |
| `AWAY` | Not implemented | Common IRC user-status command. |
| `ISON` | Not implemented | Common client query command. |
| `USERHOST` | Not implemented | Missing user metadata query. |
| `MOTD` | Not implemented | Standard welcome message after registration. |
| `LUSERS` | Not implemented | Common user-count query. |
| `OPER` | Not implemented | Server operator/admin command, out of scope for the current simple server unless planned. |
| `KILL` | Not implemented | Server-admin command, not required for the current project scope. |
| `SERVER` / network commands | Not implemented | Out of scope for a single-server IRC project. |
| Unknown commands | Partial | Logged but no consistent `421` response yet. |
| Bot commands | Implemented | `!help`, `!quiz`, `!answer` exist in the custom bot logic. |

## 3. Missing or incomplete command work

### High-priority IRC commands to add

1. `QUIT`
   - Graceful disconnect behavior
   - Remove client from all channels
   - Broadcast quit notices to channel peers
   - Close socket cleanly

2. `NOTICE`
   - Same delivery rules as `PRIVMSG`
   - No error reply when the notice is invalid or undeliverable
   - Must keep behavior different from `PRIVMSG`

3. `NAMES`
   - Support listing users in a channel or all visible channels
   - Return standard end-of-list responses

4. `LIST`
   - Show channel names, member counts, and topics
   - Respect channel visibility rules

5. `WHO` and `WHOIS`
   - Query connected users and share relevant info
   - Make sure results match actual tracked state only

### Smaller but useful commands

- `AWAY`
- `ISON`
- `USERHOST`
- `MOTD`
- `LUSERS`

These are not always required for a minimal IRC server, but they make client compatibility much better.

## 4. Protocol and reliability improvements

These are the most important improvements needed to make the server robust.

### P0 — Reliability and correctness

- Add a per-client output queue for nonblocking sockets
- Fix direct socket writes so partial writes are retried and ordered safely
- Bound input buffer growth and validate oversized IRC lines
- Centralize numeric reply generation and standard IRC error handling
- Validate channel `MODE` parameters before changing channel state
- Ensure `CAP` replies are truthful and match actual capability support

### P1 — Core command quality

- Implement `QUIT` with cleanup and peer notifications
- Implement `NOTICE` with silent invalid-message behavior
- Complete `NAMES` and `LIST`
- Add `WHO`/`WHOIS`
- Normalize IRC case-insensitive matching for nicknames and channels
- Enforce consistent `421` / `461` / `451` numeric handling

### P2 — Maintainability and deployment

- Add resource limits for clients, channels, and repeated requests
- Improve logging to avoid leaking private data
- Add TLS / secure deployment guidance or documented reverse-proxy requirements
- Improve parser tests and protocol-response tests
- Clean up duplicated logic in registration and command dispatch

## 5. Current code review summary

The repository is not missing the basic server skeleton. It is missing full IRC protocol fidelity and several command behaviors expected by real IRC clients and test suites. In particular:

- The server already handles registration and channel basics
- The biggest gaps are in standard command coverage and protocol response consistency
- A few components already hint at good patterns (`CAP`, `MODE`, `PING`, bot logic), but they still need stronger validation and standardization

## 6. Recommended implementation order

1. Fix server reliability and output buffering
2. Standardize IRC numerics and error replies
3. Implement `QUIT` and `NOTICE`
4. Add `NAMES`, `LIST`, `WHO`, and `WHOIS`
5. Improve case-insensitive nick/channel lookup
6. Tighten `MODE` validation and capability negotiation
7. Add logging, resource caps, and deployment guidance
8. Increase test coverage for parser edge cases and protocol compliance

## 7. Suggested backlog checklist

- [ ] Add client output queue and safe partial write handling
- [ ] Bound input buffer size and validate command framing
- [ ] Centralize IRC numeric formatting
- [ ] Return proper `421` for unknown commands
- [ ] Finish `QUIT` implementation
- [ ] Implement `NOTICE`
- [ ] Implement `NAMES`
- [ ] Implement `LIST`
- [ ] Implement `WHO` and `WHOIS`
- [ ] Improve IRC case mapping for nicknames/channels
- [ ] Validate `MODE` arguments and advertised modes
- [ ] Fix `CAP` negotiation semantics
- [ ] Add connection and resource limits
- [ ] Improve security-safe logging
- [ ] Add deployment guidance for TLS or reverse-proxy setup
- [ ] Expand parser and protocol test coverage

## 8. Important note for project scope

This project is a lightweight IRC server, not a full multi-server daemon. The following should remain intentionally out of scope unless the project later expands:

- server linking / remote commands
- full operator network administration
- advanced SASL/TLS integration as a first milestone
- large-scale IRC network features beyond a single-server local or lab environment

## 9. Final recommendation

The next milestone should not be “add more commands blindly.” The best next step is to finish the protocol basics first:

- reliable socket I/O
- consistent numerics
- correct `QUIT`/`NOTICE`
- standard query commands (`NAMES`, `LIST`, `WHO`, `WHOIS`)
- proper `MODE` and `CAP` validation

Once that is stable, the project will behave much closer to a real IRC server and will be easier to test with standard clients such as `irssi` or `HexChat`.
