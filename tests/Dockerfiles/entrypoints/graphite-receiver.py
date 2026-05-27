#!/usr/bin/env python3
"""Minimal Graphite/carbon plaintext receiver for NSClient++ integration tests.

Graphite's carbon line protocol is just "<path> <value> <timestamp>\\n" sent
over TCP (default port 2003). A full carbon+whisper+graphite-web stack is far
more than the test needs, so this stands in for it: listen on 2003 and append
every byte received from every connection to a file. The test reads that file
back via `docker exec cat` (containerReadAll) and asserts on the metric lines.

Single-threaded accept loop on purpose: NSClient++'s GraphiteClient opens a
short-lived connection, writes all its lines, and closes, so connections are
brief and serialising them is fine.
"""
import os
import socket

HOST = "0.0.0.0"
PORT = int(os.environ.get("PORT", "2003"))
OUT = os.environ.get("OUT", "/data/metrics.txt")


def main() -> None:
    # Touch the output file so `cat` works even before the first metric lands.
    open(OUT, "ab").close()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))
    srv.listen(16)

    while True:
        conn, _addr = srv.accept()
        try:
            with open(OUT, "ab") as f:
                while True:
                    data = conn.recv(4096)
                    if not data:
                        break
                    f.write(data)
                    f.flush()
        finally:
            conn.close()


if __name__ == "__main__":
    main()
