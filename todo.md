# Development TODO

This backlog is based on a review of the current server, command handlers,
channel model, bot, build setup, documentation, and integration tests. Items
are ordered by priority; protocol features should build on the reliability and
correctness work listed first.

## P0 — Reliability and Protocol Correctness

### 1. Queue writes for nonblocking client sockets

**Current gap:** Accepted sockets are nonblocking, but `Client::sendMessage`
attempts to send the entire message immediately and stops on a failed or
would-block send. Several command handlers also call `send` directly, so
partial writes and `EAGAIN` can silently truncate messages and a closed peer
can cause inconsistent error handling.

**Work:** Add a per-client output queue and a write offset. Queue all outbound
IRC messages through one client API. Have the poll loop request `POLLOUT` only
while a client has queued bytes, continue partial writes until the queue is
empty or the socket would block, retry interrupted calls, and close clients
on unrecoverable socket errors. Apply a documented queue-size limit so a
non-reading client cannot consume unbounded memory.

**Done when:** No protocol handler writes directly to a client socket;
messages remain ordered and complete after partial writes; queue limits and
write failures have explicit behavior; tests cover queued output, peer
disconnects, and a client that temporarily does not read.

### 2. Bound and harden input buffering and command parsing

**Current gap:** Client input is appended until a newline arrives with no size
limit. The parser does not enforce IRC line or parameter limits, and malformed
or unusually large input can grow memory without bound or be interpreted
inconsistently.

**Work:** Define supported IRC line-length and parameter limits. Enforce them
while appending/framing input, handle CRLF and multiple commands in one read,
and choose a documented protocol error or disconnect behavior for oversized
lines. Make parsing explicit about missing parameters, trailing parameters
(including empty trailing text), whitespace, and malformed lines.

**Done when:** Input memory stays bounded for clients that never send a line
ending; valid fragmented and batched commands continue to work; malformed and
oversized-line cases have deterministic tested outcomes.

### 3. Use consistent IRC numerics and protocol error replies

**Current gap:** Error responses are assembled independently across command
handlers. Some messages omit the server prefix or numeric formatting, and an
unknown command is only logged instead of receiving an IRC error.

**Work:** Centralize server-prefixed numeric and command-reply formatting.
Audit registration, messaging, and channel handlers for correct recipient
fields and trailing text. Return numeric `421` for unknown commands and
appropriate standard errors for malformed requests without changing successful
command behavior.

**Done when:** Client-visible errors use consistent IRC wire formatting;
unknown, malformed, and unsupported commands receive protocol replies; tests
assert complete server-prefixed response lines.

### 4. Validate channel mode values and advertise only supported modes

**Current gap:** `MODE +l` parses with stream extraction and accepts malformed,
zero, or negative limits. Invalid mode syntax can produce a malformed reply,
and the advertised `004`/`005` mode information does not clearly match the
implemented channel and user modes.

**Work:** Parse mode arguments strictly, require a positive user limit, reject
missing or invalid arguments before changing state, and return standard
numerics for invalid modes. Define supported channel/user modes in one place
and make welcome `004` and `005` values accurately describe them. Ensure
failed multi-argument changes do not leave partial state.

**Done when:** Invalid mode values cannot mutate channel state; valid `+i`,
`+t`, `+k`, `+l`, and `+o` changes and removals are covered; advertised
capabilities match tested behavior.

## P1 — Core IRC User and Channel Commands

### 5. Implement `QUIT` and `PART` with complete membership cleanup

**Current gap:** Clients are removed from channels only after a transport
disconnect. There is no user-initiated quit or channel-part command, so
clients cannot leave normally and channel membership/operator/invitation state
cannot be maintained across those operations.

**Work:** Implement `QUIT` with an optional reason and `PART` with one or more
supported channel targets and an optional reason. Broadcast the correct
message to affected peers, remove memberships and invitations, close/clean
up clients on `QUIT`, and remove empty channels. Define what happens when the
last operator leaves a non-empty channel and transfer operator status if
needed.

**Done when:** Clients can quit or part channels using normal IRC commands;
peers receive the corresponding messages; membership, invitations, operator
state, and empty-channel cleanup remain correct and are integration-tested.

### 6. Implement `NOTICE`

**Current gap:** The server supports `PRIVMSG` only, while IRC clients commonly
use `NOTICE` for automated messages. `NOTICE` must not generate error
replies, unlike `PRIVMSG`.

**Work:** Add user and channel `NOTICE` delivery with the same recipient and
membership rules as supported `PRIVMSG` delivery. Follow IRC behavior by
silently discarding invalid or undeliverable `NOTICE` requests rather than
replying with errors. Decide and document whether bot messages accept `NOTICE`.

**Done when:** Valid notices reach the target with the correct prefix; channel
notices exclude the sender as intended; malformed and unknown-target notices
do not cause numeric error replies.

### 7. Implement channel membership discovery with `NAMES` and `LIST`

**Current gap:** Joining a channel emits a basic names list, but clients
cannot explicitly request `NAMES` or discover channels with `LIST`.

**Work:** Implement `NAMES` for a named channel and for all visible channels,
including operator prefixes and the standard end-of-names response. Implement
`LIST` with channel names, member counts, topics, and the standard list
terminator. Define visibility behavior for invite-only or otherwise restricted
channels and return the correct error for invalid channel arguments.

**Done when:** Irssi and other clients can request names and channel lists;
empty results, multiple channels, and restricted-channel visibility have
defined, tested replies.

### 8. Implement user information queries (`WHO` and `WHOIS`)

