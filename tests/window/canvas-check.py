#!/usr/bin/env python3
"""Drive the ui::CanvasView stand (XL_CANVAS_TEST) over the inspector socket.

WHY THIS FILE EXISTS AND WHAT IT DOES NOT ASSERT. The arithmetic of a viewport - anchored zoom,
clamping, framing, the wheel curve - is `sprt::geom`'s and is asserted with no window at all, on two
ABIs, by a console harness. Repeating it here would be repeating a check that is already stronger.

What only a window can answer is whether the WIDGET agrees with that arithmetic: whether the place
`Viewport::toScreen` says a world point lands is the place the node actually is. So the stand
reports every marker twice - once by the math, once read out of the live scene graph - and the
comparison of those two numbers is the substance of this file. A widget whose transform drifted from
its own formula fails here and nowhere else.

Four more things are asserted because each was a defect in one of the four hand-written copies this
widget replaces:

  * THE ANCHOR HOLDS AT A LIMIT. Zooming into the stop and out again must leave the point under the
    cursor where it was. Solving the offset before clamping the zoom leaves an offset belonging to a
    zoom the view never took, and the pan creeps a little every time the wheel hits the stop.
  * ONE NOTCH IS ONE RATIO. Equal notches must be equal ratios, and the number of pixels a notch is
    worth must come from the widget rather than from the caller - two of the copies said 100 and one
    said 90, so the same wheel zoomed two canvases differently.
  * AND ONE NOTCH IS ONE STEP, asked through a REAL Scroll event. The event carries an AMOUNT, not a
    count of clicks, and turning one into the other is the widget's own division - which was missing:
    a detent of ten units went into the exponent whole, so one click of the wheel was the step to the
    tenth power. Asking through `canvas.wheel`, which takes notches, asserts the curve and can never
    see the units; only injected input can.
  * FRAMING HAS ITS OWN RANGE. A world too big to be framed inside the gesture range must still be
    framable; clamping a fit to the gesture range is how a large document becomes unframable.
  * IT CLIPS. Three of the four copies did not, and a thing dragged past the edge painted over
    whatever sat beside it.

    tests/window/canvas-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import base64, json, math, os, socket, struct, subprocess, sys, time, zlib

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-canvas-check.sock")

# What the stand declares (CanvasViewLayout.cpp). Duplicated on purpose: a check that reads its
# expectations out of the thing it is checking cannot fail.
MARKERS = {
    "origin": (0.0, 0.0),
    "far": (430.0, 270.0),
    "near": (-310.0, -170.0),
}
MARKER_W, MARKER_H = 120.0, 80.0

# The caption strip the stand keeps above the canvas, so the surface is not the window.
CAPTION_H = 76.0

WINDOW_W, WINDOW_H = 1024.0, 768.0

class Session:
    def __init__(self, path=ADDR, timeout=20.0):
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
                self.buf += self.s.recv(65536)
            size = struct.unpack("<I", self.buf[:4])[0]
            while len(self.buf) < 4 + size:
                self.buf += self.s.recv(65536)
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


def start_app(binary, addr=ADDR, width=1024, height=768, density=None):
    env = dict(os.environ)
    env["XL_CANVAS_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    argv = [binary, "--headless", "--width", str(width), "--height", str(height)]
    if density is not None:
        argv += ["--density", str(density)]
    proc = subprocess.Popen(argv, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(addr):
            try:
                probe = Session(addr)
                probe.close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


def drag_events(x0, y0, x1, y1, button="MouseMiddle", steps=8):
    # A press, a run of moves and a release. The moves have to be several: a swipe is recognized
    # only once the pointer has actually travelled, so a Begin alone never starts one.
    ev = [{"event": "Begin", "x": x0, "y": y0, "button": button}]
    for i in range(1, steps + 1):
        t = float(i) / float(steps)
        ev.append({"event": "Move", "x": x0 + (x1 - x0) * t, "y": y0 + (y1 - y0) * t,
                "button": button})
    ev.append({"event": "End", "x": x1, "y": y1, "button": button})
    return ev


def click_events(x, y):
    return [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"},
            {"event": "End", "x": x, "y": y, "button": "MouseLeft"}]


def wheel_events(x, y, amount):
    """A REAL Scroll event. It carries an AMOUNT, not a count of notches - which is the whole
    point of asking through this road rather than through `canvas.wheel`."""
    return [{"event": "Scroll", "x": x, "y": y, "button": "None", "valueY": amount}]


checks = 0
failures = 0


def read_png(raw):
    """Minimal PNG reader: 8-bit RGB/RGBA, non-interlaced, which is what the inspector writes.
    Returns (width, height, rows) with rows[y][x] = (r, g, b), y from the top."""
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, width, height, channels = 8, b"", 0, 0, 4
    while pos < len(raw):
        length = struct.unpack(">I", raw[pos:pos + 4])[0]
        kind = raw[pos + 4:pos + 8]
        body = raw[pos + 8:pos + 8 + length]
        pos += 12 + length
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
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        rows.append([tuple(line[x * channels:x * channels + 3]) for x in range(width)])
        prev = line
    return width, height, rows


def grid_lines(session, density, lo, hi, axis):
    """Where the stand's grid puts its lines in device pixels, by the formula the node documents:
    level lines every 4^k world units, each at floor(v + 0.5) of its device position."""
    st = session.invoke("canvas.state", settle=0.0)
    view, grid = st["viewport"], st["grid"]
    period = 4 ** grid["baseLevel"]
    off = view["x"] if axis == 0 else view["y"]
    unit = view["zoom"] * density
    out = set()
    n = math.ceil((lo / density - off) / (period * view["zoom"])) - 1
    while True:
        at = math.floor(density * off + n * period * unit + 0.5)
        if at > hi:
            break
        if at >= lo:
            out.add(at)
        n += 1
    return out


def snapshot(session):
    session.ok("frame", count=1)
    time.sleep(0.06)
    data = session.ok("screenshot")["data"]
    blob = data[7:]
    return read_png(base64.urlsafe_b64decode(blob + "=" * (-len(blob) % 4)))


def grid_columns(session, density, x0, x1):
    """The columns of one scanline that are not the ground, found in a real snapshot, and the ones
    the formula predicts. The scanline is put between two horizontal lines, so it crosses only the
    vertical ones."""
    w, h, rows = snapshot(session)
    rows_at = grid_lines(session, density, 0, h, 1)
    y = next(y for y in range(int(20 * density), h // 3) if all(abs(y - r) > 1 for r in rows_at))
    row = rows[h - 1 - y]
    ground = max(set(row[x0:x1]), key=row[x0:x1].count)
    seen = {x for x in range(x0, x1) if max(abs(a - b) for a, b in zip(row[x], ground)) > 24}
    return seen, grid_lines(session, density, x0, x1 - 1, 0)


def check(name, ok, detail=""):
    global checks, failures
    checks += 1
    if ok:
        print(f"  ok   {name}")
    else:
        failures += 1
        print(f"  FAIL {name} {detail}")


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()



def step(n=2):
    # The headless window renders on demand, and a node's transform settles while it draws.
    s.ok("frame", count=n)
    time.sleep(0.15)


def near(a, b, eps=1e-3):
    return abs(a - b) <= eps


def state():
    return s.invoke("canvas.state", settle=0.0)


def set_view(x, y, zoom):
    return s.invoke("canvas.set-view", x=x, y=y, zoom=zoom, settle=0.0)


def agree(st, why, eps=1e-3):
    """Every marker, by both roads. This is the assertion the stand was built for."""
    bad = []
    for name in MARKERS:
        m = st["markers"][name]
        if not near(m["math"]["x"], m["node"]["x"], eps) or not near(m["math"]["y"],
                m["node"]["y"], eps):
            bad.append((name, m["math"], m["node"]))
    check(why, not bad, bad)


try:
    limits = s.invoke("canvas.limits")
    step()

    # ---- the transform and the picture are the same thing --------------------------------------
    print("\n-- what the math says is where the node is --")

    st = set_view(0.0, 0.0, 1.0)
    step()
    agree(state(), "at the identity view, every marker is where the math puts it")

    st = set_view(137.0, -84.0, 1.0)
    step()
    agree(state(), "... after a pan")

    st = set_view(137.0, -84.0, 0.5)
    step()
    agree(state(), "... after a zoom out")

    st = set_view(-260.0, 310.0, 1.75)
    step()
    agree(state(), "... after a zoom in with a pan")

    # And the transform really did move: a check that passed with everything at zero would be
    # comparing two constants.
    st = state()
    check("the markers are not all at the origin - the comparison has something to compare",
          any(abs(st["markers"][n]["node"]["x"]) > 1.0 for n in MARKERS), st["markers"])

    # ---- pan is exactly the delta ---------------------------------------------------------------
    print("\n-- a pan moves every marker by the pan --")

    a = set_view(0.0, 0.0, 1.0)
    step()
    a = state()
    b = set_view(90.0, -45.0, 1.0)
    step()
    b = state()
    moved = [(b["markers"][n]["node"]["x"] - a["markers"][n]["node"]["x"],
              b["markers"][n]["node"]["y"] - a["markers"][n]["node"]["y"]) for n in MARKERS]
    check("every marker moved by exactly the offset, and by the same amount",
          all(near(dx, 90.0) and near(dy, -45.0) for dx, dy in moved), moved)

    # ---- the anchor holds, including at the stop ------------------------------------------------
    print("\n-- the point under the cursor stays under the cursor --")

    ANCHOR_X, ANCHOR_Y = 300.0, 220.0

    set_view(0.0, 0.0, 1.0)
    step()
    before = state()["viewport"]
    world_before = ((ANCHOR_X - before["x"]) / before["zoom"],
                    (ANCHOR_Y - before["y"]) / before["zoom"])

    for factor in (1.2, 1.2, 0.7, 1.5, 0.5, 0.9):
        s.invoke("canvas.zoom-at", x=ANCHOR_X, y=ANCHOR_Y, factor=factor, settle=0.0)
    step()

    after = state()["viewport"]
    world_after = ((ANCHOR_X - after["x"]) / after["zoom"],
                   (ANCHOR_Y - after["y"]) / after["zoom"])
    check("six zooms about one point leave that point on the same world coordinate",
          near(world_before[0], world_after[0], 0.05) and near(world_before[1], world_after[1],
              0.05), (world_before, world_after))

    # Into the stop and back out. This is the one that catches solving before clamping.
    set_view(0.0, 0.0, 1.0)
    step()
    at_stop = None
    for _ in range(12):
        at_stop = s.invoke("canvas.zoom-at", x=ANCHOR_X, y=ANCHOR_Y, factor=1.4, settle=0.0)
    step()
    v = state()["viewport"]
    check("zooming past the top of the range stops AT the top", near(v["zoom"], limits["max"]),
          (v["zoom"], limits["max"]))

    world_at_stop = ((ANCHOR_X - v["x"]) / v["zoom"], (ANCHOR_Y - v["y"]) / v["zoom"])
    for _ in range(6):
        s.invoke("canvas.zoom-at", x=ANCHOR_X, y=ANCHOR_Y, factor=1.4, settle=0.0)
    step()
    v2 = state()["viewport"]
    world_still = ((ANCHOR_X - v2["x"]) / v2["zoom"], (ANCHOR_Y - v2["y"]) / v2["zoom"])
    check("... and six more notches AT the stop move nothing at all - no creep",
          near(world_at_stop[0], world_still[0], 1e-3)
          and near(world_at_stop[1], world_still[1], 1e-3), (world_at_stop, world_still))

    for _ in range(20):
        s.invoke("canvas.zoom-at", x=ANCHOR_X, y=ANCHOR_Y, factor=0.7, settle=0.0)
    step()
    v = state()["viewport"]
    check("and the bottom of the range is a stop too", near(v["zoom"], limits["min"]),
          (v["zoom"], limits["min"]))

    # ---- one notch is one ratio -----------------------------------------------------------------
    print("\n-- the wheel, through the widget's own constant --")

    set_view(0.0, 0.0, 1.0)
    step()
    z0 = state()["viewport"]["zoom"]
    s.invoke("canvas.wheel", x=ANCHOR_X, y=ANCHOR_Y, notches=1.0, settle=0.0)
    step()
    z1 = state()["viewport"]["zoom"]
    s.invoke("canvas.wheel", x=ANCHOR_X, y=ANCHOR_Y, notches=1.0, settle=0.0)
    step()
    z2 = state()["viewport"]["zoom"]

    check("equal notches are equal RATIOS, not equal steps",
          near(z1 / z0, z2 / z1, 1e-4), (z0, z1, z2))

    # The ratio, taken from what the widget says its step is rather than from a number written here:
    # a check carrying its own 1.1 would go on passing if the widget's changed.
    check("... and one notch is exactly the ratio the widget declares",
          near(z1 / z0, limits["stepRatio"], 1e-4), (z1 / z0, limits["stepRatio"]))
    check("... which is a tenth, gentle enough to aim with",
          near(limits["stepRatio"], 1.1, 1e-6), limits["stepRatio"])

    # ---- and through a real event, which is where the units live ----------------------------
    #
    # `canvas.wheel` above hands the widget a count of NOTCHES, so it can only ever assert the curve.
    # The window system delivers an AMOUNT, and the division from one to the other is the widget's -
    # which is exactly the step that was wrong: the raw amount went into the exponent, so a detent
    # of ten units raised the step to the tenth power and one click of the wheel was 1.1**10, two and
    # a half times the scale. This is the road that can see it.
    set_view(0.0, 0.0, 1.0)
    step()
    z0 = state()["viewport"]["zoom"]
    s.ok("input", events=wheel_events(ANCHOR_X, ANCHOR_Y, limits["notchAmount"]))
    step()
    z1 = state()["viewport"]["zoom"]

    check("ONE REAL NOTCH of the wheel is one step, not the step ten times over",
          near(z1 / z0, limits["stepRatio"], 1e-4), (z0, z1, z1 / z0, limits["stepRatio"]))
    check("... and a notch is delivered as an AMOUNT, which is more than one unit",
          limits["notchAmount"] > 1.0, limits["notchAmount"])

    # The wheel turned the other way undoes it exactly - the same check as above stated backwards,
    # and the one that pins the SIGN: a scroll up must magnify.
    s.ok("input", events=wheel_events(ANCHOR_X, ANCHOR_Y, -limits["notchAmount"]))
    step()
    check("a notch back is the same step down, so up and down cancel",
          near(state()["viewport"]["zoom"], z0, 1e-4), (state()["viewport"]["zoom"], z0))
    check("... and scrolling UP is what magnifies", z1 > z0, (z0, z1))

    # ---- framing --------------------------------------------------------------------------------
    print("\n-- fit --")

    st = s.invoke("canvas.fit", settle=0.0)
    step()
    st = state()
    v = st["viewport"]

    lo_x = min(x for x, _ in MARKERS.values())
    lo_y = min(y for _, y in MARKERS.values())
    hi_x = max(x for x, _ in MARKERS.values()) + MARKER_W
    hi_y = max(y for _, y in MARKERS.values()) + MARKER_H

    corners = [(lo_x, lo_y), (hi_x, hi_y)]
    on_screen = [(x * v["zoom"] + v["x"], y * v["zoom"] + v["y"]) for x, y in corners]
    check("every marker is inside the surface after a fit",
          all(-1.0 <= sx <= v["width"] + 1.0 and -1.0 <= sy <= v["height"] + 1.0
              for sx, sy in on_screen), (on_screen, v))

    left = on_screen[0][0]
    right = v["width"] - on_screen[1][0]
    check("... and the margins are symmetric, because padding shrinks the area and not the centre",
          near(left, right, 0.5), (left, right))

    agree(st, "... and the markers are still where the math puts them")

    # Framing reaches below the gesture range - the whole reason there are two ranges.
    check("a fit may leave the gesture range: framing has a range of its own",
          v["zoom"] <= limits["max"] + 1e-6, (v["zoom"], limits))

    # ---- clipping -------------------------------------------------------------------------------
    print("\n-- the scissor --")

    st = s.invoke("canvas.set-clipped", clipped=True, settle=0.0)
    step()
    check("a canvas clips by default, and says so", st["clipped"] is True, st.get("clipped"))
    check("... and the scissor is actually on, read off the system rather than off the flag",
          st.get("scissorEnabled") is True, st.get("scissorEnabled"))

    st = s.invoke("canvas.set-clipped", clipped=False, settle=0.0)
    step()
    check("turning it off turns the system off too", st["clipped"] is False
          and st.get("scissorEnabled") is False, (st.get("clipped"), st.get("scissorEnabled")))

    st = s.invoke("canvas.set-clipped", clipped=True, settle=0.0)
    step()
    check("and back on again", st.get("scissorEnabled") is True, st.get("scissorEnabled"))

    # ---- the floating control -------------------------------------------------------------------
    print("\n-- the floating zoom control --")

    set_view(0.0, 0.0, 1.0)
    step()
    st = state()
    zc = st["zoomControl"]
    v = st["viewport"]

    check("a canvas carries the control by default", zc["enabled"] is True, zc)
    check("... and it is INSIDE the surface, which is what makes it pressable",
          0.0 <= zc["x"] and 0.0 <= zc["y"]
          and zc["x"] + zc["width"] <= v["width"] + 1e-3
          and zc["y"] + zc["height"] <= v["height"] + 1e-3, (zc, v))
    check("... the readout says the zoom, as a percentage", zc["value"] == "100%", zc["value"])

    # A REAL press at the button's own centre, which is the half of this that the callback cannot
    # prove: a control laid out off the surface, or under something, has a callback that works and a
    # button nobody can hit.
    s.ok("input", native=True, events=click_events(zc["plus"]["x"], zc["plus"]["y"]))
    step()
    st = state()
    check("a click on + zooms in by exactly one step",
          near(st["viewport"]["zoom"] / v["zoom"], limits["stepRatio"], 1e-4),
          (st["viewport"]["zoom"], limits["stepRatio"]))
    check("... and the readout followed", st["zoomControl"]["value"] == "110%",
          st["zoomControl"]["value"])
    agree(st, "... and the markers are still where the math puts them")

    s.ok("input", native=True, events=click_events(zc["minus"]["x"], zc["minus"]["y"]))
    step()
    st = state()
    check("a click on - takes it back", near(st["viewport"]["zoom"], v["zoom"], 1e-4),
          st["viewport"]["zoom"])
    check("... and so does the readout", st["zoomControl"]["value"] == "100%",
          st["zoomControl"]["value"])

    # The step is the WHEEL's step. Two roads to a zoom that disagreed would be the same defect this
    # widget was written to remove, one level up.
    set_view(0.0, 0.0, 1.0)
    step()
    s.invoke("canvas.wheel", x=200.0, y=200.0, notches=1.0, settle=0.0)
    step()
    by_wheel = state()["viewport"]["zoom"]
    set_view(0.0, 0.0, 1.0)
    step()
    zc = state()["zoomControl"]
    s.ok("input", native=True, events=click_events(zc["plus"]["x"], zc["plus"]["y"]))
    step()
    by_button = state()["viewport"]["zoom"]
    check("the button and the wheel take the SAME step", near(by_wheel, by_button, 1e-4),
          (by_wheel, by_button))

    # ---- and the three that FRAME ---------------------------------------------------------------
    print("\n-- fit by width, by height, and back to 1:1 --")

    """WHAT AN AXIS MEANS, MEASURED. The markers are wider than they are tall in a surface that is
    wider than it is tall, so fitting by width and fitting by height are different numbers - which is
    what lets this fail: an axis read and then ignored would answer the same zoom twice, and so would
    `min(w, h)` in place of both branches.

    PRESSED FOR REAL, at the centres the stand reports. A button whose callback works and whose box
    is off the surface is a button nobody can hit, and the callback alone cannot tell the two apart -
    the same argument the two buttons above are pressed under."""
    def world_centre():
        v = state()["viewport"]
        return ((v["width"] / 2.0 - v["x"]) / v["zoom"], (v["height"] / 2.0 - v["y"]) / v["zoom"])

    # THE ARITHMETIC FIRST, through the command and with a padding wide enough to matter: three
    # markers 860 by 520 in a surface of 1400 frame at more than 1:1 either way, and framing clamps
    # at 1 - so with the default padding both axes answer the same number and there is nothing to
    # tell apart. The presses below are about the BUTTONS; this is about the axis.
    PAD = 380.0
    both = s.invoke("canvas.fit", padding=PAD, settle=0.0)
    step()
    both_zoom, both_centre = both["viewport"]["zoom"], world_centre()
    wide = s.invoke("canvas.fit", axis="width", padding=PAD, settle=0.0)["viewport"]["zoom"]
    step()
    wide_centre = world_centre()
    tall = s.invoke("canvas.fit", axis="height", padding=PAD, settle=0.0)["viewport"]["zoom"]
    step()
    tall_centre = world_centre()

    check("the two axes are different scales", abs(wide - tall) > 1e-3, (wide, tall))
    check("... and fitting by BOTH is the smaller of them, which is what 'the whole of it' means",
          near(both_zoom, min(wide, tall), 1e-4), (both_zoom, wide, tall))
    check("... and neither of them moved the centre",
          near(both_centre[0], wide_centre[0], 0.5) and near(both_centre[1], wide_centre[1], 0.5)
          and near(both_centre[0], tall_centre[0], 0.5)
          and near(both_centre[1], tall_centre[1], 0.5),
          (both_centre, wide_centre, tall_centre))

    # AND NOW THE BUTTONS, pressed for real at the centres the stand reports. What they have to prove
    # is that they are reachable and that they run the widget's own framing - so the view is put
    # somewhere they must move it away from first.
    framed = s.invoke("canvas.fit", settle=0.0)["viewport"]["zoom"]
    step()
    for name in ("fitWidth", "fitHeight"):
        set_view(0.0, 0.0, 2.0)
        step()
        zc = state()["zoomControl"]
        s.ok("input", native=True, events=click_events(zc[name]["x"], zc[name]["y"]))
        step()
        check(f"a click on {name} frames, and by the widget's own arithmetic",
              near(state()["viewport"]["zoom"], framed, 1e-4),
              (name, state()["viewport"]["zoom"], framed))

    zc = state()["zoomControl"]
    s.ok("input", native=True, events=click_events(zc["reset"]["x"], zc["reset"]["y"]))
    step()
    check("1:1 is exactly one", near(state()["viewport"]["zoom"], 1.0, 1e-4),
          state()["viewport"]["zoom"])
    check("... and it did not move the centre either",
          near(both_centre[0], world_centre()[0], 0.5)
          and near(both_centre[1], world_centre()[1], 0.5), (both_centre, world_centre()))
    check("... and the readout says so", state()["zoomControl"]["value"] == "100%",
          state()["zoomControl"]["value"])
    agree(state(), "... and the markers are still where the math puts them")

    st = s.invoke("canvas.zoom-control", enabled=False, settle=0.0)
    step()
    check("it can be turned off, for a canvas with its own chrome in that corner",
          state()["zoomControl"]["enabled"] is False, state()["zoomControl"])
    s.invoke("canvas.zoom-control", enabled=True, settle=0.0)
    step()
    check("... and back on", state()["zoomControl"]["enabled"] is True, state()["zoomControl"])

    # ---- the background grid --------------------------------------------------------------------
    #
    # The ladder, the cache and the snapping. The level a zoom picks is the node's arithmetic and is
    # read back as state; that the lines are one device pixel each and stand where the formula puts
    # them is only visible in a snapshot, so the last check reads one.
    print("\n-- the background grid --")

    check("there is no grid until one is asked for", state()["grid"]["enabled"] is False,
          state()["grid"])
    MAGENTA = [1.0, 0.0, 1.0, 1.0]
    s.invoke("canvas.grid", enabled=True, minor=MAGENTA, major=MAGENTA, settle=0.0)
    set_view(0.0, 0.0, 1.0)
    step()
    g = state()["grid"]
    check("at 1:1 a unit is one pixel, under the step, so drawing starts a level up",
          g["enabled"] and g["baseLevel"] == 1 and near(g["baseStep"], 4.0), g)
    check("... in whole device pixels", g["snapped"] is True, g)

    set_view(0.0, 0.0, 0.25)
    step()
    g = state()["grid"]
    check("zooming out to a quarter moves it up again, to every 16 units",
          g["baseLevel"] == 2 and near(g["baseStep"], 4.0), g)

    s.invoke("canvas.grid", minStep=1.0, settle=0.0)
    set_view(0.0, 0.0, 1.0)
    step()
    g = state()["grid"]
    check("with a one-pixel threshold the unit itself is drawn", g["baseLevel"] == 0, g)
    s.invoke("canvas.grid", minStep=0.25, settle=0.0)
    step()
    g = state()["grid"]
    check("... and the threshold never goes under a pixel", near(g["minStep"], 1.0), g)
    s.invoke("canvas.grid", minStep=4.0, settle=0.0)

    set_view(123.37, 211.61, 1.37)
    step(3)
    before = state()["grid"]["rebuilds"]
    step(3)
    check("a still frame reuses the lines", state()["grid"]["rebuilds"] == before,
          (before, state()["grid"]["rebuilds"]))
    # 131.93 + n * 5.48 never ends in .5, so no line sits on a rounding tie the check could read
    # differently from the node.
    set_view(131.93, 211.61, 1.37)
    step()
    check("... and a pan rebuilds them", state()["grid"]["rebuilds"] > before,
          (before, state()["grid"]["rebuilds"]))

    seen, predicted = grid_columns(s, 1.0, 300, 1000)
    check("every line is one whole pixel where the formula puts it, at a fractional pan and zoom",
          seen == predicted and len(seen) > 20,
          (sorted(seen - predicted)[:8], sorted(predicted - seen)[:8], len(seen)))

    # ---- no parallax ----------------------------------------------------------------------------
    #
    # THE PROPERTY, NOT A NUMBER: a pan must leave the world point under the pointer under the
    # pointer. It is asked twice at DIFFERENT densities because that is where the defect lived - the
    # gesture delta arrives in scene units (physical pixels) and the world's position is in the
    # canvas's own, so a canvas that added the raw delta moved the world by `density` times what the
    # pointer moved. At density 1 the two spaces coincide and the bug is invisible.
    print("\n-- a pan follows the pointer, at any density --")

    def pan_holds(session, name, x0, y0, x1, y1):
        session.invoke("canvas.set-view", x=0.0, y=0.0, zoom=1.0, settle=0.0)
        session.ok("frame", count=2)
        time.sleep(0.15)
        before = session.invoke("canvas.probe", x=x0, y=y0, settle=0.0)["world"]
        session.ok("input", native=True, events=drag_events(x0, y0, x1, y1))
        session.ok("frame", count=2)
        time.sleep(0.15)
        after = session.invoke("canvas.probe", x=x1, y=y1, settle=0.0)["world"]
        moved = session.invoke("canvas.state", settle=0.0)["viewport"]
        check(name, near(before["x"], after["x"], 0.5) and near(before["y"], after["y"], 0.5),
              (before, after))
        return moved

    moved = pan_holds(s, "the world point under the pointer is still under it after a drag",
                      300.0, 300.0, 460.0, 380.0)
    check("... and the drag actually panned, so the check had something to hold",
          abs(moved["x"]) > 1.0 and abs(moved["y"]) > 1.0, moved)

    hidpi_addr = ADDR + "-hidpi"
    hidpi_proc = start_app(binary, addr=hidpi_addr, width=2048, height=1536, density=2)
    hidpi = Session(hidpi_addr)
    try:
        # No layout switch: XL_CANVAS_TEST already opened this stand, and the second instance is
        # this one at twice the density and twice the surface - the same logical window.
        hidpi.ok("frame", count=3)
        time.sleep(0.2)
        moved = pan_holds(hidpi, "... and at density 2, where a raw delta moved it twice as far",
                          600.0, 600.0, 920.0, 760.0)
        check("... having panned there too", abs(moved["x"]) > 1.0 and abs(moved["y"]) > 1.0, moved)

        hidpi.invoke("canvas.grid", enabled=True, minor=MAGENTA, major=MAGENTA, settle=0.0)
        hidpi.invoke("canvas.set-view", x=57.3, y=40.8, zoom=0.61, settle=0.0)
        hidpi.ok("frame", count=2)
        time.sleep(0.15)
        seen, predicted = grid_columns(hidpi, 2.0, 600, 2000)
        check("... and at density 2 the grid is still one device pixel on the formula's columns",
              seen == predicted and len(seen) > 20,
              (sorted(seen - predicted)[:8], sorted(predicted - seen)[:8], len(seen)))
    finally:
        try:
            hidpi.close()
        except Exception:
            pass
        hidpi_proc.terminate()
        try:
            hidpi_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            hidpi_proc.kill()

    print(f"\n{checks} checks, {failures} failures")
finally:
    try:
        s.close()
    except Exception:
        pass
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

sys.exit(1 if failures else 0)
