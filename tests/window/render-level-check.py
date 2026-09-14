#!/usr/bin/env python3
"""Drive the RenderingLevel stand (XL_RENDER_LEVEL_TEST) over the inspector socket - its row 4.

Rows 1-3 of the stand are read by eye. Row 4 is the case the 2d vertex plan drew WRONG, and it is a
claim about pixels and about which pass a command went to, so it is checked here:

  * A SURFACE BOX IN FRONT OF TRANSPARENT GEOMETRY IS DRAWN IN FRONT OF IT. Surface blends and does
    not write depth, and the transparent pass is drawn after the surface one - so the translucent
    strip behind the box used to paint over it. A label over a picture with an alpha channel
    vanished that way (the Fibonacci game window in xenolith-studio).
  * ONLY A SURFACE THE TRANSPARENT GEOMETRY ACTUALLY COVERS LEAVES THE SURFACE PASS. The second box
    is in front of the strip by zPath too, but beside it: the frame's `surfacePromotedCmds` must not
    count it. Moved over the strip it must be counted, and moved back it must not be again - which
    is what says the test is the overlap and not the zPath alone.

    tests/window/render-level-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, socket, struct, subprocess, sys, time, zlib

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-render-level-check.sock")

AMBER = (255, 193, 7)  # Color::Amber_500
BLUE = (13, 71, 161)  # Color::Blue_900


class Session:
    def __init__(self, path, timeout=25.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
        # the greeting is a LINE and comes before any frame; a client that starts framing at once
        # eats it as a length and then blocks forever
        line = b""
        while not line.endswith(b"\n"):
            line += self.s.recv(1)
        assert line.startswith(b"# xenolith/1 ok"), line
        self.serial = 0
        self.buf = b""

    def call(self, cmd, **kw):
        self.serial += 1
        req = {"serial": self.serial, "cmd": cmd}
        req.update(kw)
        payload = json.dumps(req).encode()
        self.s.sendall(struct.pack("<I", len(payload)) + payload)
        while True:
            while len(self.buf) < 4:
                chunk = self.s.recv(65536)
                if not chunk:
                    raise SystemExit("the app closed the connection - it crashed")
                self.buf += chunk
            size = struct.unpack("<I", self.buf[:4])[0]
            while len(self.buf) < 4 + size:
                chunk = self.s.recv(65536)
                if not chunk:
                    raise SystemExit("the app closed the connection - it crashed")
                self.buf += chunk
            frame = self.buf[4:4 + size]
            self.buf = self.buf[4 + size:]
            resp = json.loads(frame)
            if resp.get("serial") == self.serial:
                return resp

    def ok(self, cmd, **kw):
        r = self.call(cmd, **kw)
        if r.get("status") != "ok":
            raise SystemExit(f"{cmd} failed: {r.get('error')}")
        return r.get("result")

    def invoke(self, name, **args):
        return self.ok("invoke", name=name, args=args)

    def close(self):
        self.s.close()


checks = 0
failures = 0


def check(name, ok, detail=""):
    global checks, failures
    checks += 1
    if ok:
        print(f"  ok   {name}")
    else:
        failures += 1
        print(f"  FAIL {name} {detail}")


def near(a, b, eps=0.01):
    return abs(a - b) <= eps


def classes(paint):
    # an empty class set comes back as a null Value, not as an empty array
    return paint.get("classes") or []


def read_png(raw):
    """Minimal PNG reader — 8-bit RGB/RGBA, non-interlaced, which is what the inspector writes.

    Worth the thirty lines: whether the bar is PAINTED is the one claim about it that no amount of
    node state can answer, and the one that was wrong. Every field read correctly — size, position,
    opacity, colour, the resolved fill — while the thumb was multiplied to nothing by the opacity of
    the track it sits inside, so the bar was invisible except while the pointer was on it.
    Returns (width, height, pixels) with pixels[y][x] = (r, g, b).
    """
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, width, height, channels = 8, b"", 0, 0, 4
    while pos < len(raw):
        length = struct.unpack(">I", raw[pos:pos + 4])[0]
        kind = raw[pos + 4:pos + 8]
        body = raw[pos + 8:pos + 8 + length]
        pos += 12 + length  # length + type + data + crc
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and color in (2, 6), (depth, color)
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    data = zlib.decompress(idat)
    stride = width * channels
    rows, prev, at = [], bytearray(stride), 0
    for _ in range(height):
        filt = data[at]
        line = bytearray(data[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if filt == 1:
                line[i] = (line[i] + a) & 0xFF
            elif filt == 2:
                line[i] = (line[i] + b) & 0xFF
            elif filt == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append([tuple(line[x * channels:x * channels + 3]) for x in range(width)])
        prev = line
    return width, height, rows



checks = 0
failures = 0


def check(name, ok, detail=""):
    global checks, failures
    checks += 1
    if ok:
        print(f"  ok   {name}")
    else:
        failures += 1
        print(f"  FAIL {name} {detail}")


def start_app(binary):
    env = dict(os.environ)
    env["XL_RENDER_LEVEL_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(ADDR):
            try:
                Session(ADDR).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


def near(px, rgb, tol=40):
    return px is not None and all(abs(a - b) <= tol for a, b in zip(px, rgb))


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session(ADDR)


def step(n=3):
    # headless renders on demand, and the frame's statistics describe the frame BEFORE the last one
    for _ in range(n):
        s.ok("frame", count=1)
        time.sleep(0.06)


def state():
    return s.invoke("render-level.state")


def pixel(box):
    s.ok("frame", count=1)
    time.sleep(0.06)
    data = s.ok("screenshot")["data"]
    blob = data[7:]
    w, h, rows = read_png(base64.urlsafe_b64decode(blob + "=" * (-len(blob) % 4)))
    x, y = int(box["x"]), int(h - box["y"])
    return rows[y][x] if 0 <= x < w and 0 <= y < h else None


try:
    s.ok("render")
    step(6)

    print("== the stand is what this check was written for ==")
    st = state()
    check("the strip is transparent and both boxes are surfaces",
          st["strip"]["level"] == "transparent" and st["over"]["level"] == "surface"
          and st["apart"]["level"] == "surface", st)

    print("== a surface in front of transparent geometry is drawn in front of it ==")
    over = pixel(st["over"])
    check("the box over the strip shows its own colour, not the strip's", near(over, AMBER),
          (over, st["over"]))
    apart = pixel(st["apart"])
    check("... and so does the box beside it", near(apart, AMBER), (apart, st["apart"]))

    print("== only the surface the strip actually covers leaves the surface pass ==")
    base = st["surfacePromoted"]
    check("the box over the strip is counted", base >= 1, st)

    st = s.invoke("render-level.apart", overlap=True)
    step(6)
    st = state()
    check("moved over the strip, the second box is counted too", st["surfacePromoted"] == base + 1,
          (base, st))
    moved = pixel(st["apart"])
    check("... and is still drawn in front of it", near(moved, AMBER), (moved, st["apart"]))

    s.invoke("render-level.apart", overlap=False)
    step(6)
    st = state()
    check("moved clear of it again, it is not - the same zPath, a different place",
          st["surfacePromoted"] == base, (base, st))
finally:
    try:
        s.close()
    finally:
        proc.kill()
        try:
            os.unlink(ADDR)
        except OSError:
            pass

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
