#!/usr/bin/env python3
"""M1 acceptance check for the GLES backend: headless clear + readback.

GLES_M1_PLAN.md §4.3: launch `headlesstest --headless -W 640 -H 480 --gapi gles` with an
inspector socket, send `frame` + `screenshot`, decode the PNG (pure zlib/struct, no
dependencies), verify the frame is uniform white — with the scene's drawables hidden
(`show={all:false}`, M2), the flat queue renders nothing but the background, so the last
presented image must be exactly the scene's background colour. Then `quit` and check the
exit code. Prints "N checks, M failures"; exit status is the result.

    tests/headless/gles-clear-check.py [path-to-headlesstest]

The binary must have been built with GLES=1 (see the Makefile). With no argument it expects
the debug x86_64-linux build in place; nothing is built here.

Since M2 the binary runs the flat queue, so the script hides every scene drawable first:
`show={"layer":false,"sprite":false,"vector":false,"label":false}` — otherwise the sprite,
vector figure and box would be in frame and the "uniform white" assertion would fail by
design.

Expected one-off log noise, per GLES_M1_PLAN.md §4.3 — these are NOT failures:
  * «Fail to initialize with queue» (XL2dFrameContext.cc) - FrameContext2d wants the four
    standard basic2d attachments, which the clear-only queue does not declare; draw commands
    are dropped by design in M1, so the scene context is never needed and presentation still
    runs through the pass handle. (GLES builds a real flat queue since M2, so this line no
    longer appears there; it stays listed for the soft/clear-only reference run.)

Dependency-free: standard library only (socket/json/struct/zlib/base64).
"""
import base64
import json
import os
import socket
import struct
import subprocess
import sys
import time
import zlib

ROOT = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BINARY = os.path.join(ROOT, "stappler-build/x86_64-unknown-linux-gnu/debug/cc/headlesstest")
ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-gles-clear.sock")
APP_LOG = ADDR + ".app.log"

WIDTH = 640
HEIGHT = 480

CHECKS = 0
FAIL = []


def expect(cond, what, extra=""):
    global CHECKS
    CHECKS += 1
    if not cond:
        FAIL.append("%s %s" % (what, extra))
        print("  FAIL  %s %s" % (what, extra))
    else:
        print("  ok    %s" % what)


