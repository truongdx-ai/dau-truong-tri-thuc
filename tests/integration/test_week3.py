"""Play one full timed turn through TCP with three authenticated clients."""
from contextlib import ExitStack
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()


class Peer:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=40)
        self.reader = self.socket.makefile("rb")
        self.scores = {}

    def send(self, line):
        self.socket.sendall((line + "\n").encode())

    def read(self):
        while True:
            line = self.reader.readline().decode().strip()
            assert line, "Server closed unexpectedly"
            if line.startswith("SCOREBOARD_BEGIN|"):
                self.scores = {}
            elif line.startswith("SCORE|"):
                _, user, points, status = line.split("|")
                self.scores[user] = (int(points), status)
            elif not line.startswith("SCOREBOARD_END|"):
                return line

    def expect(self, line):
        actual = self.read()
        assert actual == line, (line, actual)

    def until(self, prefix):
        for _ in range(20):
            line = self.read()
            if line.startswith(prefix):
                return line
        raise AssertionError(prefix)

    def close(self):
        self.reader.close()
        self.socket.close()


with tempfile.TemporaryDirectory() as directory, ExitStack() as stack:
    # All questions use the same answer, independent of shuffled order.
    (Path(directory) / "questions.txt").write_text("Q|one|two|three|four|A\n" * 8)
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    server = subprocess.Popen([str(build / "server"), "-p", str(port), "-d", directory])
    try:
        for _ in range(100):
            try:
                first = Peer(port)
                break
            except ConnectionRefusedError:
                assert server.poll() is None
                time.sleep(.02)
        else:
            raise RuntimeError("Server did not start")
        peers = [first]
        stack.callback(first.close)
        for _ in range(2):
            peer = Peer(port)
            peers.append(peer)
            stack.callback(peer.close)

        for i, peer in enumerate(peers):
            for command in (f"REGISTER|p{i}|pw", f"LOGIN|p{i}|pw", "JOIN"):
                peer.send(command)
                peer.until("200|OK")
        peers[0].send("START")
        peers[0].until("200|OK")
        qualification = [peer.until("QUESTION|").split("|") for peer in peers]
        assert all(q[2] == "QUALIFY" and q[3] == "30" for q in qualification)
        qid = qualification[0][1]
        # Wait for acknowledgements so server receives p1's correct answer first.
        for index in (1, 0, 2):
            peers[index].send(f"ANSWER|{qid}|A")
            peers[index].expect("200|RECEIVED")
        assert peers[1].until("ROLE|") == "ROLE|MAIN"
        assert peers[0].until("ROLE|") == "ROLE|SUB|p1"
        assert peers[2].until("ROLE|") == "ROLE|SUB|p1"
        turns = [peer.until("QUESTION|").split("|") for peer in peers]
        assert [q[3] for q in turns] == ["30", "60", "30"]
        turn_id = turns[0][1]
        peers[0].send(f"ANSWER|{qid}|A")
        peers[0].expect("409|STALE_QUESTION")
        peers[0].send(f"SKIP|{turn_id}")
        peers[0].expect("403|NOT_MAIN")
        for index, answer in ((0, "B"), (1, "A"), (2, "B")):
            peers[index].send(f"ANSWER|{turn_id}|{answer}")
            peers[index].expect("200|RECEIVED")
        # SUB's window closes after 30 seconds, even when everyone answered.
        for index in (0, 2):
            peers[index].expect("TURN_RESULT|A|WRONG|0")
            peers[index].expect("ELIMINATED|WRONG_ANSWER")
            peers[index].expect("GAME_OVER|WINNER|p1|30")
        peers[1].expect("TURN_RESULT|A|CORRECT|30")
        peers[1].expect("GAME_OVER|WINNER|p1|30")
        for peer in peers:
            assert peer.scores == {"p0": (0, "ELIMINATED"), "p1": (30, "ACTIVE"),
                                   "p2": (0, "ELIMINATED")}
        # Same lobby can start another game with fresh points and fresh qids.
        peers[0].send("START")
        peers[0].expect("200|OK")
        next_question = peers[0].until("QUESTION|").split("|")
        assert int(next_question[1]) > int(turn_id)
        log = (Path(directory) / "server.log").read_text()
        for event in ("START accepted", "ANSWER accepted", "SKIP rejected",
                      "QUESTION", "ROLE|MAIN", "TURN_RESULT|A|CORRECT|30",
                      "ELIMINATED|WRONG_ANSWER", "GAME_OVER|WINNER|p1|30"):
            assert event in log, event
        assert "|pw" not in log
        print("Three-client qualification, main selection, timed scoring and restart passed")
    finally:
        server.terminate()
        assert server.wait(timeout=5) == 0
