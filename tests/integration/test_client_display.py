"""Verify the actual CLI shows a countdown and a final scoreboard."""
from pathlib import Path
import os
import pty
import select
import socket
import subprocess
import sys
import time

build = Path(sys.argv[1]).resolve()
with socket.socket() as listener:
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(5)
    master, slave = pty.openpty()
    client = subprocess.Popen([str(build / "client"), "-p", str(listener.getsockname()[1])],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=slave)
    os.close(slave)
    try:
        with listener.accept()[0] as peer:
            peer.sendall(b"QUESTION|7|TURN|2|Q|1|2|3|4\n")
            countdown = b""
            deadline = time.monotonic() + 5
            while b"Con lai 0 giay" not in countdown and time.monotonic() < deadline:
                if select.select([master], [], [], .2)[0]:
                    countdown += os.read(master, 4096)
            assert b"Con lai 2 giay (qid 7)" in countdown, countdown
            assert b"Con lai 1 giay (qid 7)" in countdown, countdown
            assert b"Con lai 0 giay (qid 7)" in countdown, countdown
            peer.sendall(b"TURN_RESULT|A|SKIPPED|5\nSCOREBOARD_BEGIN|7\n"
                         b"SCORE|an|5|ACTIVE\nSCORE|tu|15|ACTIVE\n"
                         b"SCOREBOARD_END|7\nGAME_OVER|WINNER|tu|15\n")
            peer.shutdown(socket.SHUT_WR)
            assert client.wait(timeout=5) == 0
            rendered = client.stdout.read()
            assert b"Bang diem (qid 7)" in rendered
            assert b"SKIPPED" in rendered
            assert b"Ket thuc van. Nguoi thang: tu, diem: 15" in rendered
            print("CLI countdown, scoreboard and final result rendered successfully")
    finally:
        if client.poll() is None:
            client.terminate()
            client.wait(timeout=5)
        client.stdin.close()
        client.stdout.close()
        os.close(master)
