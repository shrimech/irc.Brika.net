# Development TODO

This is the exhaustive backlog of known gaps found while reviewing every
tracked source file, test, build script, workflow, and user-facing document.
It covers the current single-server project scope; it is not a promise to
implement every command or extension in the full IRC protocol. Items are
ordered by priority, and new gaps found during implementation or review should
be added here.

## Current IRC Command Coverage

| Command or feature | Current status |
| --- | --- |
| `PASS`, `NICK`, `USER` | Registration is implemented; `USER` identity fields beyond username are not retained. |
| `PING` | Responds with `PONG`; server-initiated ping/timeout handling is not implemented. |
| `PONG` | No inbound command handler. |
| `CAP` | Partial `LS`, `LIST`, `REQ`, and `END`; no capabilities are supported or tracked. |
| `PRIVMSG` | One user or channel target; uses inconsistent write paths, and identifier matching is not uniformly IRC-case-insensitive. |
| `JOIN` | One `#` channel per command; no comma-separated channel/key lists or `JOIN 0`. |
| `TOPIC` | Read/set for channel members; topic lengths and some protocol edge cases are not validated. |
| `INVITE` | Operator-only invitation for existing channels; invite tokens are not consumed on join. |
| `KICK` | One target at a time; limited validation and operator succession behavior. |
| `MODE` | Partial channel flags `i`, `t`, `k`, `l`, and `o`; single flag change only. No user modes. |
| `PART` | Implemented for one channel with an optional reason; broadcasts departure and removes empty channels. |
| `QUIT`, `NOTICE` | Not implemented. |
| `NAMES`, `LIST`, `WHO`, `WHOIS` | Not implemented as explicit commands; `JOIN` only emits a basic names list. |
| `AWAY`, `ISON`, `USERHOST` | Not implemented. |
| Unknown commands | Logged locally; no standard `421` response. |
| Server-network/operator commands | Not implemented; server linking and remote administration are outside current scope. |

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

### 5. Implement `QUIT` with complete membership cleanup

**Current gap:** Clients are removed from channels only after a transport
disconnect or explicit `PART`. There is no user-initiated quit, so a client
cannot leave the server gracefully and peers do not receive a quit message.

**Work:** Implement `QUIT` with an optional reason. Broadcast the correct
message to all peers sharing channels, remove memberships and invitations,
close the client's connection, and clean up empty channels. Reuse the
membership cleanup already used by `PART` and transport disconnects. Define
what happens when the last operator leaves a non-empty channel and transfer
operator status if needed.

**Done when:** Clients can quit normally; peers receive the quit message;
membership, invitations, operator state, and empty-channel cleanup remain
correct and are integration-tested.

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

### 17. Handle multiple channel targets and standard JOIN edge cases

**Current gap:** `JOIN` and `KICK` process one channel/target per command.
`JOIN` does not implement `JOIN 0`, and comma-separated channel names and
corresponding keys are not supported.

**Work:** Implement the IRC forms of multi-channel `JOIN`, per-channel keys,
and `JOIN 0` (leave all current channels) using the existing `PART` cleanup.
Validate each
target independently and return protocol replies without partially applying
invalid requests. Decide and document the supported comma-separated `KICK`
behavior or explicitly return a standard error for unsupported forms.

**Done when:** Single-target behavior remains intact; valid lists, key-list
alignment, invalid members in a list, and `JOIN 0` have deterministic
integration tests.

### 18. Complete invitations and channel operator lifecycle

**Current gap:** Joining does not consume an invite, so an invitation can be
reused. The member who becomes channel operator is selected only when the
channel is empty; if all operators leave or are kicked while members remain,
the channel can have no operator. `INVITE` currently requires operator status
even when the channel is not invite-only.

**Work:** Consume an invite when the invited client successfully joins. Align
the operator requirement for `INVITE` with the selected IRC behavior. Ensure
each membership-removal path updates invitations and operator membership;
define how operator status transfers when the last operator leaves.

**Done when:** Invitations cannot be reused after a successful join; operator
state is valid after leave, quit, disconnect, kick, and mode changes; tests
cover invited/non-invited joins and operator succession.

### 19. Enforce channel and topic identifier limits

**Current gap:** Channel validation only checks that a name begins with `#`
and contains at least one following character. Maximum lengths, forbidden
characters, and topic length are not consistently enforced.

**Work:** Define supported channel-name and topic limits, validate names
before creating channels or looking them up, and return the correct numeric
for invalid names. Keep these limits consistent with the server's advertised
`005` values and parser limits.

**Done when:** Invalid and boundary-length names/topics are rejected
consistently by every command and boundary cases are tested.

### 20. Support IRC case mapping consistently for all identifiers

This expands item 9 to cover the complete command surface: private-message
recipients, channel names, invite and kick targets, operator targets, and bot
target aliases must use the same defined comparison rules. Preserve canonical
display spelling and ensure nick changes cannot leave stale references.

**Done when:** A single shared case-mapping implementation is used for every
lookup, and tests cover ASCII case and RFC1459-equivalent special characters
through each relevant command.

### 21. Support user-target lists and make message routing uniform

**Current gap:** `PRIVMSG` accepts exactly one target and its bot, channel,
and user paths use different output mechanisms. User lookup uses exact
nickname spelling and direct `send` calls.

**Work:** Route all outbound data through the client output API from item 1.
Implement comma-separated user/channel target lists if included in the
supported protocol scope, with a documented target-count limit and the
appropriate per-target error behavior. Use shared identifier matching and
preserve trailing message text.

**Done when:** Every recipient path uses the same reliable write mechanism;
multiple targets either work with tested limits or receive an explicit
standard error; one bad target does not corrupt delivery to other targets.