**Current gap:** The server has no standard command for clients to inspect
users or determine which channels they share.

**Work:** Add `WHO` and `WHOIS` for connected users, including documented
fields that the server actually tracks. Return the appropriate no-such-user
and end-of-query numerics. Do not invent unavailable host, real-name, away,
operator, or channel data; extend client state only where the protocol needs
it and the server can supply it.

**Done when:** Queries for existing and missing users return complete,
consistent numeric sequences, and results reflect current nicknames and
channel membership.

### 9. Centralize IRC case mapping for nicknames and channels

**Current gap:** Nickname registration checks uniqueness using IRC case
mapping, but private-message, invite, kick, and operator lookups use exact
string comparisons. Channel names are also looked up exactly, which can make
different commands disagree about whether two identifiers refer to the same
target.

**Work:** Implement shared IRC identifier comparison and use it consistently
for nickname lookup and channel lookup, including recipient matching and
membership operations. Keep display spelling unchanged. Advertise the
selected `CASEMAPPING` in `005` if supported by the implementation.

**Done when:** Case and RFC1459-equivalent special characters resolve
consistently across registration, messaging, channel commands, and tests.

### 10. Complete or clearly constrain CAP negotiation

**Current gap:** `CAP LS` and `CAP LIST` return empty responses, `CAP REQ`
always returns `NAK`, and no capability state is tracked. This is adequate
only for clients that request no capabilities.

**Work:** Either implement a small explicit set of useful capabilities with
correct `LS`, `LIST`, `REQ`, `ACK`/`NAK`, and `END` state transitions, or
retain an empty capability set and thoroughly test all supported negotiation
sequences. Never advertise a capability that the server does not implement.

**Done when:** CAP requests and state transitions behave consistently before
and after registration; unknown capabilities are rejected according to the
protocol; Irssi registration works with and without capability requests.

## P2 — Operations, Security, and Maintainability

### 11. Add client and server resource limits

**Current gap:** The server accepts clients without an application-level
connection limit and currently has no rate or resource limits for repeated
commands, authentication attempts, channel creation, or bot state.

**Work:** Define configurable or documented limits for concurrent clients,
pre-registration connections, input/output buffers, channel creation, and
repeated expensive commands. Reject or disconnect over-limit clients with
clear behavior and ensure cleanup releases all per-client and per-channel
state.

**Done when:** Limits are enforced before resource growth becomes unbounded;
limit behavior is deterministic, documented, and covered by tests.

### 12. Make logging useful without exposing private data

**Current gap:** Some command paths log message contents, bot prompts, or
connection identifiers, while other socket failures are silently discarded.
Private message contents should not be written to routine server logs.

**Work:** Establish consistent log levels and structured event context for
startup, shutdown, connection lifecycle, protocol errors, and socket failures.
Do not log server passwords, private message bodies, or bot answers. Preserve
enough error details to diagnose failures without exposing client content.

**Done when:** Logging policy is documented; tests or review checks confirm
that PASS values and private message contents are not logged; operational
errors include actionable context.

### 13. Add TLS support or a documented secure deployment boundary

**Current gap:** Registration sends `PASS` over plaintext TCP. The README warns
about this, but the server has no encrypted listener or secure authentication
mechanism.

**Work:** Decide whether TLS belongs in this server or at a documented
trusted reverse proxy. If implemented in the server, define certificate/key
configuration, handshake failure behavior, client compatibility, and
dependencies. If TLS remains external, document proxy configuration
requirements and ensure the plaintext listener is not exposed outside the
trusted boundary.

**Done when:** The chosen deployment model provides a verifiable encrypted
client-to-server path, has documented setup and failure behavior, and is
tested with an IRC client or an automated TLS integration test.

### 14. Improve parser and protocol test structure

**Current gap:** The test suite is primarily socket-level integration tests;
command parsing edge cases and reusable protocol-line assertions are coupled
to the live server harness.

**Work:** Add focused tests for command parsing and line framing, factor
protocol response assertions into reusable helpers, and cover boundary cases
such as empty trailing parameters, multiple commands per read, maximum line
length, and mixed-case command names. Keep integration tests for end-to-end
registration, delivery, and channel behavior.

**Done when:** Parser/framing failures can be diagnosed independently from
socket integration failures, boundary cases are deterministic, and the full
test suite remains runnable through `make test` and the CI distribution
matrix.

### 15. Align the QuizBot content and lifecycle with the server

**Current gap:** Quiz questions are embedded in `Bot.hpp`, include stale
project/standard references, and the tests cover only a small portion of the
full question/answer lifecycle.

**Work:** Review the wording and answer keys, remove stale project references,
and decide whether questions should remain compiled in or be loaded from
validated data. Test all-question completion, invalid answers, independent
progress for multiple clients, and progress cleanup when clients disconnect.

**Done when:** Quiz content is accurate and maintainable, invalid data cannot
crash startup if externalized, and the full bot lifecycle has automated
coverage.

### 16. Make local dependency installation match documented platforms

**Current gap:** CI tests Ubuntu, Debian, Fedora, and Alpine, but
`scripts/install-dependencies.sh` only supports systems with `apt-get`.

**Work:** Either explicitly document that the helper is Debian/Ubuntu-only
and provide native package commands for Fedora and Alpine, or add safe,
tested package-manager branches for the distributions the project intends to
support. Keep dependency installation separate from building and avoid
unnecessary system changes.

**Done when:** Supported platforms and installation commands are documented,
the helper reports unsupported systems clearly, and its behavior is tested
without requiring contributors to guess package names.
