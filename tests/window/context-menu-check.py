#!/usr/bin/env python3
"""Drive the context-menu stand (XL_CONTEXT_MENU_TEST) over the inspector socket.

Headless throughout. A menu is a REAL window here - the pseudo-controller stands in for the window
manager - so what a menu contains is read out of that window's own scene, and clicking a row is
addressed to it, exactly as on a desktop.

Most of what is checked is a menu that must NOT open, which on screen looks like nothing at all:

  * a target is published while it is DRAWN, so the topmost one answers and an invisible one does
    not exist. Two overlapping regions with distinct z-orders say which;
  * the press point reaches the builder in the TARGET'S own space. The region answers with
    different items for its two halves, which no screenshot could tell apart from a wrong menu;
  * a target that offers nothing BLOCKS instead of falling through, and the counter says the
    builder under it was never asked - "refused" told from "not reached";
  * a widget that swallows the right button stops the menu without declaring anything at all, and
    its own tap counter proves the press did arrive;
  * the mouse opens on RELEASE, so a right DRAG is not a menu;
  * a long press opens one only when the event says it came from a finger - the button is MouseLeft
    either way, so the modifier is the only thing that distinguishes them.

    tests/window/context-menu-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, re, socket, struct, subprocess, sys, time

# What the stand declares, duplicated here on purpose: a check that reads its expectations out of
# the thing it is checking cannot fail.
REGION_W = 600.0
REGION_H = 420.0

# InputModifier::Touch, 1 << 25. The one thing that tells a finger from a mouse: the button is
# MouseLeft for both.
TOUCH = 1 << 25

# ContextMenuSystem::DefaultLongPressInterval, plus room for a debug build to get round to it
LONG_PRESS = 0.5


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


def start_app(binary, addr):
    env = dict(os.environ)
    env["XL_CONTEXT_MENU_TEST"] = "1"
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


ROW = re.compile(r"menu-item #([\w-]+)")
POS = re.compile(r"pos=\(([-\d.]+),([-\d.]+)\)")
SZ = re.compile(r"sz=([\d.]+)x([\d.]+)")


class Stand:
    def __init__(self, binary, addr):
        self.proc = start_app(binary, addr)
        self.s = Session(addr)
        self.addr = addr
        self.step(3)

    def step(self, n=3, window=None):
        if window:
            self.s.ok("frame", count=n, window=window)
        else:
            self.s.ok("frame", count=n)
        # headless renders on demand, but a window created this frame is answered for on the next
        time.sleep(0.1)

    def state(self):
        return self.s.invoke("context-menu.state")

    def popup(self):
        """The id of the open menu window, or None."""
        for w in self.s.ok("windows")["windows"]:
            if w["type"] == "Popup":
                return w["id"]
        return None

    def items(self):
        """The item names of the open menu, in order, or []."""
        wid = self.popup()
        if not wid:
            return []
        self.step(2, window=wid)
        text = self.s.ok("scene", window=wid)["text"]
        return ROW.findall(text)

    def rows(self):
        """(name, world-centre) of every row of the open menu, in the POPUP's coordinates."""
        wid = self.popup()
        if not wid:
            return []
        text = self.s.ok("scene", window=wid)["text"]
        out = []
        for line in text.splitlines():
            m = ROW.search(line)
            if not m:
                continue
            p, z = POS.search(line), SZ.search(line)
            # a row's pos is its TOP (anchor TopLeft), and the scene is Y-up
            top, h, w = float(p.group(2)), float(z.group(2)), float(z.group(1))
            out.append((m.group(1), w / 2.0, top - h / 2.0))
        return out

    def send(self, *events, **kw):
        self.s.ok("input", events=list(events), **kw)

    def right_click(self, x, y):
        self.send(ev("Begin", x, y, "MouseRight"))
        self.step(1)
        self.send(ev("End", x, y, "MouseRight"))
        self.step(3)

    def clear(self):
        self.s.invoke("context-menu.close")
        self.s.invoke("context-menu.reset")
        self.step(2)

    def close(self):
        self.s.close()
        self.proc.kill()
        try:
            os.unlink(self.addr)
        except OSError:
            pass