class Session:
    """Framed inspector protocol; the same client xlclient.py speaks."""

    def __init__(self, path=ADDR, timeout=15.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.buf = b""
        self.serial = 0
        self.s.sendall(b"xenolith/1 json\n")
        line = self._read_line()
        if not line.startswith("# xenolith/1 ok"):
            raise OSError("unexpected handshake reply: %r" % line)

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass

    def _fill(self):
        chunk = self.s.recv(65536)
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
        if size > 64 * 1024 * 1024:
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
        self.s.sendall(struct.pack("<I", len(payload)) + payload)
        # replies are correlated by serial: a slow screenshot may be overtaken
        while True:
            response = self._read_frame()
            if response.get("serial") == self.serial:
                if response.get("status") != "ok":
                    raise OSError("%s failed: %s" % (cmd, response.get("error", "unknown error")))
                return response.get("result")


def decode_bytes(value):
    """data::Value encodes Bytes into JSON as \"BASE64:<base64url, unpadded>\"."""
    if value.startswith("BASE64:"):
        value = value[len("BASE64:"):]
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def decode_png(data):
    """Decode an 8-bit RGB/RGBA PNG into (width, height, pixels, channels)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG (bad signature)")

    width = height = bitdepth = colortype = None
    idat = b""
    pos = 8
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(">IIBB", payload[:10])
        elif ctype == b"IDAT":
            idat += payload
        elif ctype == b"IEND":
            break
        pos += 12 + length

    if width is None:
        raise ValueError("no IHDR chunk")
    if bitdepth != 8 or colortype not in (2, 6):
        raise ValueError("unsupported PNG: bit depth %d, color type %d" % (bitdepth, colortype))
    channels = 3 if colortype == 2 else 4

    raw = zlib.decompress(idat)
    stride = width * channels
    need = height * (stride + 1)
    if len(raw) < need:
        raise ValueError("raw scanlines too short: %d < %d" % (len(raw), need))

    out = bytearray(width * height * channels)
    prev = bytearray(stride)
    i = 0
    for _ in range(height):
        filter_type = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        if filter_type == 1:  # Sub
            for x in range(channels, stride):
                line[x] = (line[x] + line[x - channels]) & 0xFF
        elif filter_type == 2:  # Up
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif filter_type == 3:  # Average
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif filter_type == 4:  # Paeth
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                c = prev[x - channels] if x >= channels else 0
                line[x] = (line[x] + _paeth(a, prev[x], c)) & 0xFF
        elif filter_type != 0:
            raise ValueError("unknown PNG filter type %d" % filter_type)
        out[_ * stride:(_ + 1) * stride] = line
        prev = line
        i += stride + 1

    return width, height, bytes(out), channels


def start_app(binary):
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    log = open(APP_LOG, "wb")
    # --width/--height: the engine's option parser does not know single-dash -W/-H forms.
    return subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height", str(HEIGHT),
                             "--gapi", "gles"], env=env, stdout=log, stderr=subprocess.STDOUT)


def connect():
    deadline = time.monotonic() + 60.0
    last = None
    while time.monotonic() < deadline:
        try:
            return Session(timeout=20.0)
        except OSError as e:
            last = e
            time.sleep(0.5)
    raise SystemExit("app never came up: %s (log: %s)" % (last, APP_LOG))


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY
    if not os.path.exists(binary):
        raise SystemExit("binary not found: %s\nbuild it with: make -C %s GLES=1" % (binary, ROOT))

    app = start_app(binary)
    session = None
    try:
        # The engine can die while starting up (bad GL stack); give it a moment to crash so the
        # connect loop below does not wait out its full deadline against a dead socket.
        time.sleep(0.5)
        if app.poll() is not None:
            expect(False, "the app stayed alive",
                   "(exited early with code %s; log: %s)" % (app.returncode, APP_LOG))
            return

        session = connect()
        expect(True, "inspector accepted the connection")

        # Since M2 the binary runs the flat queue with a real scene; hide every drawable so the
        # "uniform white" assertion below checks exactly what M1 checked - background only.
        session.call("invoke", name="show",
                     args={"layer": False, "sprite": False, "vector": False, "label": False})

        # Headless renders on demand and `screenshot` reads back the LAST presented image, which
        # trails the frames just submitted (xlclient.py measured a few-frame lag). Burn several
        # frames first, then settle on two consecutive identical captures before asserting.
        #
        # Equality alone is not the signal: until the first frame carrying the `show` above
        # reaches the swapchain, consecutive captures agree on the *stale* image - the scene with
        # its drawables still visible - and the assertion below then fails on content that is no
        # longer being drawn. So burn MIN_ROUNDS rounds before trusting equality, the same guard
        # xlclient.py applies with --min-rounds.
        MIN_ROUNDS = 6
        previous = None
        info = data = None
        settled = False
        for round_index in range(10):
            session.call("frame", count=4)
            info = session.call("screenshot")
            data = decode_bytes(info["data"])
            if data == previous and round_index + 1 >= MIN_ROUNDS:
                settled = True
                break
            previous = data

        expect(settled, "the capture settled to a stable image")
        expect(info.get("width") == WIDTH and info.get("height") == HEIGHT,
               "screenshot is %dx%d" % (WIDTH, HEIGHT),
               "(got %sx%s)" % (info.get("width"), info.get("height")))

        try:
            width, height, pixels, channels = decode_png(data)
        except ValueError as e:
            expect(False, "the screenshot decodes as a PNG", str(e))
            return

        expect(width == WIDTH and height == HEIGHT,
               "PNG dimensions are %dx%d" % (WIDTH, HEIGHT), "(got %sx%s)" % (width, height))
        expect(len(pixels) == width * height * channels,
               "PNG pixel count matches its header",
               "(%d bytes for %dx%d x %d channels)" % (len(pixels), width, height, channels))

        # M1 draws nothing but the clear: every channel of every pixel must be the scene's white.
        all_white = pixels == b"\xFF" * len(pixels)
        if not all_white and pixels:
            first_bad = next(i for i in range(len(pixels)) if pixels[i] != 0xFF)
            expect(False, "the frame is uniform white (M1 clear pass only)",
                   "(first non-255 byte at offset %d; channels=%s)" % (first_bad, channels))
        else:
            expect(all_white, "the frame is uniform white (M1 clear pass only)")

        # call() raises on a non-ok status, so reaching here means quit was accepted.
        session.call("quit", graceful=True)
        expect(True, "quit was accepted by the inspector")

        code = app.wait(timeout=15)
        expect(code == 0, "the app exited cleanly after quit",
               "(exit code %s; log: %s)" % (code, APP_LOG))
    finally:
        if session is not None:
            try:
                session.close()
            except OSError:
                pass
        # Guarantee no orphaned engine on any early-return path.
        if app.poll() is None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()

    print("\nSUMMARY: %d checks, %d failures" % (CHECKS, len(FAIL)))
    for f in FAIL:
        print("  -", f)
    raise SystemExit(1 if FAIL else 0)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
