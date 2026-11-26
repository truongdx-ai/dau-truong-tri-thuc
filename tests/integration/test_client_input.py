"""Verify invalid local input is discarded without sending a truncated command."""
from pathlib import Path
import socket
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
with socket.socket() as listener:
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(5)
    port = str(listener.getsockname()[1])
    with subprocess.Popen([str(build / "client"), "-p", port], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as client:
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(5)
                client.stdin.write(b"PING\0ignored\nPING\n")
                client.stdin.flush()
                with peer.makefile("rb") as stream:
                    assert stream.readline() == b"PING\n"
                    peer.sendall(b"PONG\n")
                    client.stdin.close()
                    assert stream.readline() == b""
                peer.shutdown(socket.SHUT_WR)
            assert client.wait(timeout=5) == 0
            assert b"NUL" in client.stdout.read()
        finally:
            if client.poll() is None:
                client.terminate()
                client.wait(timeout=5)
print("Client rejects NUL input and resumes reading")

with socket.socket() as listener:
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(5)
    port = str(listener.getsockname()[1])
    with subprocess.Popen([str(build / "client"), "-p", port], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as client:
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(5)
                client.stdin.write(b"PING")
                client.stdin.close()
                with peer.makefile("rb") as stream:
                    assert stream.readline() == b"PING\n"
                    assert stream.readline() == b""
                peer.sendall(b"PONG\nQUESTION|7|TURN|30|2 + 2?|1|2|3|4\n")
                peer.shutdown(socket.SHUT_WR)
            assert client.wait(timeout=5) == 0
            rendered = client.stdout.read()
            assert b"PONG\n" in rendered
            assert b"Cau hoi 7 (TURN, 30 giay)" in rendered
            assert b"A) 1   B) 2   C) 3   D) 4" in rendered
        finally:
            if client.poll() is None:
                client.terminate()
                client.wait(timeout=5)
print("Client flushes final command at EOF and receives the final response")