### 22. Track complete registered-user identity and session state

**Current gap:** The server discards `USER`'s real name and other parameters,
does not track sign-on time or away state, and uses a fixed `localhost` host
in message prefixes. This limits correct `WHO`/`WHOIS`, `AWAY`, and client
identity replies.

**Work:** Define which `USER` fields and connection metadata are retained.
Store the real name and sign-on time where needed; derive or deliberately
configure the client host rather than hardcoding it. Add away-state storage
when implementing `AWAY`. Do not expose private or unavailable fields.

**Done when:** Welcome and user-facing prefixes use consistent identity data;
the fields needed by supported query commands are retained, documented, and
tested through registration and nickname changes.

### 23. Implement standard single-server utility commands

**Current gap:** `PONG` is not accepted, and common client utility commands
such as `AWAY`, `ISON`, and `USERHOST` are absent. The server does not
originate `PING` probes or detect idle clients.

**Work:** Implement the commands that fit the single-server scope, using
standard numerics and current client state. If server-originated liveness
probes are added, track outstanding tokens and deadlines and disconnect
clients that do not answer. Explicitly document protocol commands excluded
because they require server linking, operator privileges, or services.

**Done when:** Supported utility commands have correct behavior and tests for
valid, missing, and unknown targets; unsupported network/operator commands
are documented as out of scope rather than silently implied to be available.

### 24. Make server startup, accept, and shutdown behavior robust

**Current gap:** The accept loop handles one queued connection per poll event.
The server is IPv4-only, startup settings are fixed to an all-interface
listener, and shutdown behavior is spread across signal handling and object
destruction. Some socket errors are ignored or reported without their cause.

**Work:** Drain pending accepts until would-block, classify transient and
fatal errors, and ensure descriptor ownership/cleanup is explicit. Define
bind-address and IPv4/IPv6 support requirements. Make signal-driven shutdown
close active connections and release all session state deterministically;
include meaningful system error details in diagnostics.

**Done when:** Bursts of connections are accepted without requiring unrelated
events, shutdown leaves no open descriptors or stale state, and startup,
accept, and shutdown failure cases are integration-tested.

### 25. Make server configuration and secret handling safer

**Current gap:** The bind address and server name are hard-coded, and the
password is provided as a command-line argument, which may be visible to
other local processes. There is no configuration validation beyond the port.

**Work:** Decide the supported configuration interface for bind address,
server name, connection limits, and password. Prefer a protected configuration
file, environment input, or another mechanism that avoids exposing secrets in
process listings; validate missing/empty values and report usage clearly.
Keep the existing command line compatible if changing it would break users.

**Done when:** Configuration options, defaults, and secret handling are
documented; invalid settings fail before opening the listener; tests cover
argument/configuration errors without printing secret values.

### 26. Improve integration-test isolation and reliability

**Current gap:** The test suite starts one long-lived server for all tests,
reserves a port and closes the reservation before server startup, uses
time-based sleeps, and occasionally waits for socket timeouts to infer that
no message arrived.

**Work:** Use deterministic readiness and message-framing helpers, avoid
fixed sleeps where a protocol event or bounded polling can be used, and make
the ephemeral-port allocation race as small as practical. Ensure server
processes and sockets are cleaned up if setup or a test fails. Add focused
parser tests where feasible.

**Done when:** Repeated test runs are stable under load and across CI
distributions; failures identify the test and relevant captured server/client
output; no background process or socket is left behind after a failed test.

### 27. Add sanitizer and static-analysis CI checks

**Current gap:** CI compiles with GCC and Clang across several Linux
distributions, but does not run sanitizers, static analysis, or a compiler
warning profile beyond the Makefile's current flags.

**Work:** Add a focused Linux sanitizer job (for example AddressSanitizer and
UndefinedBehaviorSanitizer) and an appropriate static-analysis or stricter
warning job. Keep these separate from the distribution matrix if needed to
control runtime. Capture reports as CI artifacts and avoid hiding findings
with blanket suppressions.

**Done when:** The test suite runs under the selected sanitizers and
analysis checks, actionable findings fail CI, and tool versions are recorded.

### 28. Document the complete supported protocol and deployment contract

**Current gap:** The README describes features and registration but does not
provide a complete supported-command list, known protocol limitations,
connection limits, or deployment/security requirements. The dependency
installer and distribution CI scope are also not fully explained together.

**Work:** Document implemented commands and mode flags, unsupported
single-server commands, channel and line limits, client registration, server
configuration, build/test commands, supported installation environments,
and the TLS/password security boundary. Keep this documentation synchronized
with actual behavior and welcome `005` capabilities.

**Done when:** A new user can determine which client operations work and how
to deploy safely without relying on undocumented behavior; documented
capabilities are checked against tests.

### 29. Define deployment, packaging, and release support

**Current gap:** The project builds a local executable and tests it in CI but
has no installation target, package format, versioning policy, release
workflow, or documented service-management integration.

**Work:** Decide whether deployment is in scope. If it is, define install
paths, ownership/permissions, configuration location, service lifecycle,
version reporting, and release artifact targets. If not, document that
building from source is the supported deployment model.

**Done when:** The supported deployment approach is explicit and reproducible;
if releases are added, artifacts are traceable to a tested commit and have
documented installation and rollback instructions.

## Explicitly Out of Scope Unless the Project Direction Changes

- Multi-server linking and routing (`SERVER`, `SQUIT`, and related commands).
- IRC operator administration (`OPER`, `KILL`, `DIE`, `REHASH`, and related
  privileged commands).
- Service/server-to-server protocols and IRC extensions not required by the
  single-server client use case.
