"""Verify client rendering and answer shortcuts against a scripted TCP peer."""
from pathlib import Path
import socket
import subprocess
import sys

build = Path(sys.argv[1]).resolve()


def scenario(raw=False):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        args = [str(build / "client"), "-p", str(listener.getsockname()[1])]
        if raw:
            args.append("-r")
        with subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE) as client:
            shown = bytearray()

            def see(expected):
                while True:
                    line = client.stdout.readline()
                    assert line, ("Client closed before rendering", expected)
                    shown.extend(line)
                    if expected.encode() in line:
                        return

            def enter(text):
                client.stdin.write(text.encode())
                client.stdin.flush()

            try:
                with listener.accept()[0] as peer:
                    peer.settimeout(5)
                    with peer.makefile("rb") as stream:
                        opening = "QUESTION|7|QUALIFY|30|First question?|one|two|three|four"
                        peer.sendall((opening + "\n").encode())
                        see(opening if raw else "Thoi gian: 30 giay")
                        enter("b\n")
                        assert stream.readline() == b"ANSWER|7|B\n"
                        enter("D\nPING\n")
                        assert stream.readline() == b"PING\n"  # Duplicate choice stayed local.
                        peer.sendall(b"200|RECEIVED\nROLE|MAIN\n")
                        see("ROLE|MAIN" if raw else "Ban la nguoi choi chinh.")
                        enter("A\nPING\n")
                        assert stream.readline() == b"PING\n"  # ROLE ended the answer window.
                        second = "QUESTION|8|QUALIFY|30|Second question?|red|blue|green|white"
                        peer.sendall((second + "\n").encode())
                        see(second if raw else "Thoi gian: 30 giay")
                        enter(" d \n")
                        assert stream.readline() == b"ANSWER|8|D\n"
                        peer.sendall(b"408|TIMEOUT\nGAME_OVER|NO_WINNER\n")
                        see("GAME_OVER|NO_WINNER" if raw else "khong co nguoi chien thang")
                        enter("C\nPING\n")
                        assert stream.readline() == b"PING\n"
                        third = "QUESTION|9|QUALIFY|30|Third question?|a|b|c|d"
                        peer.sendall((third + "\nROLE|SUB|an\n").encode())
                        see("ROLE|SUB|an" if raw else "Nguoi choi chinh: an.")
                        client.stdin.close()
                        assert stream.readline() == b""
                    peer.shutdown(socket.SHUT_WR)
                assert client.wait(timeout=5) == 0
                if not raw:
                    assert b"A. one\nB. two\nC. three\nD. four\n" in shown
                    assert b"Ban da gui dap an" in shown
                    assert b"Da het thoi gian tra loi" in shown
                print("Client raw events and shortcuts passed" if raw else
                      "Client friendly questions, roles and shortcuts passed")
            finally:
                if client.poll() is None:
                    client.terminate()
                    client.wait(timeout=5)


scenario()
scenario(raw=True)