def ev(name, x, y, button="MouseLeft", mods=0):
    return {"event": name, "id": 1, "button": button, "x": x, "y": y, "modifiers": mods}


def centre(rect, fx=0.5, fy=0.5):
    return (rect["x"] + rect["width"] * fx, rect["y"] + rect["height"] * fy)


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

app = Stand(binary, "/tmp/xl-context-menu-check.sock")
try:
    st = app.state()
    rects = st["rects"]

    print("the roster is what was drawn")
    # region, under, over, blocked - and NOT hidden, which was never visited
    check("only the visited targets are in it", st["targetCount"] == 4, st["targetCount"])
    check("and the invisible one is not drawn at all", rects["hidden"]["visible"] is False)

    print("a right click asks the target under it")
    app.clear()
    x, y = centre(rects["region"], 0.25, 0.5)
    app.right_click(x, y)
    st = app.state()
    check("the menu opened", st["menuOpen"] is True)
    check("the target that answered is the region", st["currentTarget"] == "region",
            st.get("currentTarget"))
    check("its items are the ones the builder made", app.items() == ["left-1", "left-2"],
            app.items())
    # the point is in the REGION's own space, not the window's
    req = st.get("lastRequest") or {}
    check("the point reached the builder in the target's space",
            abs(req.get("x", -1) - REGION_W * 0.25) < 1.0
            and abs(req.get("y", -1) - REGION_H * 0.5) < 1.0, req)

    print("and the point decides what it says")
    app.clear()
    x, y = centre(rects["region"], 0.75, 0.5)
    app.right_click(x, y)
    check("the other half of the same target answers differently",
            app.items() == ["right-1", "right-2"], app.items())

    print("the topmost target wins")
    app.clear()
    x, y = centre(rects["over"])
    app.right_click(x, y)
    check("the one drawn on top answers", app.items() == ["over-1"], app.items())
    check("and it is the one reported", app.state()["currentTarget"] == "over",
            app.state().get("currentTarget"))

    app.clear()
    # inside `under`, clear of `over`: 20px in from its left edge
    x = rects["under"]["x"] + 20.0
    y = rects["under"]["y"] + rects["under"]["height"] / 2.0
    app.right_click(x, y)
    check("beside it, the one underneath does", app.items() == ["under-1"], app.items())

    print("an invisible target does not exist")
    app.clear()
    x, y = centre(rects["hidden"])
    app.right_click(x, y)
    check("the region under it answers instead", app.state()["currentTarget"] == "region",
            app.state().get("currentTarget"))
    check("with its own items", app.items() == ["right-1", "right-2"], app.items())

    print("a target that offers nothing blocks")
    app.clear()
    x, y = centre(rects["blocked"])
    app.right_click(x, y)
    st = app.state()
    check("no menu opened", st["menuOpen"] is False)
    check("the blocking target is the one that answered", st["currentTarget"] == "blocked",
            st.get("currentTarget"))
    check("and nothing under it was asked", st["builderCalls"] == 1, st["builderCalls"])

    print("a widget that swallows the press stops it too")
    app.clear()
    x, y = centre(rects["swallow"])
    app.right_click(x, y)
    st = app.state()
    check("no menu opened", st["menuOpen"] is False)
    check("the press did arrive - the widget took it", st["swallowTaps"] == 1, st["swallowTaps"])
    check("and no builder ran", st["builderCalls"] == 0, st["builderCalls"])

    print("the mouse opens on release, so a drag is not a menu")
    app.clear()
    x, y = centre(rects["region"], 0.25, 0.5)
    app.send(ev("Begin", x, y, "MouseRight"))
    app.step(1)
    for i in range(1, 5):
        app.send(ev("Move", x + 10.0 * i, y, "MouseRight"))
        app.step(1)
    app.send(ev("End", x + 40.0, y, "MouseRight"))
    app.step(3)
    st = app.state()
    check("a right drag opens nothing", st["menuOpen"] is False and st["builderCalls"] == 0,
            (st["menuOpen"], st["builderCalls"]))

    print("a long press opens one, but only from a finger")
    app.clear()
    x, y = centre(rects["region"], 0.25, 0.5)
    # MouseLeft, not "Touch": the two are the same button and the protocol knows it by that name.
    # The modifier is the whole difference, which is exactly what is under test here
    app.send(ev("Begin", x, y, "MouseLeft", TOUCH))
    deadline = time.time() + LONG_PRESS + 1.5
    while time.time() < deadline and not app.state()["menuOpen"]:
        app.step(2)
    st = app.state()
    check("the finger held still opened it", st["menuOpen"] is True)
    check("and the builder was told it was a finger", st["lastFromTouch"] is True)
    check("with the same items a click gives", app.items() == ["left-1", "left-2"], app.items())
    app.send(ev("End", x, y, "MouseLeft", TOUCH))
    app.step(2)

    app.clear()
    app.send(ev("Begin", x, y, "MouseLeft"))
    deadline = time.time() + LONG_PRESS + 1.0
    while time.time() < deadline:
        app.step(2)
    st = app.state()
    check("the same hold with a mouse opens nothing",
            st["menuOpen"] is False and st["builderCalls"] == 0,
            (st["menuOpen"], st["builderCalls"]))
    app.send(ev("End", x, y, "MouseLeft"))
    app.step(2)

    print("an open menu covers the scene")
    app.clear()
    x, y = centre(rects["region"], 0.25, 0.5)
    app.right_click(x, y)
    check("a menu is up to be covered by", app.state()["menuOpen"] is True)

    # a plain click on the region, which counts taps of its own
    cx, cy = centre(rects["region"], 0.6, 0.3)
    app.send(ev("Begin", cx, cy))
    app.step(1)
    app.send(ev("End", cx, cy))
    app.step(4)
    st = app.state()
    check("the click took the menu down", st["menuOpen"] is False)
    check("and its window with it", app.popup() is None, app.popup())
    check("and it reached nothing else - the dismissal is what it was spent on",
            st["regionTaps"] == 0, st["regionTaps"])

    # ...and the NEXT one is an ordinary click
    app.send(ev("Begin", cx, cy))
    app.step(1)
    app.send(ev("End", cx, cy))
    app.step(3)
    check("the next click is an ordinary one", app.state()["regionTaps"] == 1,
            app.state()["regionTaps"])

    print("and a right click while one is open only closes it")
    app.clear()
    app.right_click(x, y)
    first = app.popup()
    check("the first one opened", first is not None)
    x2, y2 = centre(rects["region"], 0.75, 0.5)
    app.right_click(x2, y2)
    st = app.state()
    check("the second click closed it instead of opening another", st["menuOpen"] is False,
            st["menuOpen"])
    check("and asked no builder", st["builderCalls"] == 1, st["builderCalls"])

    # the one after it opens again, at the new place
    app.right_click(x2, y2)
    check("the click after that opens the menu again", app.state()["menuOpen"] is True)
    check("at the point it was asked for", app.items() == ["right-1", "right-2"], app.items())

    print("choosing an item")
    app.clear()
    x, y = centre(rects["region"], 0.25, 0.5)
    app.right_click(x, y)
    wid = app.popup()
    check("there is a menu to choose from", wid is not None)
    if wid:
        rows = app.rows()
        check("and it has the rows to choose", len(rows) == 2, rows)
        name, rx, ry = rows[0]
        app.send(ev("Begin", rx, ry), ev("End", rx, ry), window=wid, native=True)
        app.step(3)
        st = app.state()
        check("the item's own callback ran exactly once", st["activations"] == 1,
                st["activations"])
        check("and it was the row that was clicked", st["lastItem"] == name,
                (st["lastItem"], name))
        check("the menu is gone", st["menuOpen"] is False)

    print("the same call, with no pointer at all")
    app.clear()
    x, y = centre(rects["over"])
    res = app.s.invoke("context-menu.open-at", x=x, y=y)
    check("openAt answers for the same target", res["ok"] is True and res["lastBuilder"] == "over",
            (res["ok"], res.get("lastBuilder")))
    app.step(3)
    check("and opens the same menu", app.items() == ["over-1"], app.items())

    app.clear()
    x, y = centre(rects["blocked"])
    res = app.s.invoke("context-menu.open-at", x=x, y=y)
    check("and refuses where a click would", res["ok"] is False, res["ok"])
finally:
    app.close()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
