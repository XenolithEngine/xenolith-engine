#!/usr/bin/env python3
"""Drive the basic2d::ScrollView scroll bar stand (XL_SCROLLBAR_TEST) over the inspector socket.

Everything here is a number, because none of it is visible. A thumb one pixel too short and one
exactly right are the same picture; a bar that answers a drag at twice the rate looks like a bar
that answers it; "the press reached the row behind the bar" leaves no mark at all; and a rule that
never arrived and a rule that asked for the default are the same colour.

Four groups of claims:

  * GEOMETRY - the thumb's length is `track * viewport/content` floored at the minimum, and its
    position is the scroll position mapped through the travel. Compared by equality: a doubled
    response shows up as 2x and a forward/inverse mismatch as a constant offset, and both look
    right to the eye;
  * INPUT - grabbing the thumb, keeping it while the pointer leaves the bar sideways, clicking the
    empty track, hovering it. Each one also asserts that the row UNDER the bar saw nothing, which
    is the only way to tell "the bar swallowed the press" from "the bar was not in the way";
  * PAINT - `background-color` reaches the bar as built, `border-radius` and `outline` do not, and
    after ui::useStyledScrollIndicator all of it does. The control half matters as much as the
    other: without it a passing outline check could mean the swap happened somewhere it should not;
  * THE POINTING DEVICE - the whole run again under --headless-no-pointer, where the bar must be
    the thin one, must not answer a press, must not carry `.active` and must not reveal on hover.

    tests/window/scrollbar-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, socket, struct, subprocess, sys, time, zlib

# What the stand declares, duplicated here on purpose: a check that reads its expectations out of
# the thing it is checking cannot fail.
ROWS = 40
ROW_H = 40.0
VIEW_W = 400.0
VIEW_H = 320.0

# ScrollView's own constants, same reasoning
INSET = 2.0
MIN_LEN = 20.0
THICK_IDLE = 3.0
THICK_ACTIVE = 8.0

# what the stand's stylesheet asks for
CSS_TRACK_RADIUS = 5.0
CSS_THUMB_RADIUS = 3.0
CSS_THUMB_FILL = "#b0b0b0a0"
CSS_ACTIVE_OUTLINE = 2.0
CSS_ACTIVE_OUTLINE_COLOR = "#ff8000ff"

# what the widget itself uses when no sheet can reach it
WIDGET_THUMB_RADIUS = 2.0


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


def start_app(binary, addr, extra=()):
    env = dict(os.environ)
    env["XL_SCROLLBAR_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768", *extra],
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


class Stand:
    def __init__(self, binary, addr, extra=()):
        self.proc = start_app(binary, addr, extra)
        self.s = Session(addr)
        self.addr = addr
        self.step(3)

    def step(self, n=2):
        self.s.ok("frame", count=n)
        # headless renders on demand, but a style pass and an action tick land on the frames AFTER
        # the one that dirtied them, so the pause is what lets the next call see them
        time.sleep(0.08)

    def state(self):
        return self.s.invoke("scrollbar.state")

    def send(self, *events):
        self.s.ok("input", events=list(events))

    def pixels(self):
        self.step(3)
        data = self.s.ok("screenshot")["data"]
        assert data.startswith("BASE64:"), data[:32]
        blob = data[7:]
        return read_png(base64.urlsafe_b64decode(blob + "=" * (-len(blob) % 4)))

    def pixel_at(self, world_x, world_y):
        """A world point, which is y-UP, sampled out of an image, which is y-down."""
        w, h, rows = self.pixels()
        x, y = int(world_x), int(h - world_y)
        if x < 0 or y < 0 or x >= w or y >= h:
            return None
        return rows[y][x]

    def close(self):
        self.s.close()
        self.proc.kill()
        try:
            os.unlink(self.addr)
        except OSError:
            pass


def ev(name, x, y, button="MouseLeft"):
    return {"event": name, "id": 1, "button": button, "x": x, "y": y, "modifiers": 0}


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

app = Stand(binary, "/tmp/xl-scrollbar-check.sock")
try:
    st = app.state()
    content = ROWS * ROW_H
    track_len = VIEW_H - INSET * 2.0
    thumb_len = max(track_len * (VIEW_H / content), MIN_LEN)
    travel = track_len - thumb_len

    print("geometry")
    check("the content is as tall as the rows make it", near(st["scrollLength"], content),
            st["scrollLength"])
    check("the viewport is the one the stand pinned", near(st["scrollSize"], VIEW_H),
            st["scrollSize"])
    check("the track spans the viewport minus the inset", near(st["track"]["height"], track_len),
            st["track"]["height"])
    check("the thumb's length encodes the ratio", near(st["thumb"]["height"], thumb_len),
            (st["thumb"]["height"], thumb_len))
    check("and its position is the scroll position, mapped",
            near(st["thumb"]["y"], travel * (1.0 - st["relative"])), st["thumb"]["y"])

    print("the mapping is its own inverse")
    for value in (0.0, 0.25, 0.5, 0.75, 1.0):
        app.s.invoke("scrollbar.scroll", relative=value)
        app.step(1)
        st = app.state()
        check("relative %.2f survives the round trip" % value, near(st["relative"], value, 1e-4),
                st["relative"])
        check("and the thumb is where that says (%.2f)" % value,
                near(st["thumb"]["y"], travel * (1.0 - value), 0.05), st["thumb"]["y"])

    app.s.invoke("scrollbar.scroll", relative=0.0)
    app.step(1)

    st = app.state()
    has_pointer = st["hasInputPointer"]
    check("headless reports a pointing device by default", has_pointer is True)

    print("what a pointing device changes")
    check("the bar is the thick one", near(st["thickness"], THICK_ACTIVE), st["thickness"])
    check("it does not fade away", st["thumb"]["opacity"] > 0.0, st["thumb"]["opacity"])
    check("and both nodes carry `.active`, which is the only way a sheet can see that state",
            "active" in classes(st["trackPaint"]) and "active" in classes(st["thumbPaint"]),
            (classes(st["trackPaint"]), classes(st["thumbPaint"])))

    # --- input -------------------------------------------------------------------------------
    world = st["track"]["world"]
    check("the track has a place on screen", world["width"] > 0.0 and world["height"] > 0.0, world)
    x = world["x"] + world["width"] / 2.0

    print("grabbing the thumb")
    app.s.invoke("scrollbar.scroll", relative=0.0)
    app.s.invoke("scrollbar.reset-taps")
    app.step()
    st = app.state()
    grab_y = st["thumb"]["world"]["y"] + st["thumb"]["world"]["height"] / 2.0
    target = grab_y - travel / 2.0
    app.send(ev("MouseMove", x, grab_y, "None"))
    app.step()
    app.send(ev("Begin", x, grab_y))
    app.step()
    for i in range(1, 9):
        app.send(ev("Move", x, grab_y + (target - grab_y) * i / 8.0))
        app.step(1)
    st = app.state()
    check("half the travel puts it at the middle", near(st["relative"], 0.5, 0.02), st["relative"])
    app.send(ev("End", x, target))
    app.step(2)
    st = app.state()
    check("and it stays there after the release", near(st["relative"], 0.5, 0.02), st["relative"])
    check("no press reached the rows behind the bar", st["rowTaps"] == 0, st["rowTaps"])

    print("the pointer may leave the bar mid-drag")
    app.s.invoke("scrollbar.scroll", relative=0.0)
    app.step()
    st = app.state()
    grab_y = st["thumb"]["world"]["y"] + st["thumb"]["world"]["height"] / 2.0
    app.send(ev("MouseMove", x, grab_y, "None"))
    app.step()
    app.send(ev("Begin", x, grab_y))
    app.step()
    for i in range(1, 9):
        # 200px to the left: outside the track, outside the view
        app.send(ev("Move", x - 200.0, grab_y - travel / 2.0 * i / 8.0))
        app.step(1)
    check("the scroll follows anyway", near(app.state()["relative"], 0.5, 0.02),
            app.state()["relative"])
    app.send(ev("End", x - 200.0, grab_y - travel / 2.0))
    app.step(2)

    print("clicking the empty track")
    app.s.invoke("scrollbar.scroll", relative=0.0)
    app.s.invoke("scrollbar.reset-taps")
    app.step()
    click_y = world["y"] + 20.0  # near the bottom, well below a thumb parked at the top
    app.send(ev("MouseMove", x, click_y, "None"))
    app.step()
    app.send(ev("Begin", x, click_y))
    app.step(1)
    app.send(ev("End", x, click_y))
    app.step(2)
    st = app.state()
    check("jumps toward the press", st["relative"] > 0.5, st["relative"])
    check("and the row underneath never saw it", st["rowTaps"] == 0, st["rowTaps"])

    print("and the same press, with a pointing device")
    app.s.invoke("scrollbar.scroll", relative=0.0)
    app.s.invoke("scrollbar.reset-taps")
    app.step()
    row_y = world["y"] + world["height"] - ROW_H / 2.0
    app.send(ev("MouseMove", x, row_y, "None"))
    app.step(1)
    app.send(ev("Begin", x, row_y))
    app.step(1)
    app.send(ev("End", x, row_y))
    app.step(3)
    time.sleep(0.3)
    app.step(2)
    st = app.state()
    check("the bar takes it", st["rowTaps"] == 0, st["rowTaps"])
    check("and the view under it sees nothing either", st["viewTaps"] == 0, st["viewTaps"])

    print("hovering the track")
    app.s.invoke("scrollbar.reset-taps")
    app.send(ev("MouseMove", x, world["y"] + world["height"] / 2.0, "None"))
    app.step(2)
    check("reveals it", app.state()["track"]["opacity"] > 0.0, app.state()["track"]["opacity"])
    app.send(ev("MouseMove", world["x"] - 150.0, world["y"] + world["height"] / 2.0, "None"))
    app.step(2)
    check("and moving off hides it again", app.state()["track"]["opacity"] == 0.0,
            app.state()["track"]["opacity"])

    # --- paint -------------------------------------------------------------------------------
    print("the bar is actually on screen")
    app.s.invoke("scrollbar.scroll", relative=0.3)
    app.step(3)
    st = app.state()
    thumb = st["thumb"]["world"]
    cx = thumb["x"] + thumb["width"] / 2.0
    cy = thumb["y"] + thumb["height"] / 2.0
    on = app.pixel_at(cx, cy)
    behind = app.pixel_at(cx - 30.0, cy)   # the same row, clear of the bar
    check("the thumb paints over the row behind it", on is not None and on != behind,
            (on, behind))

    print("the bar as basic2d builds it")
    st = app.state()
    check("nothing has been swapped yet", st["styled"] is False)
    check("the colour from the sheet reached the thumb",
            st["thumbPaint"]["color"].lower().startswith("#b0b0b0"), st["thumbPaint"]["color"])
    check("but the radius is still the widget's own, not the sheet's",
            near(st["thumbPaint"]["radius"], WIDGET_THUMB_RADIUS), st["thumbPaint"]["radius"])
    check("and there is no outline at all, because the node cannot draw one",
            near(st["thumbPaint"]["outlineWidth"], 0.0), st["thumbPaint"]["outlineWidth"])

    print("and after useStyledScrollIndicator")
    app.s.invoke("scrollbar.set-styled")
    app.step(3)
    st = app.state()
    check("both nodes are the ones a stylesheet can paint",
            st["trackPaint"]["styled"] is True and st["thumbPaint"]["styled"] is True)
    check("the fill is what the sheet asked for",
            st["thumbPaint"]["fill"].lower() == CSS_THUMB_FILL, st["thumbPaint"]["fill"])
    check("the radius is the sheet's on the thumb",
            near(st["thumbPaint"]["radius"], CSS_THUMB_RADIUS), st["thumbPaint"]["radius"])
    check("and on the track", near(st["trackPaint"]["radius"], CSS_TRACK_RADIUS),
            st["trackPaint"]["radius"])
    check("the outline on `.active` arrived, which is the half the old node could not draw",
            near(st["thumbPaint"]["outlineWidth"], CSS_ACTIVE_OUTLINE)
            and st["thumbPaint"]["outlineColor"].lower() == CSS_ACTIVE_OUTLINE_COLOR,
            (st["thumbPaint"]["outlineWidth"], st["thumbPaint"]["outlineColor"]))
    check("the bar is still where it was - a swap is not a re-layout",
            near(st["track"]["height"], track_len) and near(st["thumb"]["height"], thumb_len),
            (st["track"]["height"], st["thumb"]["height"]))

    st = app.state()
    thumb = st["thumb"]["world"]
    cx = thumb["x"] + thumb["width"] / 2.0
    cy = thumb["y"] + thumb["height"] / 2.0
    on = app.pixel_at(cx, cy)
    behind = app.pixel_at(cx - 30.0, cy)
    check("and the swapped bar is on screen too, not merely configured",
            on is not None and on != behind, (on, behind))

    # ...and after the track's own opacity has been written, which is the moment a thumb that
    # inherited it would go dark: hover the bar and leave again
    app.send(ev("MouseMove", x, cy, "None"))
    app.step(2)
    app.send(ev("MouseMove", world["x"] - 150.0, cy, "None"))
    app.step(2)
    st = app.state()
    thumb = st["thumb"]["world"]
    cx = thumb["x"] + thumb["width"] / 2.0
    cy = thumb["y"] + thumb["height"] / 2.0
    check("and stays on screen after the track has faded in and out",
            app.pixel_at(cx, cy) != app.pixel_at(cx - 30.0, cy),
            (app.pixel_at(cx, cy), app.pixel_at(cx - 30.0, cy)))

    print("display: none removes it")
    app.s.invoke("scrollbar.set-hidden", value=True)
    app.step(3)
    st = app.state()
    check("the track is not effectively visible", st["trackPaint"]["effectivelyVisible"] is False)
    check("even though the widget still sets its own visible flag",
            st["trackPaint"]["visible"] is True)
    app.s.invoke("scrollbar.set-hidden", value=False)
    app.step(3)
    check("and taking the rule away brings it back",
            app.state()["trackPaint"]["effectivelyVisible"] is True)
    st = app.state()
    thumb = st["thumb"]["world"]
    check("and the pixels are back with it",
            app.pixel_at(thumb["x"] + thumb["width"] / 2.0, thumb["y"] + thumb["height"] / 2.0)
            != app.pixel_at(thumb["x"] - 30.0, thumb["y"] + thumb["height"] / 2.0))
finally:
    app.close()

# --- the same stand with no pointing device ------------------------------------------------------
print("with --headless-no-pointer")
app = Stand(binary, "/tmp/xl-scrollbar-check-np.sock", ["--headless-no-pointer"])
try:
    st = app.state()
    check("  the window reports no pointing device", st["hasInputPointer"] is False)
    check("  the bar is the thin one", near(st["thickness"], THICK_IDLE), st["thickness"])
    check("  and carries no `.active`", "active" not in classes(st["trackPaint"]),
            classes(st["trackPaint"]))

    world = st["track"]["world"]
    x = world["x"] + world["width"] / 2.0
    y = world["y"] + world["height"] / 2.0

    app.s.invoke("scrollbar.reset-taps")
    app.send(ev("MouseMove", x, y, "None"))
    app.step(2)
    check("  hovering does not reveal the track", app.state()["track"]["opacity"] == 0.0,
            app.state()["track"]["opacity"])

    # A row's own centre, not the middle of the track: a point that lands exactly on the boundary
    # between two rows belongs to neither box and the press is delivered to no row at all
    row_y = world["y"] + world["height"] - ROW_H / 2.0
    before = app.state()["relative"]
    app.send(ev("MouseMove", x, row_y, "None"))
    app.step(1)
    app.send(ev("Begin", x, row_y))
    app.step(1)
    app.send(ev("End", x, row_y))
    app.step(3)
    time.sleep(0.3)
    app.step(2)
    st = app.state()
    check("  a press does not move the bar", near(st["relative"], before, 1e-4), st["relative"])
    check("  and reaches the row underneath instead, which is what a dead bar means",
            st["rowTaps"] > 0, st["rowTaps"])
finally:
    app.close()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
