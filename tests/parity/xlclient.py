#!/usr/bin/env python3
"""
Minimal inspector-socket client for the backend-parity harness.

Connects to a running Xenolith app's inspector listener, runs a scripted sequence
against it, and writes the resulting frame to a PNG. This is the same framed
protocol the MCP server speaks (.claude/mcp/scene-inspector/server.py) reduced to
the four commands a parity run needs: invoke, frame, screenshot, quit.

The order is fixed and it matters: every --invoke runs first, then frames are
rendered, and only then is the screenshot taken. Headless renders on demand *and*
`screenshot` reads back the last presented image, which trails the frames just
submitted - measured on this engine, a scene change needs four frames before it
shows up in a capture, not the one the docs suggest. Rather than hard-code that
depth, the capture repeats until two consecutive screenshots come back
byte-identical. That is only sound for a still scene: this scene has no animation
and no FPS counter, so it settles.

Usage:
  xlclient.py --address unix:/tmp/xl.sock --out shot.png [options]
    --invoke NAME=JSON   run a scene command (repeatable, applied in order);
                         the pseudo-step "@frame[=N]" renders N frames in place
    --frames N           frames to render per settle round (default 2)
    --min-rounds N       rounds to render before trusting stability (default 6)
    --rounds N           give up after N rounds without a stable capture (default 8)
    --wait SECONDS       wait up to SECONDS for the listener to accept (default 20)
    --quit               shut the app down after capturing
    --timeout SECONDS    per-operation socket timeout (default 30)

Dependency-free: standard library only.
"""

import argparse
import base64
import json
import os
import socket
import struct
import sys
import time

MAX_FRAME_SIZE = 64 * 1024 * 1024


def connect(address, timeout):
    if address.startswith("unix:"):
        path = address[len("unix:"):]
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(timeout)
        # a leading @ selects the Linux abstract namespace, which Python spells as a
        # leading NUL byte
        s.connect("\0" + path[1:] if path.startswith("@") else path)
        return s

    host, _, port = address.rpartition(":")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((host or "127.0.0.1", int(port)))
    return s


class Session:
    def __init__(self, address, timeout):
        self.sock = connect(address, timeout)
        self.buf = b""
        self.serial = 0
        self.sock.sendall(b"xenolith/1 json\n")
        greeting = self._read_line()
        if not greeting.startswith("# xenolith/1 ok"):
            raise OSError("unexpected handshake reply: %r" % greeting)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def _fill(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise OSError("inspector closed the connection")
        self.buf += chunk

    def _read_line(self):
        while b"\n" not in self.buf:
            self._fill()
        line, _, self.buf = self.buf.partition(b"\n")
        return line.decode("utf-8", "replace")

    def _read_frame(self):
        while len(self.buf) < 4:
            self._fill()
        size = struct.unpack("<I", self.buf[:4])[0]
        if size > MAX_FRAME_SIZE:
            raise OSError("frame too large: %d" % size)
        while len(self.buf) < 4 + size:
            self._fill()
        payload = self.buf[4:4 + size]
        self.buf = self.buf[4 + size:]
        return json.loads(payload.decode("utf-8"))

    def call(self, cmd, **args):
        self.serial += 1
        request = dict(args)
        request["serial"] = self.serial
        request["cmd"] = cmd
        payload = json.dumps(request).encode("utf-8")
        self.sock.sendall(struct.pack("<I", len(payload)) + payload)
        # replies are correlated by serial: a slow screenshot may be overtaken
        while True:
            response = self._read_frame()
            if response.get("serial") == self.serial:
                if response.get("status") != "ok":
                    raise OSError("%s failed: %s" % (cmd, response.get("error", "unknown error")))
                return response.get("result")


def decode_bytes(value):
    """data::Value encodes Bytes into JSON as "BASE64:<base64url, unpadded>"."""
    if value.startswith("BASE64:"):
        value = value[len("BASE64:"):]
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


def open_session(address, wait, timeout):
    """The app is launched concurrently with us, so the listener may not be bound yet."""
    deadline = time.monotonic() + wait
    last = None
    while True:
        try:
            return Session(address, timeout)
        except (OSError, ValueError) as e:
            last = e
            if time.monotonic() >= deadline:
                raise OSError("could not reach %s within %gs: %s" % (address, wait, last))
            time.sleep(0.2)


def main():
    parser = argparse.ArgumentParser(add_help=True, description=__doc__)
    parser.add_argument("--address", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--invoke", action="append", default=[], metavar="NAME=JSON")
    parser.add_argument("--frames", type=int, default=2)
    parser.add_argument("--rounds", type=int, default=8)
    # A content change that needs new glyphs reaches the swapchain a few frames late, and until it
    # does, consecutive captures agree on the *previous* image - so "stable" alone is not a signal.
    # Six rounds is a wide margin over the two frames a change was measured to need.
    parser.add_argument("--min-rounds", type=int, default=6)
    parser.add_argument("--wait", type=float, default=20.0)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--quit", action="store_true")
    args = parser.parse_args()

    session = open_session(args.address, args.wait, args.timeout)
    try:
        for spec in args.invoke:
            name, _, payload = spec.partition("=")
            # "@frame" is not a scene command: it renders in the middle of the script, so a
            # case can check what happens *between* two states (a glyph cache growing, say)
            # instead of only their sum.
            if name == "@frame":
                session.call("frame", count=int(payload) if payload else args.frames)
                continue
            session.call("invoke", name=name, args=json.loads(payload) if payload else {})

        previous = None
        info = None
        data = None
        for round_index in range(args.rounds):
            if args.frames > 0:
                session.call("frame", count=args.frames)
            info = session.call("screenshot")
            data = decode_bytes(info["data"])
            # "two captures in a row agree" is not on its own enough: before the first
            # frame reflecting the invokes reaches the swapchain, consecutive captures
            # agree on the *stale* image. Burn --min-rounds first, then trust equality.
            if data == previous and round_index + 1 >= args.min_rounds:
                break
            previous = data
        else:
            raise OSError("capture did not settle in %d rounds (is the scene animating?)"
                    % args.rounds)

        with open(args.out, "wb") as f:
            f.write(data)
        print("%s %dx%d %d bytes, settled after %d round(s)"
                % (args.out, info["width"], info["height"], len(data), round_index + 1))

        if args.quit:
            session.call("quit", graceful=True)
    finally:
        session.close()

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print("xlclient.py: %s" % e, file=sys.stderr)
        sys.exit(1)
