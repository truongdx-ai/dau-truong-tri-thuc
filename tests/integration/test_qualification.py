"""Exercise qualification through real TCP, including an idle 30-second deadline."""
from contextlib import ExitStack
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()


class Peer:
    def __init__(self, address):
        self.socket = socket.create_connection(address, timeout=35)
        self.reader = self.socket.makefile("rb")
        self.closed = False

    def send(self, line):
        self.socket.sendall((line + "\n").encode())

    def expect(self, expected):
        actual = self.reader.readline().decode().rstrip("\n")
        assert actual == expected, (expected, actual)

    def command(self, line, expected):
        self.send(line)
        self.expect(expected)

    def close(self):
        if not self.closed:
            self.closed = True
            self.reader.close()
            self.socket.close()


with tempfile.TemporaryDirectory() as directory, ExitStack() as stack:
    (Path(directory) / "questions.txt").write_text(
        "First question?|one|two|three|four|D\n"
        "Second question?|red|blue|green|white|C\n", encoding="utf-8")
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        address = reserve.getsockname()
    server = subprocess.Popen([str(build / "server"), "-p", str(address[1]), "-d", directory])

    def connect():
        peer = Peer(address)
        stack.callback(peer.close)
        return peer

    def account(peer, name):
        peer.command(f"REGISTER|{name}|pw", "200|OK")
        peer.command(f"LOGIN|{name}|pw", "200|OK")

    def roster(peers, names):
        line = f"PLAYERS|{len(names)}|" + ",".join(names)
        for peer in peers:
            peer.expect(line)

    def start(host, peers, names, qid, question):
        host.command("START", "200|OK")
        roster(peers, names)
        for peer in peers:
            peer.expect(f"QUESTION|{qid}|QUALIFY|30|{question}")

    try:
        for _ in range(100):
            try:
                a = connect()
                break
            except ConnectionRefusedError:
                assert server.poll() is None
                time.sleep(.02)
        else:
            raise RuntimeError("Server start timed out")
        b, c, spectator = connect(), connect(), connect()
        for peer, name in ((a, "a"), (b, "b"), (c, "c"), (spectator, "spectator")):
            account(peer, name)
        joined = []
        names = []
        for peer, name in ((a, "a"), (b, "b"), (c, "c")):
            peer.command("JOIN", "200|OK")
            joined.append(peer)
            names.append(name)
            roster(joined, names)
        start(a, joined, names, 1, "First question?|one|two|three|four")
        spectator.command("PING", "PONG")  # No QUESTION was sent to a spectator.
        spectator.command("ANSWER|1|D", "403|NOT_ALLOWED")
        a.command("ANSWER|0|D", "400|BAD_REQUEST")
        a.command("ANSWER|999|D", "409|WRONG_QUESTION")
        a.command("ANSWER|1|AA", "400|BAD_REQUEST")
        a.command("ANSWER|1|A", "200|RECEIVED")
        a.command("ANSWER|1|D", "409|ALREADY_ANSWERED")
        # A split TCP frame is accepted only when its final LF arrives.
        b.socket.sendall(b"ANSWER|1|")
        b.socket.sendall(b"D\n")
        b.expect("200|RECEIVED")
        c.command("ANSWER|1|D", "200|RECEIVED")
        a.expect("ROLE|SUB|b")
        b.expect("ROLE|MAIN")
        c.expect("ROLE|SUB|b")
        a.command("START", "409|GAME_RUNNING")
        a.command("LEAVE", "200|OK")
        roster((b, c), ("b", "c"))
        b.close()
        roster((c,), ("c",))
        c.expect("ROLE|MAIN")
        # Reuse the departed main's connection slot without inheriting participation.
        replacement = connect()
        account(replacement, "replacement")
        replacement.command("JOIN", "409|GAME_RUNNING")
        replacement.command("ANSWER|1|D", "403|NOT_ALLOWED")
        c.command("LOGOUT", "200|OK")
        replacement.command("JOIN", "200|OK")
        roster((replacement,), ("replacement",))
        a.command("JOIN", "200|OK")
        roster((a, replacement), ("a", "replacement"))
        start(replacement, (a, replacement), ("a", "replacement"), 2,
              "Second question?|red|blue|green|white")
        a.command("ANSWER|1|D", "409|WRONG_QUESTION")
        a.command("ANSWER|2|A", "200|RECEIVED")
        replacement.command("ANSWER|2|B", "200|RECEIVED")
        a.expect("GAME_OVER|NO_WINNER")
        replacement.expect("GAME_OVER|NO_WINNER")
        start(replacement, (a, replacement), ("a", "replacement"), 3,
              "First question?|one|two|three|four")
        # No client command wakes the server: the poll loop must resolve this itself.
        a.expect("GAME_OVER|NO_WINNER")
        replacement.expect("GAME_OVER|NO_WINNER")
        a.command("ANSWER|3|D", "408|TIMEOUT")
        start(replacement, (a, replacement), ("a", "replacement"), 4,
              "Second question?|red|blue|green|white")
        replacement.command("ANSWER|4|C", "200|RECEIVED")
        a.command("LOGOUT", "200|OK")
        roster((replacement,), ("replacement",))
        replacement.expect("ROLE|MAIN")
        log = (Path(directory) / "server.log").read_text()
        assert "ANSWER received" in log
        assert "QUALIFY MAIN b" in log and "QUALIFY MAIN c" in log
        assert "QUALIFY NO_WINNER" in log
        print("Qualification TCP scenarios passed (including 30-second idle timeout)")
    finally:
        server.terminate()
        assert server.wait(timeout=5) == 0
