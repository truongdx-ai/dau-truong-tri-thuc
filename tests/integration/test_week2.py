"""Exercise the server binary and the interactive client through real TCP."""
import contextlib
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()


class Peer:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=15)
        self.reader = self.socket.makefile("rb")

    def send(self, line):
        self.socket.sendall((line + "\n").encode())

    def expect(self, line):
        actual = self.reader.readline().decode().rstrip("\n")
        assert actual == line, (line, actual)

    def close(self):
        self.reader.close()
        self.socket.close()


with tempfile.TemporaryDirectory() as directory, contextlib.ExitStack() as stack:
    shutil.copyfile("data/questions.txt", Path(directory) / "questions.txt")
    questions = [line for line in (Path(directory) / "questions.txt").read_text(
        encoding="utf-8").splitlines() if line]
    opening = "QUESTION|1|QUALIFY|30|" + "|".join(questions[0].split("|")[:5])
    next_opening = "QUESTION|2|QUALIFY|30|" + "|".join(questions[1 % len(questions)].split("|")[:5])
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    server = subprocess.Popen([str(build / "server"), "-p", str(port), "-d", directory])
    try:
        for _ in range(100):
            try:
                a = Peer(port)
                break
            except ConnectionRefusedError:
                assert server.poll() is None
                time.sleep(.02)
        else:
            raise RuntimeError("Server did not start")
        stack.callback(a.close)
        b, c = Peer(port), Peer(port)
        stack.callback(b.close)
        stack.callback(c.close)

        def command(peer, request, response):
            peer.send(request)
            peer.expect(response)

        command(a, "JOIN", "401|LOGIN_REQUIRED")
        command(a, "LOGIN|absent|pw", "401|WRONG_CREDENTIAL")
        command(a, "REGISTER|an|123", "200|OK")
        command(b, "REGISTER|tu|456", "200|OK")
        command(a, "LOGIN|an|wrong", "401|WRONG_CREDENTIAL")
        command(a, "LOGIN|an|123", "200|OK")
        command(c, "LOGIN|an|123", "409|ALREADY_LOGGED_IN")
        command(a, "JOIN", "200|OK")
        a.expect("PLAYERS|1|an")
        command(a, "START", "412|NOT_ENOUGH_PLAYERS")
        command(b, "LOGIN|tu|456", "200|OK")
        command(b, "JOIN", "200|OK")
        a.expect("PLAYERS|2|an,tu")
        b.expect("PLAYERS|2|an,tu")
        command(b, "START", "403|NOT_HOST")
        command(a, "START", "200|OK")
        a.expect("PLAYERS|2|an,tu")
        b.expect("PLAYERS|2|an,tu")
        a.expect(opening)
        b.expect(opening)
        command(a, "START", "409|GAME_RUNNING")
        command(a, "PING", "PONG")
        command(b, "PING", "PONG")
        command(c, "JOIN", "401|LOGIN_REQUIRED")
        command(a, "LEAVE", "200|OK")
        b.expect("PLAYERS|1|tu")
        command(b, "JOIN", "409|GAME_RUNNING")
        command(b, "LOGOUT", "200|OK")
        command(c, "LOGIN|tu|456", "200|OK")
        command(c, "JOIN", "200|OK")
        c.expect("PLAYERS|1|tu")
        command(c, "LEAVE", "200|OK")
        # Half-close must preserve all queued replies.
        a.socket.sendall(b"PING\n" * 500)
        a.socket.shutdown(socket.SHUT_WR)
        for _ in range(500):
            a.expect("PONG")
        a.expect("")
        command(c, "LOGOUT", "200|OK")
        # Real CLI keeps receiving pushes while waiting for stdin.
        client = subprocess.Popen([str(build / "client"), "-p", str(port)],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        try:
            client.stdin.write("LOGIN|tu|456\nJOIN\n")
            client.stdin.flush()
            while True:
                line = client.stdout.readline()
                assert line, "CLI closed before joining"
                if line.strip() == "PLAYERS|1|tu":
                    break
            command(b, "LOGIN|an|123", "200|OK")
            command(b, "JOIN", "200|OK")
            b.expect("PLAYERS|2|tu,an")
            assert client.stdout.readline().strip() == "PLAYERS|2|tu,an"
            client.stdin.write("START\n")
            client.stdin.flush()
            assert client.stdout.readline().strip() == "200|OK"
            assert client.stdout.readline().strip() == "PLAYERS|2|tu,an"
            b.expect("PLAYERS|2|tu,an")
            assert client.stdout.readline().strip() == next_opening
            b.expect(next_opening)
            client.stdin.close()
            assert client.wait(timeout=5) == 0
        finally:
            if client.poll() is None:
                client.terminate()
                client.wait(timeout=5)
            client.stdout.close()
            client.stderr.close()
        b.expect("PLAYERS|1|an")
        log = (Path(directory) / "server.log").read_text()
        assert "LOGIN accepted" in log and "LOGIN rejected" in log
        assert "LOGOUT accepted" in log and "SESSION closed" in log
        assert "123" not in log and "456" not in log
        print("Week 2 TCP and CLI scenarios passed")
    finally:
        server.terminate()
        assert server.wait(timeout=5) == 0
