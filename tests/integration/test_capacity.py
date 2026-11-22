"""Exercise connection capacity and reuse after a disconnect."""
from contextlib import ExitStack, closing
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as data, ExitStack() as stack:
    shutil.copyfile("data/questions.txt", Path(data) / "questions.txt")
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        address = reserve.getsockname()
    server = subprocess.Popen([str(build / "server"), "-p", str(address[1]), "-d", data])

    def connect():
        return stack.enter_context(closing(socket.create_connection(address, timeout=5)))

    def ping(client):
        client.sendall(b"PING\n")
        with client.makefile("rb") as reader:
            assert reader.readline() == b"PONG\n"

    try:
        for _ in range(100):
            try:
                first = connect()
                break
            except ConnectionRefusedError:
                assert server.poll() is None
                time.sleep(.02)
        else:
            raise RuntimeError("Server start timed out")
        clients = [first]
        ping(first)
        for _ in range(99):
            client = connect()
            ping(client)
            clients.append(client)
        extra = connect()
        try:
            assert extra.recv(1) == b""
        except ConnectionResetError:
            pass
        # A full room must keep serving existing peers.
        ping(clients[-1])
        clients[0].shutdown(socket.SHUT_WR)
        assert clients[0].recv(1) == b""
        clients[0].close()
        ping(connect())
        print("100 connections served, excess rejected, freed slot reused")
    finally:
        server.terminate()
        assert server.wait(timeout=5) == 0
