#!/usr/bin/env python3
"""Drive the floating-window stand (XL_FLOATING_TEST) over the inspector socket.

ui::FloatingSystem makes a node a window inside the scene. Checked here:

  * a drag on the header moves it by exactly the pointer's travel, and a click on a button in the
    header is still a click;
  * each of the eight borders resizes it, the opposite border staying where it was, and never
    below the minimum;
  * it stays within the scene however far it is thrown;
  * it is drawn above an opaque ui::Panel - a Surface, drawn by material, which only the overlay
    pass is guaranteed to cover;
  * where it is drawn, nothing below it is tapped, scrolled, hovered or hinted, while its own
    content is tapped and scrolled.

    tests/window/floating-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, socket, struct, subprocess, sys, time, zlib

MIN_W, MIN_H = 160.0, 100.0


class Session:
    def __init__(self, path, timeout=25.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
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


def start_app(binary, addr):
    env = dict(os.environ)
    env["XL_FLOATING_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(addr):
            try:
                Session(addr).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")
addr = f"/tmp/xl-floating-{os.getpid()}.sock"

proc = start_app(binary, addr)
s = Session(addr)


def step(n=2):
    s.ok("frame", count=n)
    time.sleep(0.03)


def state():
    return s.invoke("floating.state")


def centre(r, fx=0.5, fy=0.5):
    return r["x"] + r["width"] * fx, r["y"] + r["height"] * fy


def send(events):
    s.ok("input", native=True, events=events)
    step(2)


def drag(a, b, steps=8):
    ev = [{"event": "Begin", "x": a[0], "y": a[1], "button": "MouseLeft"}]
    for i in range(1, steps + 1):
        t = i / steps
        ev.append({"event": "Move", "x": a[0] + (b[0] - a[0]) * t, "y": a[1] + (b[1] - a[1]) * t,
                   "button": "MouseLeft"})
    ev.append({"event": "End", "x": b[0], "y": b[1], "button": "MouseLeft"})
    send(ev)


def click(p):
    ev = {"x": p[0], "y": p[1], "button": "MouseLeft"}
    send([dict(ev, event="Begin"), dict(ev, event="End")])


def scroll(p, amount=-40.0):
    send([{"event": "Scroll", "x": p[0], "y": p[1], "button": "None", "valueY": amount}])


def hover(p):
    send([{"event": "MouseMove", "x": p[0], "y": p[1], "button": "None"}])


def pixel(p):
    step(1)
    data = s.ok("screenshot")["data"]
    blob = data[7:]
    w, h, rows = read_png(base64.urlsafe_b64decode(blob + "=" * (-len(blob) % 4)))
    x, y = int(p[0]), int(h - p[1])
    return rows[y][x] if 0 <= x < w and 0 <= y < h else None


def frame():
    f = state()["frame"]
    return (f["x"], f["y"], f["width"], f["height"])


def near(a, b, eps=0.51):
    return all(abs(x - y) <= eps for x, y in zip(a, b))


try:
    s.ok("render")
    step(6)

    print("the window is there")
    st = state()
    check("it is drawn in the overlay pass", st["overlay"] is True, st)
    check("it has eight borders", sum(1 for k in st["rects"] if k.startswith("floating-resize-")) == 8,
            sorted(st["rects"]))
    start = frame()
    check("at the frame it was given", near(start, (200.0, 200.0, 360.0, 240.0)), start)

    print("the header moves it")
    s.invoke("floating.reset")
    hb = st["rects"]["header"]
    a = centre(hb, 0.6, 0.5)
    drag(a, (a[0] + 50.0, a[1] - 30.0))
    moved = frame()
    check("by exactly the pointer's travel", near(moved, (250.0, 170.0, 360.0, 240.0)), moved)
    check("and says so once, when the drag ends", state()["frameChanges"] == 1, state())
    check("the drag reached nothing below the window", state()["belowTaps"] == 0, state())

    btn = state()["rects"]["header-button"]
    click(centre(btn))
    st = state()
    check("a click on a button in the header is a click", st["headerTaps"] == 1, st)
    check("and moves nothing", near(frame(), moved), frame())

    print("the borders resize it")
    names = {
        "floating-resize-right": (20.0, 0.0, lambda f0, f: near(f, (f0[0], f0[1], f0[2] + 20.0, f0[3]))),
        "floating-resize-left": (-20.0, 0.0, lambda f0, f: near(f, (f0[0] - 20.0, f0[1], f0[2] + 20.0, f0[3]))),
        "floating-resize-top": (0.0, 20.0, lambda f0, f: near(f, (f0[0], f0[1], f0[2], f0[3] + 20.0))),
        "floating-resize-bottom": (0.0, -20.0, lambda f0, f: near(f, (f0[0], f0[1] - 20.0, f0[2], f0[3] + 20.0))),
        "floating-resize-top-right": (15.0, 15.0, lambda f0, f: near(f, (f0[0], f0[1], f0[2] + 15.0, f0[3] + 15.0))),
        "floating-resize-top-left": (-15.0, 15.0, lambda f0, f: near(f, (f0[0] - 15.0, f0[1], f0[2] + 15.0, f0[3] + 15.0))),
        "floating-resize-bottom-right": (15.0, -15.0, lambda f0, f: near(f, (f0[0], f0[1] - 15.0, f0[2] + 15.0, f0[3] + 15.0))),
        "floating-resize-bottom-left": (-15.0, -15.0, lambda f0, f: near(f, (f0[0] - 15.0, f0[1] - 15.0, f0[2] + 15.0, f0[3] + 15.0))),
    }
    for name, (dx, dy, ok) in names.items():
        s.invoke("floating.set-frame", x=300.0, y=250.0, width=360.0, height=240.0)
        step(2)
        f0 = frame()
        r = state()["rects"][name]
        p = centre(r)
        drag(p, (p[0] + dx, p[1] + dy))
        f = frame()
        check(f"{name} moves its own borders only", ok(f0, f), (f0, f))

    s.invoke("floating.set-frame", x=300.0, y=250.0, width=360.0, height=240.0)
    step(2)
    r = state()["rects"]["floating-resize-right"]
    p = centre(r)
    drag(p, (p[0] - 600.0, p[1]))
    f = frame()
    check("a border stops at the minimum size, the other one staying put",
            near(f, (300.0, 250.0, MIN_W, 240.0)), f)

    print("it stays within the scene")
    st = state()
    s.invoke("floating.set-frame", x=300.0, y=250.0, width=360.0, height=240.0)
    step(2)
    hb = state()["rects"]["header"]
    a = centre(hb, 0.6, 0.5)
    drag(a, (a[0] + 2000.0, a[1] + 2000.0), steps=12)
    f = frame()
    check("thrown past the corner it stops at the corner",
            near(f, (st["areaWidth"] - 360.0, st["areaHeight"] - 240.0, 360.0, 240.0)), (f, st["areaWidth"], st["areaHeight"]))
    s.invoke("floating.set-frame", x=-500.0, y=-500.0, width=5000.0, height=5000.0)
    step(2)
    f = frame()
    check("a frame larger than the scene is cut to it",
            near(f, (0.0, 0.0, st["areaWidth"], st["areaHeight"])), f)

    print("it is drawn over an opaque Panel")
    s.invoke("floating.set-frame", x=200.0, y=200.0, width=360.0, height=240.0)
    step(3)
    st = state()
    plate = st["rects"]["plate"]
    win = st["rects"]["window"]
    # a point of the window's own border strip, over the plate and outside the content
    p = (win["x"] + 4.0, win["y"] + 4.0)
    check("the probe is over the plate", plate["x"] < p[0] < plate["x"] + plate["width"]
            and plate["y"] < p[1] < plate["y"] + plate["height"], (plate, win))
    px = pixel(p)
    check("and shows the window's red, not the plate", px is not None and px[0] > 180 and px[1] < 120
            and px[2] < 120, px)

    print("nothing below it answers where it is drawn")
    s.invoke("floating.reset")
    body = (win["x"] + 4.0, win["y"] + win["height"] * 0.5)
    inside = centre(st["rects"]["inside"])
    outside = (win["x"] + win["width"] + 60.0, win["y"] + win["height"] * 0.5)
    click(body)
    check("a click on the window's body does not reach the background", state()["belowTaps"] == 0,
            state())
    click(outside)
    check("a click beside it does", state()["belowTaps"] == 1, state())
    click(inside)
    st = state()
    check("the content is clicked", st["insideTaps"] == 1, st)
    check("... and the background is not", st["belowTaps"] == 1, st)
    scroll(inside)
    st = state()
    check("the wheel over the content turns the content", st["insideScrolls"] >= 1, st)
    check("... and not the background", st["belowScrolls"] == 0, st)
    scroll(body)
    check("the wheel over the window's body does not reach the background",
            state()["belowScrolls"] == 0, state())

    hover(outside)
    step(2)
    check("the background is hovered beside the window", state()["belowHovered"] is True, state())
    hover(body)
    step(2)
    st = state()
    check("and stops being hovered once the pointer is over the window",
            st["belowHovered"] is False, st)
    check("its hint is not resolved through the window", st.get("tipHovered", "") != "below",
            st.get("tipHovered"))
finally:
    try:
        s.ok("quit")
    except Exception:
        pass
    s.close()
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
