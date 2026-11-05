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
        self.received = {}

    def tearDown(self):
        for client in self.clients:
            client.close()

    def connect_client(self):
        client = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        client.settimeout(0.5)
        self.clients.append(client)
        self.received[client] = b""
        return client

    def receive_until(self, client, expected):
        received = self.received[client]
        deadline = time.time() + 3
        while expected not in received and time.time() < deadline:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            received += chunk
        if expected in received:
            marker_end = received.index(expected) + len(expected)
            self.received[client] = received[marker_end:]
            return received
        self.received[client] = b""
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

    def join_channel(self, client, nickname, channel):
        client.sendall(("JOIN " + channel + "\r\n").encode())
        reply = self.receive_until(
            client, (" 366 " + nickname + " " + channel + " ").encode()
        )
        self.assertIn((" 366 " + nickname + " " + channel + " ").encode(), reply)
        reply += self.receive_until(
            client, (" 331 " + nickname + " " + channel + " ").encode()
        )
        return reply

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

    def test_registration_supports_capability_negotiation_and_standard_welcome(self):
        client = self.connect_client()
        client.sendall(
            b"CAP LS 302\r\n"
            b"PASS ci-test-password\r\n"
            b"USER irssi_user 0 * :Irssi Test User\r\n"
            b"NICK irssi_user\r\n"
            b"CAP END\r\n"
        )
        response = self.receive_until(client, b" 422 irssi_user ")
        self.assertIn(b":irc.Brika.net CAP * LS :", response)
        self.assertIn(b" 001 irssi_user :Welcome", response)
        self.assertIn(b" 004 irssi_user irc.Brika.net 1.0 o itklo", response)
        self.assertIn(b" 005 irssi_user CHANTYPES=# NICKLEN=30", response)
        self.assertIn(b" 422 irssi_user :MOTD File is missing", response)
        self.assertEqual(response.count(b" 001 irssi_user "), 1)

    def test_registration_can_arrive_in_any_order_and_waits_for_all_fields(self):
        client = self.connect_client()
        client.sendall(b"NICK order_test\r\nUSER order_user 0 * :CI Test\r\n")
        time.sleep(0.05)
        client.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            client.recv(4096)

        client.settimeout(2)
        client.sendall(b"PASS ci-test-password\r\n")
        welcome = self.receive_until(client, b" 422 order_test ")
        self.assertIn(b" 001 order_test ", welcome)
        self.assertEqual(welcome.count(b" 001 order_test "), 1)

    def test_pass_requires_a_password_argument(self):
        client = self.connect_client()
        client.sendall(b"PASS\r\n")
        reply = self.receive_until(client, b" 461 * PASS ")
        self.assertIn(b":irc.Brika.net 461 * PASS :Not enough parameters", reply)
        self.assertNotIn(b" 001 ", reply)

    def test_nick_requires_a_nickname_argument(self):
        client = self.connect_client()
        client.sendall(b"NICK\r\n")
        reply = self.receive_until(client, b" 431 * ")
        self.assertIn(b":irc.Brika.net 431 * :No nickname given", reply)

    def test_rejects_all_invalid_nickname_character_positions(self):
        client = self.connect_client()
        client.sendall(
            b"PASS ci-test-password\r\n"
            b"NICK -leading\r\n"
            b"NICK bad!nick\r\n"
            b"NICK bad.nick\r\n"
            b"NICK bad~nick\r\n"
        )
        reply = self.receive_until(client, b" 432 * bad~nick ")
        self.assertIn(b" 432 * -leading :Erroneous nickname", reply)
        self.assertIn(b" 432 * bad!nick :Erroneous nickname", reply)
        self.assertIn(b" 432 * bad.nick :Erroneous nickname", reply)
        self.assertIn(b" 432 * bad~nick :Erroneous nickname", reply)
        self.assertNotIn(b" 001 ", reply)

    def test_accepts_allowed_irc_nickname_special_characters(self):
        client = self.connect_client()
        nickname = "[User]^_{}|\\`-9"
        client.sendall(
            b"PASS ci-test-password\r\n"
            + ("NICK " + nickname + "\r\n").encode()
            + b"USER allowed_user 0 * :CI Test\r\n"
        )
        welcome = self.receive_until(client, (" 001 " + nickname + " ").encode())
        self.assertIn((" 001 " + nickname + " ").encode(), welcome)

    def test_nickname_special_characters_follow_irc_case_mapping(self):
        self.register("[mapping]")
        client = self.connect_client()
        client.sendall(
            b"PASS ci-test-password\r\n"
            b"NICK {MAPPING}\r\n"
            b"USER mapping_user 0 * :CI Test\r\n"
        )
        reply = self.receive_until(client, b" 433 * {MAPPING} ")
        self.assertIn(
            b" 433 * {MAPPING} :Nickname is already in use",
            reply,
        )

    def test_rejects_duplicate_nickname_with_standard_numeric(self):
        self.register("duplicate")
        client = self.connect_client()
        client.sendall(
            b"PASS ci-test-password\r\n"
            b"NICK duplicate\r\n"
            b"USER duplicate 0 * :CI Test\r\n"
        )
        duplicate = self.receive_until(client, b" 433 * duplicate ")
        self.assertIn(
            b":irc.Brika.net 433 * duplicate :Nickname is already in use",
            duplicate,
        )
        self.assertNotIn(b" 001 ", duplicate)

        client.sendall(b"NICK duplicate_new\r\n")
        welcome = self.receive_until(client, b" 001 duplicate_new ")
        self.assertIn(b" 001 duplicate_new ", welcome)

    def test_nickname_uniqueness_uses_irc_case_mapping(self):
        self.register("CaseNick")
        client = self.connect_client()
        client.sendall(
            b"pass ci-test-password\r\n"
            b"nick casenick\r\n"
            b"user another_user 0 * :CI Test\r\n"
        )
        reply = self.receive_until(client, b" 433 * casenick ")
        self.assertIn(b" 433 * casenick :Nickname is already in use", reply)
        self.assertNotIn(b" 001 ", reply)

    def test_rejects_invalid_nickname_without_registering(self):
        client = self.connect_client()
        client.sendall(
            b"PASS ci-test-password\r\n"
            b"NICK 123invalid\r\n"
            b"USER invalid_user 0 * :CI Test\r\n"
        )
        reply = self.receive_until(client, b" 432 * 123invalid ")
        self.assertIn(
            b":irc.Brika.net 432 * 123invalid :Erroneous nickname",
            reply,
        )
        self.assertNotIn(b" 001 ", reply)

    def test_rejects_nickname_longer_than_thirty_characters(self):
        nickname = "n" * 31
        client = self.connect_client()
        client.sendall(
            b"PASS ci-test-password\r\n"
            + ("NICK " + nickname + "\r\n").encode()
            + b"USER test_user 0 * :CI Test\r\n"
        )
        reply = self.receive_until(client, (" 432 * " + nickname + " ").encode())
        self.assertIn(
            (":irc.Brika.net 432 * " + nickname + " :Erroneous nickname").encode(),
            reply,
        )
        self.assertNotIn(b" 001 ", reply)

    def test_requires_four_user_parameters_and_nonempty_username(self):
        client = self.connect_client()
        client.sendall(b"PASS ci-test-password\r\nNICK validuser\r\nUSER\r\n")
        reply = self.receive_until(client, b" 461 * USER ")
        self.assertIn(b":irc.Brika.net 461 * USER :Not enough parameters", reply)
        self.assertNotIn(b" 001 ", reply)

        client.sendall(b"USER validuser 0 * :CI Test\r\n")
        welcome = self.receive_until(client, b" 001 validuser ")
        self.assertIn(b" 001 validuser ", welcome)

    def test_invalid_password_can_be_retried_before_registration(self):
        client = self.connect_client()
        client.sendall(b"PASS incorrect-password\r\n")
        error = self.receive_until(client, b" 464 * ")
        self.assertIn(b" 464 * :Password incorrect", error)

        client.sendall(
            b"PASS ci-test-password\r\n"
            b"NICK password_retry\r\n"
            b"USER retry_user 0 * :CI Test\r\n"
        )
        welcome = self.receive_until(client, b" 001 password_retry ")
        self.assertIn(b" 001 password_retry ", welcome)

    def test_rejects_unsupported_capability_requests(self):
        client = self.connect_client()
        client.sendall(b"CAP rEq :sasl\r\nCAP END\r\n")
        reply = self.receive_until(client, b" CAP * NAK :sasl")
        self.assertIn(b":irc.Brika.net CAP * NAK :sasl", reply)

        client.sendall(b"CAP UNKNOWN\r\n")
        invalid = self.receive_until(client, b" 410 * UNKNOWN ")
        self.assertIn(
            b":irc.Brika.net 410 * UNKNOWN :Invalid CAP subcommand",
            invalid,
        )

    def test_rejects_reregistering_after_welcome(self):
        client = self.register("reregister")
        client.sendall(b"PASS ci-test-password\r\n")
        reply = self.receive_until(client, b" 462 reregister ")
        self.assertIn(
            b":irc.Brika.net 462 reregister :You may not reregister",
            reply,
        )

    def test_registered_client_can_change_nickname(self):
        client = self.register("old_nickname")
        observer = self.register("nick_observer")
        client.sendall(b"NICK new_nickname\r\n")
        own_reply = self.receive_until(client, b"NICK :new_nickname")
        observer_reply = self.receive_until(observer, b"NICK :new_nickname")
        expected = b":old_nickname!old_nickname@localhost NICK :new_nickname"
        self.assertIn(expected, own_reply)
        self.assertIn(expected, observer_reply)

    def test_registered_client_cannot_change_to_a_taken_nickname(self):
        self.register("taken_nick")
        client = self.register("change_nick_user")
        client.sendall(b"NICK taken_nick\r\n")
        reply = self.receive_until(client, b" 433 change_nick_user taken_nick ")
        self.assertIn(
            b" 433 change_nick_user taken_nick :Nickname is already in use",
            reply,
        )

    def test_answers_ping_before_registration(self):
        client = self.connect_client()
        client.sendall(b"PING :registration-check\r\n")
        reply = self.receive_until(client, b"PONG irc.Brika.net :registration-check")
        self.assertIn(
            b":irc.Brika.net PONG irc.Brika.net :registration-check",
            reply,
        )

    def test_answers_ping_after_registration(self):
        client = self.register("ping_registered")
        client.sendall(b"PING :after-registration-token\r\n")
        reply = self.receive_until(client, b"PONG irc.Brika.net :after-registration-token")
        self.assertIn(
            b":irc.Brika.net PONG irc.Brika.net :after-registration-token",
            reply,
        )

    def test_rejects_commands_before_registration(self):
        client = self.connect_client()
        client.sendall(b"PRIVMSG anyone :hello\r\n")
        reply = self.receive_until(client, b" 451 * ")
        self.assertIn(b":irc.Brika.net 451 * :You have not registered", reply)

    def test_server_accepts_clients_after_disconnect(self):
        first = self.register("disconnecting")
        first.close()
        second = self.register("afterdisconnect")
        self.assertIsNotNone(second)
        self.assertIsNone(self.server.poll())

    def test_handles_command_split_across_tcp_writes(self):
        client = self.connect_client()
        client.sendall(b"PASS ")
        time.sleep(0.02)
        client.sendall(
            b"ci-test-password\r\n"
            b"NICK splitwrite\r\n"
            b"USER splitwrite 0 * :CI Test\r\n"
        )
        welcome = self.receive_until(client, b" 001 splitwrite ")
        self.assertIn(b" 001 splitwrite ", welcome)

    def test_rejects_incorrect_password(self):
        client = self.connect_client()
        client.sendall(b"PASS incorrect-password\r\n")
        reply = self.receive_until(client, b" 464 * ")
        self.assertIn(
            b":irc.Brika.net 464 * :Password incorrect",
            reply,
        )

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

    def test_channel_message_preserves_spaces_and_is_not_echoed_to_sender(self):
        sender = self.register("multiword_sender")
        receiver = self.register("multiword_receiver")
        self.join_channel(sender, "multiword_sender", "#multiword")
        self.join_channel(receiver, "multiword_receiver", "#multiword")
        self.receive_until(
            sender, b":multiword_receiver!multiword_receiver@localhost JOIN #multiword"
        )

        sender.sendall(b"privmsg #multiword :multiple words stay together\r\n")
        delivered = self.receive_until(receiver, b"multiple words stay together")
        self.assertIn(
            b":multiword_sender!multiword_sender@localhost PRIVMSG #multiword :multiple words stay together",
            delivered,
        )
        sender.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            sender.recv(4096)

    def test_join_broadcasts_join_to_existing_members(self):
        first = self.register("join_first")
        second = self.register("join_second")
        self.join_channel(first, "join_first", "#join-broadcast")

        second.sendall(b"JOIN #join-broadcast\r\n")
        second_reply = self.receive_until(second, b" 366 join_second #join-broadcast ")
        first_reply = self.receive_until(first, b":join_second!join_second@localhost JOIN #join-broadcast")
        self.assertIn(b" 353 join_second = #join-broadcast ", second_reply)
        self.assertIn(
            b":join_second!join_second@localhost JOIN #join-broadcast",
            first_reply,
        )

    def test_joining_same_channel_twice_does_not_duplicate_membership(self):
        client = self.register("join_once")
        self.join_channel(client, "join_once", "#join-once")
        client.sendall(b"JOIN #join-once\r\n")
        client.settimeout(0.1)
        with self.assertRaises(socket.timeout):
            client.recv(4096)

    def test_part_broadcasts_reason_and_removes_membership(self):
        leaving = self.register("part_leaving")
        remaining = self.register("part_remaining")
        self.join_channel(leaving, "part_leaving", "#part-flow")
        self.join_channel(remaining, "part_remaining", "#part-flow")

        leaving.sendall(b"PART #part-flow :done for today\r\n")
        expected = (
            b":part_leaving!part_leaving@localhost PART #part-flow :done for today"
        )
        self.assertIn(expected, self.receive_until(leaving, expected))
        self.assertIn(expected, self.receive_until(remaining, expected))

        leaving.sendall(b"PRIVMSG #part-flow :I have left\r\n")
        rejected = self.receive_until(leaving, b" 404 part_leaving #part-flow ")
        self.assertIn(
            b" 404 part_leaving #part-flow :Cannot send to channel",
            rejected,
        )

    def test_part_without_reason_omits_trailing_reason(self):
        leaving = self.register("part_default")
        remaining = self.register("part_default_peer")
        self.join_channel(leaving, "part_default", "#part-default")
        self.join_channel(remaining, "part_default_peer", "#part-default")

        leaving.sendall(b"PART #part-default\r\n")
        expected = (
            b":part_default!part_default@localhost PART #part-default\r\n"
        )
        self.assertIn(expected, self.receive_until(remaining, expected))

    def test_part_leaves_multiple_channels_with_one_reason(self):
        leaving = self.register("part_multi")
        first_peer = self.register("part_multi_first")
        second_peer = self.register("part_multi_second")
        self.join_channel(leaving, "part_multi", "#part-first")
        self.join_channel(leaving, "part_multi", "#part-second")
        self.join_channel(first_peer, "part_multi_first", "#part-first")
        self.join_channel(second_peer, "part_multi_second", "#part-second")

        leaving.sendall(b"PART #part-first,#part-second :leaving both\r\n")
        first_part = (
            b":part_multi!part_multi@localhost PART #part-first :leaving both"
        )
        second_part = (
            b":part_multi!part_multi@localhost PART #part-second :leaving both"
        )
        self.assertIn(first_part, self.receive_until(leaving, first_part))
        self.assertIn(second_part, self.receive_until(leaving, second_part))
        self.assertIn(first_part, self.receive_until(first_peer, first_part))
        self.assertIn(second_part, self.receive_until(second_peer, second_part))

    def test_part_continues_after_an_invalid_channel_target(self):
        leaving = self.register("part_mixed")
        peer = self.register("part_mixed_peer")
        self.join_channel(leaving, "part_mixed", "#part-valid")
        self.join_channel(peer, "part_mixed_peer", "#part-valid")

        leaving.sendall(b"PART #part-missing,#part-valid :partial success\r\n")
        error = self.receive_until(
            leaving, b" 403 part_mixed #part-missing :No such channel"
        )
        self.assertIn(
            b":irc.Brika.net 403 part_mixed #part-missing :No such channel",
            error,
        )
        part = b":part_mixed!part_mixed@localhost PART #part-valid :partial success"
        self.assertIn(part, self.receive_until(leaving, part))
        self.assertIn(part, self.receive_until(peer, part))

    def test_part_removes_empty_channel(self):
        client = self.register("part_last_member")
        self.join_channel(client, "part_last_member", "#part-empty")

        client.sendall(b"PART #part-empty :finished\r\n")
        self.assertIn(
            b" PART #part-empty :finished",
            self.receive_until(client, b" PART #part-empty :finished"),
        )

        client.sendall(b"TOPIC #part-empty\r\n")
        reply = self.receive_until(client, b" 403 part_last_member #part-empty ")
        self.assertIn(
            b" 403 part_last_member #part-empty :No such channel",
            reply,
        )

    def test_channel_key_rejects_missing_or_incorrect_key(self):
        owner = self.register("key_owner")
        joiner = self.register("key_joiner")
        self.join_channel(owner, "key_owner", "#keyed")
        owner.sendall(b"MODE #keyed +k secret\r\n")
        self.receive_until(owner, b" MODE #keyed +k secret")

        joiner.sendall(b"JOIN #keyed\r\n")
        missing = self.receive_until(joiner, b" 475 key_joiner #keyed ")
        self.assertIn(b" 475 key_joiner #keyed :Cannot join channel (+k)", missing)

        joiner.sendall(b"JOIN #keyed wrong\r\n")
        wrong = self.receive_until(joiner, b" 475 key_joiner #keyed ")
        self.assertIn(b" 475 key_joiner #keyed :Cannot join channel (+k)", wrong)

        joiner.sendall(b"JOIN #keyed secret\r\n")
        joined = self.receive_until(joiner, b" 366 key_joiner #keyed ")
        self.assertIn(b" 366 key_joiner #keyed ", joined)

    def test_invite_only_channel_allows_invited_client(self):
        owner = self.register("invite_owner")
        invited = self.register("invite_target")
        self.join_channel(owner, "invite_owner", "#invite-flow")
        owner.sendall(b"MODE #invite-flow +i\r\n")
        self.receive_until(owner, b" MODE #invite-flow +i")

        invited.sendall(b"JOIN #invite-flow\r\n")
        denied = self.receive_until(invited, b" 473 invite_target #invite-flow ")
        self.assertIn(
            b" 473 invite_target #invite-flow :Cannot join channel (+i)",
            denied,
        )

        owner.sendall(b"INVITE invite_target #invite-flow\r\n")
        invite_reply = self.receive_until(owner, b" 341 invite_owner invite_target #invite-flow")
        delivered = self.receive_until(invited, b" INVITE invite_target :#invite-flow")
        self.assertIn(b" 341 invite_owner invite_target #invite-flow", invite_reply)
        self.assertIn(
            b":invite_owner!invite_owner@localhost INVITE invite_target :#invite-flow",
            delivered,
        )

        invited.sendall(b"JOIN #invite-flow\r\n")
        joined = self.receive_until(invited, b" 366 invite_target #invite-flow ")
        self.assertIn(b" 366 invite_target #invite-flow ", joined)

    def test_channel_user_limit_blocks_additional_members(self):
        owner = self.register("limit_owner")
        second = self.register("limit_second")
        third = self.register("limit_third")
        self.join_channel(owner, "limit_owner", "#limited")
        owner.sendall(b"MODE #limited +l 2\r\n")
        self.receive_until(owner, b" MODE #limited +l 2")
        self.join_channel(second, "limit_second", "#limited")

        third.sendall(b"JOIN #limited\r\n")
        denied = self.receive_until(third, b" 471 limit_third #limited ")
        self.assertIn(
            b" 471 limit_third #limited :Cannot join channel (+l)",
            denied,
        )

    def test_mode_query_reports_default_and_updated_modes(self):
        client = self.register("mode_query")
        self.join_channel(client, "mode_query", "#mode-query")
        client.sendall(b"MODE #mode-query\r\n")
        default = self.receive_until(client, b" 324 mode_query #mode-query ")
        self.assertIn(b" 324 mode_query #mode-query +t", default)

        client.sendall(b"MODE #mode-query +i\r\n")
        self.receive_until(client, b" MODE #mode-query +i")
        client.sendall(b"MODE #mode-query\r\n")
        updated = self.receive_until(client, b" 324 mode_query #mode-query ")
        self.assertIn(b" 324 mode_query #mode-query +it", updated)

    def test_operator_mode_grants_and_revokes_topic_privileges(self):
        owner = self.register("promote_owner")
        member = self.register("promote_member")
        self.join_channel(owner, "promote_owner", "#promotion")
        self.join_channel(member, "promote_member", "#promotion")

        owner.sendall(b"MODE #promotion +o promote_member\r\n")
        promoted = self.receive_until(owner, b" MODE #promotion +o promote_member")
        self.assertIn(b" MODE #promotion +o promote_member", promoted)
        member.sendall(b"TOPIC #promotion :member topic\r\n")
        topic = self.receive_until(owner, b" TOPIC #promotion :member topic")
        self.assertIn(b" TOPIC #promotion :member topic", topic)

        owner.sendall(b"MODE #promotion -o promote_member\r\n")
        self.receive_until(owner, b" MODE #promotion -o promote_member")
        member.sendall(b"TOPIC #promotion :denied topic\r\n")
        denied = self.receive_until(member, b" 482 promote_member #promotion ")
        self.assertIn(
            b" 482 promote_member #promotion :You're not a channel operator",
            denied,
        )

    def test_topic_updates_are_broadcast_to_all_channel_members(self):
        owner = self.register("topic_broadcast_owner")
        member = self.register("topic_broadcast_member")
        self.join_channel(owner, "topic_broadcast_owner", "#topic-broadcast")
        self.join_channel(member, "topic_broadcast_member", "#topic-broadcast")

        owner.sendall(b"TOPIC #topic-broadcast :shared topic\r\n")
        owner_reply = self.receive_until(owner, b" TOPIC #topic-broadcast :shared topic")
        member_reply = self.receive_until(member, b" TOPIC #topic-broadcast :shared topic")
        expected = (
            b":topic_broadcast_owner!topic_broadcast_owner@localhost "
            b"TOPIC #topic-broadcast :shared topic"
        )
        self.assertIn(expected, owner_reply)
        self.assertIn(expected, member_reply)

    def test_kick_notifies_channel_and_removes_target_membership(self):
        owner = self.register("kick_owner")
        target = self.register("kick_target")
        observer = self.register("kick_observer")
        for client, nick in (
            (owner, "kick_owner"),
            (target, "kick_target"),
            (observer, "kick_observer"),
        ):
            self.join_channel(client, nick, "#kick-flow")

        owner.sendall(b"KICK #kick-flow kick_target :test removal\r\n")
        expected = (
            b":kick_owner!kick_owner@localhost KICK #kick-flow "
            b"kick_target :test removal"
        )
        self.assertIn(expected, self.receive_until(owner, expected))
        self.assertIn(expected, self.receive_until(target, expected))
        self.assertIn(expected, self.receive_until(observer, expected))

        target.sendall(b"PRIVMSG #kick-flow :not a member\r\n")
        rejected = self.receive_until(target, b"404 kick_target #kick-flow ")
        self.assertIn(b"404 kick_target #kick-flow :Cannot send to channel", rejected)

    def test_kick_without_comment_uses_requesting_nickname(self):
        owner = self.register("kick_default_owner")
        target = self.register("kick_default_target")
        self.join_channel(owner, "kick_default_owner", "#kick-default")
        self.join_channel(target, "kick_default_target", "#kick-default")

        owner.sendall(b"KICK #kick-default kick_default_target\r\n")
        expected = (
            b":kick_default_owner!kick_default_owner@localhost KICK #kick-default "
            b"kick_default_target :kick_default_owner"
        )
        self.assertIn(expected, self.receive_until(target, expected))

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

    def test_quiz_bot_validates_answer_number(self):
        client = self.register("bot_answer_user")
        client.sendall(b"PRIVMSG QuizBot :!quiz\r\n")
        question = self.receive_until(client, b"What is the capital of Morocco?")
        self.assertIn(b"What is the capital of Morocco?", question)

        client.sendall(b"PRIVMSG QuizBot :!answer 2extra\r\n")
        invalid = self.receive_until(client, b"Wrong answer!")
        self.assertIn(b"Wrong answer!", invalid)

        client.sendall(b"PRIVMSG QuizBot :!answer 2\r\n")
        correct = self.receive_until(client, b"CORRECT!")
        self.assertIn(b"CORRECT!", correct)

    def test_quiz_bot_rejects_answer_before_quiz_starts(self):
        client = self.register("bot_before_quiz")
        client.sendall(b"PRIVMSG QuizBot :!answer 2\r\n")
        reply = self.receive_until(client, b"Type !quiz to start")
        self.assertIn(b"Type !quiz to start a question first!", reply)

    def test_quiz_bot_wrong_answer_keeps_current_question(self):
        client = self.register("bot_wrong_answer")
        client.sendall(b"PRIVMSG QuizBot :!quiz\r\n")
        self.receive_until(client, b"What is the capital of Morocco?")

        client.sendall(b"PRIVMSG QuizBot :!answer 1\r\n")
        wrong = self.receive_until(client, b"Wrong answer!")
        self.assertIn(b"Wrong answer!", wrong)

        client.sendall(b"PRIVMSG QuizBot :!quiz\r\n")
        same_question = self.receive_until(client, b"What is the capital of Morocco?")
        self.assertIn(b"What is the capital of Morocco?", same_question)

    def test_quiz_bot_advances_to_next_question_after_correct_answer(self):
        client = self.register("bot_advance")
        client.sendall(b"PRIVMSG QuizBot :!quiz\r\n")
        self.receive_until(client, b"What is the capital of Morocco?")
        client.sendall(b"PRIVMSG QuizBot :!answer 2\r\n")
        self.assertIn(b"CORRECT!", self.receive_until(client, b"CORRECT!"))

        client.sendall(b"PRIVMSG QuizBot :!quiz\r\n")
        next_question = self.receive_until(client, b"Which system call")
        self.assertIn(b"Which system call watches multiple file descriptors", next_question)

    def test_quiz_bot_handles_unknown_commands(self):
        client = self.register("bot_unknown")
        client.sendall(b"PRIVMSG QuizBot :!unknown\r\n")
        reply = self.receive_until(client, b"Unknown bot command")
        self.assertIn(b"Unknown bot command. Type !help for info.", reply)

    def test_rejects_invalid_port_values(self):
        for port in ("65536", "999999999999999999999999", "123abc"):
            result = subprocess.run(
                [SERVER, port, PASSWORD],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=2,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Port must be a number", result.stderr)


PROTOCOL_CASES = [
    ("join_requires_channel", "none", "JOIN", " 461 {nick} JOIN", False),
    ("join_rejects_invalid_channel", "none", "JOIN invalid", " 403 {nick} invalid", False),
    ("part_requires_channel", "none", "PART", " 461 {nick} PART", False),
    ("part_rejects_invalid_channel", "none", "PART invalid", " 403 {nick} invalid", False),
    ("part_requires_existing_channel", "none", "PART #missing", " 403 {nick} #missing", False),
    ("part_requires_membership", "channel", "PART {channel}", " 442 {nick} {channel}", False),
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
