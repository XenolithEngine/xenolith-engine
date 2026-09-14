#!/usr/bin/env python3
"""Drive the CSS overflow stand (XL_OVERFLOW_TEST) over the inspector socket - its two `hidden` boxes.

The stand asserts the overflow COMPONENTS itself: a ScrollSystem, a scroll range, a scissor with the
right axes. What none of that can say is whether the scissor reaches the pixels, and on a ui::Panel
it did not: the ScrollSystem adopted the Panel's own DynamicStateSystem - the sprite's, DoNotApply,
switched off again whenever the image is placed - so the oversized child was drawn in full beside
the box while every component read as clipped. A table cell is a Panel; the component editor's long
descriptions ran across the next column and into the rail beside the editor.

  * INSIDE the box the child is drawn (the red it is painted), on a Layer box and on a Panel box;
  * to the RIGHT of the box and BELOW it, where the 400x300 child would reach, it is not.

The Layer box is the control: it was always clipped, so a failure on the Panel alone is the defect
and not the way this script reads a screenshot.

    tests/window/overflow-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, socket, struct, subprocess, sys, time, zlib

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-overflow-check.sock")

RED = (244, 67, 54)  # Color::Red_500


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


def read_png(raw):
    """Minimal PNG reader — 8-bit RGB/RGBA, non-interlaced, which is what the inspector writes.

    What a clip cuts off is a claim about pixels: every component on the box reads correctly whether
    or not the scissor reaches its children.
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


def start_app(binary):
    env = dict(os.environ)
    env["XL_OVERFLOW_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1280", "--height", "768"],
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


def shot():
    s.ok("frame", count=1)
    time.sleep(0.06)
    data = s.ok("screenshot")["data"]
    blob = data[7:]
    return read_png(base64.urlsafe_b64decode(blob + "=" * (-len(blob) % 4)))


def at(image, x, y):
    w, h, rows = image
    px, py = int(x), int(h - y)
    return rows[py][px] if 0 <= px < w and 0 <= py < h else None


try:
    s.ok("render")
    for _ in range(6):
        s.ok("frame", count=1)
        time.sleep(0.06)

    st = s.invoke("overflow.state")
    image = shot()

    for key, what in (("hiddenLayer", "a Layer"), ("hiddenPanel", "a ui::Panel")):
        box = st[key]
        print(f"== `overflow: hidden` on {what} ==")
        inside = at(image, box["x"] + box["width"] * 0.5, box["y"] + box["height"] * 0.5)
        check(f"{what}: the oversized child is drawn inside the box", near(inside, RED),
              (inside, box))
        # the child starts at the box's top-left and is 400x300 in a 220x120 box: it reaches past
        # both the right edge and the bottom one
        right = at(image, box["x"] + box["width"] + 10, box["y"] + box["height"] - 20)
        check(f"{what}: ... and cut off at the right edge", right is not None and not near(right, RED),
              (right, box))
        below = at(image, box["x"] + 20, box["y"] - 20)
        check(f"{what}: ... and at the bottom edge", below is not None and not near(below, RED),
              (below, box))
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
