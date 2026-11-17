"""The stress harness's socket pump: the TCP connections a flood makes, made
from Python instead of node.

Every libuv to date (fixed in 1.53.0, which no node release ships yet) hands
an uninitialised OSVERSIONINFOW to RtlGetVersion on every loopback connect on
Windows (libuv issue #5106). Once in a while the garbage on the stack reads as
the size of the larger OSVERSIONINFOEXW, RtlGetVersion writes 8 bytes too far,
and node dies on the stack cookie (0xC0000409) with no output at all. A
connection per request at a few thousand a second hits it within minutes, so
the NRPE and NSCA clients (stress/lib/nrpe-client.ts, nsca-client.ts) hand
their connections to this process and keep everything else - packet encoding,
CRCs, ciphers, checking the answers - in TypeScript.

It knows no protocol. It speaks newline-delimited JSON on stdin/stdout, one
object per message, bytes in base64, matched up by `id`:

  -> {"id": 1, "op": "request", "host": ..., "port": ..., "send": b64,
      "tls": {"ca": path, "cert": path, "key": path, "servername": name} | null,
      "timeout": ms}
     Connect (and handshake), write `send`, read until the server closes.
  <- {"id": 1, "ok": true, "data": b64, "ms": exchange_ms, "retries": n}

  -> {"id": 2, "op": "handshake", "host": ..., "port": ..., "read": n, "timeout": ms}
     Connect and read exactly `read` bytes...
  <- {"id": 2, "phase": "read", "data": b64}
  -> {"id": 2, "send": b64}
     ...then write what the caller built from them and read until the server
     closes (NSCA: the IV packet, then the encrypted data packet). An empty
     `send` hangs up instead.
  <- {"id": 2, "ok": true, "data": b64, "ms": exchange_ms, "retries": n}

  <- {"id": n, "ok": false, "error": "...", "ms": ..., "retries": n} on any failure.

Reading until the server closes, and then resetting rather than closing,
matters: the side that closes first holds TIME_WAIT for two minutes on
Windows. A client that hung up first ran its 16384 ephemeral ports dry within
seconds; a server that is left holding it fills the (port, client port) pairs
instead, and on loopback a new connect from a reused port now and then has its
SYN dropped against one of them - a 21 s connect timeout (WinError 121), seen
right after a plain flood on the same port. Once the server has closed it has
sent everything, so a linger-0 close (an RST) loses nothing and leaves
neither side in TIME_WAIT. `ms` is the time from the
first connect attempt that got a port to the server's close; a connect that
failed for want of a local port is retried (like the TypeScript clients did)
and counted in `retries`, not timed.

Standard library only, so any Python 3.8+ on the runner does.
"""

import asyncio
import base64
import errno
import json
import socket
import ssl
import struct
import sys
import threading
import time

# A local port that is not there to be had: the ephemeral range is in
# TIME_WAIT. WSAEADDRINUSE and WSAENOBUFS are what Windows reports.
_NO_LOCAL_PORT = {errno.EADDRINUSE, errno.EADDRNOTAVAIL, 10048, 10049, 10055}
_MAX_RETRIES = 20
# struct linger: two u_shorts on Windows, two ints elsewhere. On, 0 s: an RST.
_LINGER_0 = struct.pack("HH" if sys.platform == "win32" else "ii", 1, 0)

_contexts = {}


def _context(tls):
    key = (tls.get("ca"), tls.get("cert"), tls.get("key"))
    ctx = _contexts.get(key)
    if ctx is None:
        ctx = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=tls.get("ca"))
        # Python 3.13 made the default context X509-strict, which refuses the
        # test CA (generateCertChain issues it without an Authority Key
        # Identifier). Node accepts the same chain; verify like it does.
        ctx.verify_flags &= ~getattr(ssl, "VERIFY_X509_STRICT", 0)
        if tls.get("cert"):
            ctx.load_cert_chain(tls["cert"], tls.get("key"))
        _contexts[key] = ctx
    return ctx


def _emit(message):
    sys.stdout.write(json.dumps(message, separators=(",", ":")) + "\n")
    sys.stdout.flush()


