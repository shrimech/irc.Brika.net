import os
import socket
import subprocess
import time
import unittest


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER = os.path.join(ROOT, "ircserv")
PASSWORD = "ci-test-password"


class IrcServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["make", "re", "-j2"], cwd=ROOT, check=True)

        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        cls.port = probe.getsockname()[1]
        probe.close()

        cls.server = subprocess.Popen(
            [SERVER, str(cls.port), PASSWORD],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        cls.readiness_socket = None
        deadline = time.time() + 5
        while time.time() < deadline:
            if cls.server.poll() is not None:
                raise RuntimeError("IRC server exited during startup")
            try:
                cls.readiness_socket = socket.create_connection(
                    ("127.0.0.1", cls.port), timeout=0.2
                )
                break
            except OSError:
                time.sleep(0.05)
        if cls.readiness_socket is None:
            cls.server.terminate()
            cls.server.wait(timeout=3)
            raise RuntimeError("IRC server did not start listening")

    @classmethod
    def tearDownClass(cls):
        if cls.readiness_socket is not None:
            cls.readiness_socket.close()
        cls.server.terminate()
        cls.server.wait(timeout=3)

    def setUp(self):
        self.clients = []

    def tearDown(self):
        for client in self.clients:
            client.close()

    def connect_client(self):
        client = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        client.settimeout(2)
        self.clients.append(client)
        return client

    @staticmethod
    def receive_until(client, expected):
        received = b""
        deadline = time.time() + 3
        while expected not in received and time.time() < deadline:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            received += chunk
        return received

    def register(self, nickname):
        client = self.connect_client()
        client.sendall(
            ("PASS " + PASSWORD + "\r\n").encode()
            + ("NICK " + nickname + "\r\n").encode()
            + ("USER " + nickname + " 0 * :CI Test\r\n").encode()
        )
        welcome = self.receive_until(client, b" 001 " + nickname.encode() + b" ")
        self.assertIn(b" 001 " + nickname.encode() + b" ", welcome)
        return client

    def assert_protocol_case(self, index, setup, command, expected, register_target):
        channel = "#case" + str(index)
        nickname = "caseuser" + str(index)
        owner = None

        if setup in ("channel", "nonoperator", "nonoperator_target"):
            owner = self.register("caseowner" + str(index))
            owner.sendall(("JOIN " + channel + "\r\n").encode())
            self.receive_until(owner, (" 366 caseowner" + str(index) + " " + channel + " ").encode())

        client = self.register(nickname)
        if setup in ("operator", "nonoperator", "nonoperator_target", "operator_target"):
            if owner is None:
                client.sendall(("JOIN " + channel + "\r\n").encode())
                self.receive_until(client, (" 366 " + nickname + " " + channel + " ").encode())
            else:
                client.sendall(("JOIN " + channel + "\r\n").encode())
                self.receive_until(client, (" 366 " + nickname + " " + channel + " ").encode())

        target_nick = "casetarget" + str(index)
        if register_target:
            target = self.register(target_nick)
            if setup == "nonoperator_target":
                target.sendall(("JOIN " + channel + "\r\n").encode())
                self.receive_until(target, (" 366 " + target_nick + " " + channel + " ").encode())

        line = command.format(
            channel=channel,
            nick=nickname,
            target=target_nick,
        ) + "\r\n"
        client.sendall(line.encode())
        expected_reply = expected.format(
            channel=channel,
            nick=nickname,
            target=target_nick,
        ).encode()
        reply = self.receive_until(client, expected_reply)
        self.assertIn(expected_reply, reply)

    def test_registers_client(self):
        client = self.register("registertest")
        self.assertIsNotNone(client)

    def test_rejects_incorrect_password(self):
        client = self.connect_client()
        client.sendall(b"PASS incorrect-password\r\n")
        reply = self.receive_until(client, b"464 ERR_PASSWDMISMATCH")
        self.assertIn(b"464 ERR_PASSWDMISMATCH", reply)

    def test_broadcasts_channel_message_to_members(self):
        sender = self.register("sender")
        receiver = self.register("receiver")

        sender.sendall(b"JOIN #ci-room\r\n")
        sender_join = self.receive_until(sender, b" 366 sender #ci-room ")
        self.assertIn(b" 353 sender = #ci-room ", sender_join)

        receiver.sendall(b"JOIN #ci-room\r\n")
        receiver_join = self.receive_until(receiver, b" 366 receiver #ci-room ")
        self.assertIn(b" 353 receiver = #ci-room ", receiver_join)

        sender.sendall(b"PRIVMSG #ci-room :hello from CI\r\n")
        message = self.receive_until(receiver, b"PRIVMSG #ci-room :hello from CI")
        self.assertIn(b":sender!sender@localhost PRIVMSG #ci-room :hello from CI", message)

    def test_sends_private_message_to_nickname(self):
        sender = self.register("private_sender")
        receiver = self.register("private_receiver")

        sender.sendall(b"PRIVMSG private_receiver :private hello\r\n")
        message = self.receive_until(receiver, b"PRIVMSG private_receiver :private hello")
        self.assertIn(
            b":private_sender!private_sender@localhost PRIVMSG private_receiver :private hello",
            message,
        )

    def test_rejects_channel_message_from_non_member(self):
        member = self.register("channel_member")
        outsider = self.register("channel_outsider")

        member.sendall(b"JOIN #restricted-room\r\n")
        joined = self.receive_until(member, b" 366 channel_member #restricted-room ")
        self.assertIn(b" 353 channel_member = #restricted-room ", joined)

        outsider.sendall(b"PRIVMSG #restricted-room :not allowed\r\n")
        reply = self.receive_until(
            outsider, b"404 channel_outsider #restricted-room "
        )
        self.assertIn(
            b"404 channel_outsider #restricted-room :Cannot send to channel",
            reply,
        )

    def test_channel_operator_sets_and_reads_topic(self):
        client = self.register("topic_operator")
        client.sendall(b"JOIN #topic-room\r\n")
        joined = self.receive_until(client, b" 366 topic_operator #topic-room ")
        self.assertIn(b" 353 topic_operator = #topic-room ", joined)

        client.sendall(b"TOPIC #topic-room :CI topic text\r\n")
        update = self.receive_until(
            client, b" TOPIC #topic-room :CI topic text"
        )
        self.assertIn(
            b":topic_operator!topic_operator@localhost TOPIC #topic-room :CI topic text",
            update,
        )

        client.sendall(b"TOPIC #topic-room\r\n")
        reply = self.receive_until(
            client, b" 332 topic_operator #topic-room :CI topic text"
        )
        self.assertIn(
            b" 332 topic_operator #topic-room :CI topic text",
            reply,
        )

    def test_quiz_bot_help(self):
        client = self.register("bot_user")
        client.sendall(b"PRIVMSG QuizBot :!help\r\n")
        reply = self.receive_until(client, b"Commands: !quiz")
        self.assertIn(b"Commands: !quiz (next question)", reply)


PROTOCOL_CASES = [
    ("join_requires_channel", "none", "JOIN", " 461 {nick} JOIN", False),
    ("join_rejects_invalid_channel", "none", "JOIN invalid", " 403 {nick} invalid", False),
    ("topic_requires_channel", "none", "TOPIC", " 461 {nick} TOPIC", False),
    ("topic_requires_existing_channel", "none", "TOPIC #missing", " 403 {nick} #missing", False),
    ("topic_requires_membership_to_read", "channel", "TOPIC {channel}", " 442 {nick} {channel}", False),
    ("topic_requires_membership_to_set", "channel", "TOPIC {channel} :new topic", " 442 {nick} {channel}", False),
    ("invite_requires_parameters", "none", "INVITE", " 461 {nick} INVITE", False),
    ("invite_rejects_invalid_channel", "none", "INVITE {target} invalid", " 403 {nick} invalid", True),
    ("invite_requires_existing_channel", "none", "INVITE {target} #missing", " 403 {nick} #missing", True),
    ("invite_requires_membership", "channel", "INVITE {target} {channel}", " 442 {nick} {channel}", True),
    ("invite_rejects_unknown_nickname", "operator", "INVITE {target} {channel}", " 401 {nick} {target}", False),
    ("invite_requires_operator", "nonoperator", "INVITE {target} {channel}", " 482 {nick} {channel}", True),
    ("kick_requires_parameters", "none", "KICK", " 461 {nick} KICK", False),
    ("kick_rejects_invalid_channel", "none", "KICK invalid {target}", " 403 {nick} invalid", True),
    ("kick_requires_existing_channel", "none", "KICK #missing {target}", " 403 {nick} #missing", True),
    ("kick_requires_membership", "channel", "KICK {channel} {target}", " 442 {nick} {channel}", True),
    ("kick_requires_operator", "nonoperator", "KICK {channel} {target}", " 482 {nick} {channel}", True),
    ("kick_rejects_unknown_nickname", "operator", "KICK {channel} {target}", " 401 {nick} {target}", False),
    ("kick_requires_target_membership", "operator", "KICK {channel} {target}", " 441 {nick} {target} {channel}", True),
    ("mode_requires_parameters", "none", "MODE", " 461 {nick} MODE", False),
    ("mode_requires_existing_channel", "none", "MODE #missing", " 403 {nick} #missing", False),
    ("mode_requires_membership", "channel", "MODE {channel}", " 442 {nick} {channel}", False),
    ("mode_rejects_multiple_flags", "operator", "MODE {channel} +it", "multiple mode flags are not supported", False),
    ("mode_rejects_unknown_flag", "operator", "MODE {channel} +z", " 472 {nick} z ", False),
    ("mode_key_requires_value", "operator", "MODE {channel} +k", " 461 {nick} MODE", False),
    ("privmsg_requires_recipient", "none", "PRIVMSG", "411 {nick} :No recipient given", False),
    ("privmsg_requires_text", "none", "PRIVMSG {target}", "412 {nick} :No text to send", False),
    ("privmsg_rejects_unknown_nickname", "none", "PRIVMSG {target} :hello", "401 {nick} {target}", False),
]


def make_protocol_test(index, case):
    def test(self):
        setup, command, expected, register_target = case[1:]
        self.assert_protocol_case(index, setup, command, expected, register_target)

    test.__name__ = "test_" + case[0]
    return test


for case_index, protocol_case in enumerate(PROTOCOL_CASES):
    setattr(
        IrcServerTests,
        "test_" + protocol_case[0],
        make_protocol_test(case_index, protocol_case),
    )


if __name__ == "__main__":
    unittest.main()
