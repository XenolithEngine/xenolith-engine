#!/usr/bin/env python3
"""Drive the rubber band over lists (XL_MARQUEE_TEST) over the inspector socket, headless.

  * a mouse drag over a table sweeps a band: the rows it covers are lit before the release, and
    nothing else moves - no selection, no callback, no :selected;
  * the release applies it once: plain replaces, Ctrl toggles, Shift adds; a filtered row is
    neither lit nor taken; Escape cancels and the release then writes nothing;
  * clicks stay clicks, and the header, the reorder grip, a panel drawn over the table and a
    finger start no band; with the band off a drag scrolls;
  * the band is cut below the header, and near the bottom edge, or below it, the list scrolls;
  * a rebuild or a mode change in the middle of a sweep leaves nothing stale;
  * a tree keeps a selected row of a collapsed branch through a band that adds or toggles;
  * a grid of its own over a ScrollView gets its band in content space, across a scroll.

    tests/window/marquee-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-marquee-check.sock")

SHIFT = 1 << 0
CTRL = 1 << 2
TOUCH = 1 << 25


class Session:
    def __init__(self, path=ADDR, timeout=25.0):
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


def key(code, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def key(code, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def start_app(binary):
    env = dict(os.environ)
    env["XL_MARQUEE_TEST"] = "1"
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
                s = Session()
                s.close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


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


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()


def state():
    return s.invoke("marquee.state")


def row(view, index):
    return state()[view]["rows"][index]


def wait_for(predicate, tries=20):
    for _ in range(tries):
        if predicate():
            return True
        s.ok("frame", count=1)
        time.sleep(0.02)
    return predicate()


def settle():
    # Past the double-press window, so the next press is a press of its own
    time.sleep(0.45)


pointer = [0.0, 0.0]
mods_held = [0]


def send(event, x, y, mods):
    s.ok("input", native=True, events=[{"event": event, "x": x, "y": y, "button": "MouseLeft",
        "modifiers": mods}])


def begin(x, y, mods=0):
    pointer[0], pointer[1] = x, y
    mods_held[0] = mods
    send("Begin", x, y, mods)
    s.ok("frame", count=1)


def move_to(x, y, step=3.0):
    """Small steps with a frame after each, as a hand moves: the listeners see every one."""
    guard = 0
    while (abs(pointer[0] - x) > step or abs(pointer[1] - y) > step) and guard < 600:
        dx = max(-step, min(step, x - pointer[0]))
        dy = max(-step, min(step, y - pointer[1]))
        pointer[0] += dx
        pointer[1] += dy
        send("Move", pointer[0], pointer[1], mods_held[0])
        s.ok("frame", count=1)
        guard += 1
    pointer[0], pointer[1] = x, y
    send("Move", x, y, mods_held[0])
    s.ok("frame", count=1)
    wait_for(caught_up)


def caught_up():
    """Whether a band that is up has seen the last move: input can land a frame late."""
    st = state()
    for v in ("table", "tree", "grid"):
        band = st[v]["marquee"]
        if st[v]["active"] and (abs(band.get("x", 0) - pointer[0]) > 0.5
                or abs(band.get("y", 0) - pointer[1]) > 0.5):
            return False
    return True


def end():
    """The release; a sweep is over once no band is up, which is waited for rather than assumed."""
    send("End", pointer[0], pointer[1], mods_held[0])
    s.ok("frame", count=2)
    wait_for(lambda: not any(state()[v]["active"] for v in ("table", "tree", "grid")))


def sweep(a, b, mods=0):
    begin(a[0], a[1], mods)
    move_to(b[0], b[1])
    end()


def sweep_batch(a, b, mods=0, step=3.0):
    """The whole drag in one dispatch, as a harness that sends a list of events does."""
    events = [{"event": "Begin", "x": a[0], "y": a[1], "button": "MouseLeft", "modifiers": mods}]
    y = a[1]
    while abs(y - b[1]) > step:
        y += step if b[1] > y else -step
        events.append({"event": "Move", "x": a[0], "y": y, "button": "MouseLeft",
            "modifiers": mods})
    events.append({"event": "Move", "x": b[0], "y": b[1], "button": "MouseLeft",
        "modifiers": mods})
    events.append({"event": "End", "x": b[0], "y": b[1], "button": "MouseLeft",
        "modifiers": mods})
    s.ok("input", native=True, events=events)
    s.ok("frame", count=2)


def at(view, index, st=None):
    r = (st or state())[view]["rows"][index]
    return (r["x"], r["y"])


def click(x, y, mods=0):
    ev = {"x": x, "y": y, "button": "MouseLeft", "modifiers": mods}
    s.ok("input", native=True, events=[dict(ev, event="Begin"), dict(ev, event="End")])
    s.ok("frame", count=2)


def press_key(code, mods=0):
    s.ok("input", native=True, events=key(code, mods))
    s.ok("frame", count=2)


def selected(view):
    return state()[view]["selected"]


def after(view, rows):
    wait_for(lambda: selected(view) == rows)
    return state()


def intersects(a, b):
    return (a["x"] <= b["x"] + b["width"] and b["x"] <= a["x"] + a["width"]
            and a["y"] <= b["y"] + b["height"] and b["y"] <= a["y"] + a["height"])


try:
    s.ok("frame", count=4)
    st = state()
    check("the table and the tree are in the multiple mode with the band on",
            st["table"]["mode"] == "multiple" and st["table"]["enabled"]
                    and st["tree"]["mode"] == "multiple" and st["tree"]["enabled"],
            (st["table"]["mode"], st["table"]["enabled"]))
    check("the table carries its rows and scrolls", st["table"]["rowCount"] == 60
            and st["table"]["scroll"]["max"] > 0, (st["table"]["rowCount"], st["table"]["scroll"]))

    print("-- a band over the table")
    st = state()
    begin(*at("table", 1, st))
    move_to(*at("table", 3, st))
    mid = state()
    t = mid["table"]
    check("in the middle of a sweep the rows it covers are lit",
            t["active"] and t["hits"] == [1, 2, 3] and t["shown"] == [1, 2, 3],
            (t["active"], t["hits"], t["shown"]))
    check("... and nothing else moved: no selection, no :selected, no callback",
            t["selected"] == [] and t["states"] == [] and mid["selects"] == 0
                    and mid["sweeps"] == 0, (t["selected"], t["states"], mid["selects"]))
    band = t["marquee"]
    check("the band is drawn, painted by the sheet",
            band["visible"] and band["band"]["height"] > 0 and band["color"][:3] == [0, 120, 255],
            band)
    end()
    st = after("table", [1, 2, 3])
    t = st["table"]
    check("the release applies it once, as a replace",
            t["selected"] == [1, 2, 3] and st["sweeps"] == 1 and st["lastSweepOp"] == "replace"
                    and st["lastSweepRows"] == [1, 2, 3], (t["selected"], st["sweeps"], st["lastSweepOp"]))
    check("... the current row where the pointer was, the select callback silent",
            t["current"] == 3 and st["selects"] == 0, (t["current"], st["selects"]))
    check("... and the table takes the scene's selection, every row :selected",
            st["owner"] == "table" and st["items"] == 3 and t["states"] == [1, 2, 3]
                    and not t["marquee"]["visible"], (st["owner"], st["items"], t["states"]))

    st = state()
    sweep(at("table", 6, st), at("table", 7, st), SHIFT)
    st = after("table", [1, 2, 3, 6, 7])
    check("with Shift the band adds", st["table"]["selected"] == [1, 2, 3, 6, 7]
            and st["lastSweepOp"] == "add-range", (st["table"]["selected"], st["lastSweepOp"]))

    st = state()
    begin(*at("table", 2, st), CTRL)
    move_to(*at("table", 6, st))
    t = state()["table"]
    check("with Ctrl it toggles what it covers, and the locked row is neither lit nor hit",
            t["hits"] == [2, 3, 5, 6] and t["shown"] == [1, 5, 7] and 4 not in t["shown"],
            (t["hits"], t["shown"]))
    end()
    st = after("table", [1, 5, 7])
    check("... and the release says so", st["table"]["selected"] == [1, 5, 7]
            and st["lastSweepOp"] == "toggle" and st["sweeps"] == 3,
            (st["table"]["selected"], st["lastSweepOp"], st["sweeps"]))

    st = state()
    sweep(at("table", 8, st), at("table", 9, st))
    check("a plain band replaces again", after("table", [8, 9])["table"]["selected"] == [8, 9],
            selected("table"))

    st = state()
    sweep_batch(at("table", 10, st), at("table", 11, st))
    check("a whole drag in one dispatch sweeps too",
            after("table", [10, 11])["table"]["selected"] == [10, 11], selected("table"))

    print("-- cancel")
    sweeps = state()["sweeps"]
    st = state()
    begin(*at("table", 0, st))
    move_to(*at("table", 2, st))
    check("a sweep is running", state()["table"]["active"], "")
    press_key("ESCAPE")
    wait_for(lambda: not state()["table"]["active"])
    t = state()["table"]
    check("Escape takes it down and the rows show the selection again",
            not t["active"] and t["shown"] == [10, 11] and not t["marquee"]["visible"],
            (t["active"], t["shown"]))
    move_to(*at("table", 5, st))
    end()
    st = state()
    check("... and the release writes nothing", st["table"]["selected"] == [10, 11]
            and st["sweeps"] == sweeps and not st["table"]["active"],
            (st["table"]["selected"], st["sweeps"]))

    print("-- what starts no band")
    settle()
    selects = state()["selects"]
    click(*at("table", 5))
    st = after("table", [5])
    check("a click is still a click", st["table"]["selected"] == [5]
            and st["selects"] == selects + 1, (st["table"]["selected"], st["selects"]))
    settle()
    x, y = at("table", 6)
    begin(x, y)
    move_to(x, y - 3.0)
    end()
    st = after("table", [6])
    check("... and so is a press that shakes by a few points", st["table"]["selected"] == [6]
            and not st["table"]["active"], st["table"]["selected"])
    settle()
    activates = state()["activates"]
    x, y = at("table", 7)
    ev = {"x": x, "y": y, "button": "MouseLeft", "modifiers": 0}
    pair = [dict(ev, event="Begin"), dict(ev, event="End")]
    s.ok("input", native=True, events=pair)
    s.ok("input", native=True, events=pair)
    s.ok("frame", count=2)
    check("a double click activates", wait_for(lambda: state()["activates"] == activates + 1),
            state()["activates"])
    settle()

    st = state()
    header = st["header"]
    hx = header["x"] + header["width"] / 2.0
    hy = header["y"] + header["height"] / 2.0
    begin(hx, hy)
    move_to(hx, at("table", 3, st)[1])
    active = state()["table"]["active"]
    end()
    st = state()
    check("a press on the header starts no band", not active and st["table"]["selected"] == [7],
            (active, st["table"]["selected"]))

    st = state()
    reorders = st["reorders"]
    gy = at("table", 8, st)[1]
    begin(st["gripX"], gy)
    move_to(st["gripX"], at("table", 10, st)[1] - 6.0)
    active = state()["table"]["active"]
    end()
    st = state()
    check("a press on the grip moves the row, with no band",
            not active and wait_for(lambda: state()["reorders"] == reorders + 1),
            (active, state()["reorders"]))

    st = state()
    cover = st["cover"]
    cx = cover["x"] + cover["width"] / 2.0
    cy = cover["y"] + cover["height"] / 2.0
    begin(cx, cy)
    move_to(cx - 60.0, cy + 60.0)
    active = state()["table"]["active"]
    end()
    check("a press on a panel drawn over the table starts no band", not active, active)

    before = state()["table"]["scroll"]["pos"]
    st = state()
    x, y = at("table", 8, st)
    s.ok("input", native=True, events=[{"event": "Begin", "x": x, "y": y, "button": "MouseLeft",
        "modifiers": TOUCH}])
    s.ok("frame", count=1)
    moved = False
    active = False
    for i in range(1, 30):
        s.ok("input", native=True, events=[{"event": "Move", "x": x, "y": y + 3.0 * i,
            "button": "MouseLeft", "modifiers": TOUCH}])
        s.ok("frame", count=1)
        active = active or state()["table"]["active"]
    s.ok("input", native=True, events=[{"event": "End", "x": x, "y": y + 87.0,
        "button": "MouseLeft", "modifiers": TOUCH}])
    s.ok("frame", count=4)
    check("a finger pans the list rather than sweeping it",
            not active and state()["table"]["scroll"]["pos"] != before,
            (active, before, state()["table"]["scroll"]["pos"]))
    s.invoke("marquee.scroll", view="table", position=0)
    s.ok("frame", count=2)

    s.invoke("marquee.set", view="table", enabled=False)
    s.ok("frame", count=2)
    before = state()["table"]["scroll"]["pos"]
    st = state()
    x, y = at("table", 8, st)
    begin(x, y)
    move_to(x, y + 90.0)
    active = state()["table"]["active"]
    end()
    s.ok("frame", count=4)
    st = state()
    check("with the band off a drag scrolls", not active
            and st["table"]["scroll"]["pos"] != before and st["table"]["selected"] == [7],
            (active, before, st["table"]["scroll"]["pos"], st["table"]["selected"]))
    s.invoke("marquee.set", view="table", enabled=True)
    s.invoke("marquee.scroll", view="table", position=0)
    s.ok("frame", count=2)

    print("-- the edges")
    st = state()
    header = st["header"]
    begin(*at("table", 2, st))
    move_to(at("table", 2, st)[0], header["y"] + header["height"] + 40.0)
    t = state()["table"]
    band = t["marquee"]
    check("the band is cut below the header, though the sweep reaches above it",
            band["visible"] and band["world"]["y"] + band["world"]["height"] <= header["y"] + 0.5
                    and t["hits"][:1] == [0], (band["world"], header, t["hits"]))
    press_key("ESCAPE")
    end()

    st = state()
    viewport = st["table"]["marquee"]["viewport"]
    start = at("table", 2, st)
    visible = [r["index"] for r in st["table"]["rows"] if r.get("visible")]
    begin(*start)
    bottom = st["header"]["y"] - viewport["height"] + 10.0
    move_to(start[0], bottom)
    first = state()["table"]["scroll"]["pos"]
    for _ in range(20):
        s.ok("frame", count=1)
        time.sleep(0.02)
    second = state()["table"]["scroll"]["pos"]
    check("held near the bottom edge the list scrolls", second > first, (first, second))
    move_to(start[0], bottom - 50.0)
    for _ in range(20):
        s.ok("frame", count=1)
        time.sleep(0.02)
    t = state()["table"]
    check("... and keeps scrolling below it", t["scroll"]["pos"] > second, (second, t["scroll"]))
    check("... the band reaching rows that were out of sight at the press",
            t["hits"] and t["hits"][-1] > max(visible), (t["hits"][-3:], max(visible)))
    last = t["hits"][-1]
    end()
    st = state()
    sel = st["table"]["selected"]
    check("... and the release takes the run, but the locked row, as far as the list had scrolled",
            sel[:1] == [2] and sel[-1] >= last
                    and sel == [i for i in range(2, sel[-1] + 1) if i != 4]
                    and not st["table"]["active"], (sel[:2], sel[-2:], last))
    s.invoke("marquee.scroll", view="table", position=0)
    s.ok("frame", count=2)

    print("-- changes under a sweep")
    st = state()
    begin(*at("table", 1, st))
    move_to(*at("table", 3, st))
    s.invoke("marquee.insert", at=0)
    s.ok("frame", count=3)
    t = state()["table"]
    check("a row inserted under a sweep: the band still covers the same place",
            t["active"] and t["rowCount"] == 61 and t["hits"] == [1, 2, 3], (t["active"], t["hits"]))
    check("... and what is lit is what it covers now", t["shown"] == t["hits"], t["shown"])
    end()
    check("... which the release takes", after("table", [1, 2, 3])["table"]["selected"] == [1, 2, 3],
            selected("table"))

    sweeps = state()["sweeps"]
    st = state()
    begin(*at("table", 5, st))
    move_to(*at("table", 7, st))
    s.invoke("marquee.set", view="table", mode="single")
    s.ok("frame", count=2)
    active = state()["table"]["active"]
    move_to(*at("table", 8, st))
    end()
    st = state()
    check("switching to the single mode takes a sweep down, and it writes nothing",
            not active and st["sweeps"] == sweeps and not st["table"]["active"],
            (active, st["sweeps"]))
    s.invoke("marquee.set", view="table", mode="multiple")
    s.ok("frame", count=2)

    print("-- the tree")
    s.invoke("marquee.expand", row=0, open=True)
    s.ok("frame", count=2)
    s.invoke("marquee.expand", row=5, open=True)
    s.ok("frame", count=2)
    check("both branches are open", state()["tree"]["rowCount"] == 10, state()["tree"]["rowCount"])

    def hidden_case(mods, expect, name):
        s.invoke("marquee.select", view="tree", rows=[7], current=7)
        s.ok("frame", count=2)
        s.invoke("marquee.expand", row=5, open=False)
        s.ok("frame", count=2)
        st = state()
        sweep(at("tree", 1, st), at("tree", 2, st), mods)
        s.invoke("marquee.expand", row=5, open=True)
        s.ok("frame", count=2)
        check(name, after("tree", expect)["tree"]["selected"] == expect, selected("tree"))

    hidden_case(SHIFT, [1, 2, 7], "a band that adds keeps a selected row of a closed branch")
    hidden_case(CTRL, [1, 2, 7], "... so does one that toggles")
    hidden_case(0, [1, 2], "... and one that replaces drops it")
    check("the tree reports its bands", state()["treeSweeps"] >= 3, state()["treeSweeps"])

    print("-- a grid of its own")
    st = state()
    g = st["grid"]
    tiles = g["tiles"]
    a = (tiles[1]["x"], tiles[1]["y"])
    b = (tiles[6]["x"], tiles[6]["y"])
    begin(*a)
    move_to(*b)
    g = state()["grid"]
    rect = g["marquee"]["rect"]
    expected = [t["index"] for t in tiles if "content" in t and intersects(t["content"], rect)]
    check("in the middle of a sweep the grid is told the band in content space",
            g["active"] and g["hits"] == expected and 1 in expected and 6 in expected,
            (g["hits"], expected, rect))
    offset = tiles[1]["y"] - (tiles[1]["content"]["y"] + tiles[1]["content"]["height"] / 2.0)
    check("... the band's corner at the press, in the space of the tiles",
            abs(rect["y"] + rect["height"] + offset - a[1]) < 1.0,
            (rect, offset, a))
    check("... lit, and not selected yet", g["shown"] == g["hits"] and g["selected"] == []
            and g["commits"] == 0, (g["shown"], g["selected"]))
    hits = g["hits"]
    end()
    g = state()["grid"]
    check("the release selects what it covered, once", g["selected"] == hits and g["commits"] == 1
            and not g["active"], (g["selected"], g["commits"]))

    begin(*a)
    move_to(*b)
    top = state()["grid"]["marquee"]["rect"]
    s.invoke("marquee.scroll", view="grid", position=48)
    s.ok("frame", count=3)
    g = state()["grid"]
    rect = g["marquee"]["rect"]
    check("scrolled under a sweep, the band keeps its corner in the content",
            g["active"] and abs((rect["y"] + rect["height"]) - (top["y"] + top["height"])) < 0.5
                    and rect["height"] > top["height"] + 40.0, (top, rect))
    check("... and covers the row that came under the pointer",
            any(i >= 8 for i in g["hits"]), g["hits"])
    hits = g["hits"]
    end()
    g = state()["grid"]
    check("... which the release takes", g["selected"] == hits and g["commits"] == 2,
            (g["selected"], g["commits"]))
finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