async def _connect(msg):
    """Open the connection, retrying while no local port is to be had."""
    tls = msg.get("tls")
    for attempt in range(_MAX_RETRIES + 1):
        try:
            if tls:
                reader, writer = await asyncio.open_connection(
                    msg["host"], msg["port"], ssl=_context(tls),
                    server_hostname=tls.get("servername") or msg["host"])
            else:
                reader, writer = await asyncio.open_connection(msg["host"], msg["port"])
            return reader, writer, attempt
        except OSError as e:
            if (e.errno not in _NO_LOCAL_PORT and getattr(e, "winerror", None) not in _NO_LOCAL_PORT) \
                    or attempt >= _MAX_RETRIES:
                raise
            await asyncio.sleep(0.05 * (attempt + 1))


async def _read_to_close(reader):
    chunks = []
    while True:
        try:
            chunk = await reader.read(65536)
        except (ssl.SSLError, ConnectionResetError) as e:
            # A server that drops the connection without a close_notify (or
            # resets it) has still said everything it was going to.
            if chunks:
                break
            raise e
        if not chunk:
            break
        chunks.append(chunk)
    return b"".join(chunks)


def _reset(writer):
    """Close with an RST, so neither side keeps the connection in TIME_WAIT."""
    sock = writer.get_extra_info("socket")
    try:
        if sock is not None:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, _LINGER_0)
    except OSError:
        pass
    writer.transport.abort()


class Pump:
    def __init__(self, loop):
        self.loop = loop
        # Handshake replies from the caller, by id.
        self.replies = {}

    def dispatch(self, line):
        msg = json.loads(line)
        if "op" in msg:
            self.loop.create_task(self.run(msg))
            return
        future = self.replies.pop(msg["id"], None)
        if future is not None and not future.done():
            future.set_result(base64.b64decode(msg["send"]))

    async def run(self, msg):
        rid = msg["id"]
        started = None
        retries = 0
        writer = None
        try:
            async def exchange():
                nonlocal started, retries, writer
                reader, writer, retries = await _connect(msg)
                started = time.perf_counter()
                if msg["op"] == "handshake":
                    got = await reader.readexactly(msg["read"])
                    reply = self.loop.create_future()
                    self.replies[rid] = reply
                    _emit({"id": rid, "phase": "read", "data": base64.b64encode(got).decode()})
                    send = await reply
                    if not send:
                        # The caller could not build an answer: hang up
                        # rather than wait for the server to give up on us.
                        raise ConnectionAbortedError("aborted by the caller")
                    writer.write(send)
                else:
                    writer.write(base64.b64decode(msg["send"]))
                await writer.drain()
                return await _read_to_close(reader)

            data = await asyncio.wait_for(exchange(), msg.get("timeout", 120000) / 1000)
            _emit({"id": rid, "ok": True, "data": base64.b64encode(data).decode(),
                   "ms": (time.perf_counter() - started) * 1000, "retries": retries})
        except BaseException as e:  # noqa: B036 - the caller must hear of every failure
            text = "timed out" if isinstance(e, asyncio.TimeoutError) else (str(e) or type(e).__name__)
            ms = (time.perf_counter() - started) * 1000 if started is not None else 0
            _emit({"id": rid, "ok": False, "error": text, "ms": ms, "retries": retries})
            if isinstance(e, (KeyboardInterrupt, SystemExit)):
                raise
        finally:
            self.replies.pop(rid, None)
            if writer is not None:
                _reset(writer)


def main():
    # The selector loop, not the Windows default proactor: the proactor
    # transport shuts a socket down (a FIN) before closing it, so the
    # linger-0 reset in _reset() would never happen. A pump process holds a
    # few dozen sockets at most, well inside what select() takes.
    loop = asyncio.SelectorEventLoop()
    asyncio.set_event_loop(loop)
    pump = Pump(loop)
    done = loop.create_future()

    # stdin from a thread: the selector loop cannot wait on a pipe on
    # Windows.
    def read_stdin():
        for line in sys.stdin:
            if line.strip():
                loop.call_soon_threadsafe(pump.dispatch, line)
        loop.call_soon_threadsafe(lambda: done.done() or done.set_result(None))

    threading.Thread(target=read_stdin, daemon=True).start()
    _emit({"ready": True})
    loop.run_until_complete(done)


if __name__ == "__main__":
    main()
