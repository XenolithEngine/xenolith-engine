#!/usr/bin/env python3
"""Drive multiple selection in lists (XL_LIST_SELECTION_TEST) over the inspector socket, headless.

  * a press picks one row, Ctrl toggles one, Shift takes the run from the anchor instead of the
    rest, Ctrl+Shift adds the run; the anchor stays where the last plain or Ctrl press put it;
  * a second press with a modifier is another pick, never an activation;
  * Shift+Up/Down extend the run from the keyboard, a plain arrow collapses it, Ctrl+A takes every
    row - and none of the three reach the list while a text field has the caret;
  * every selected row carries the `selected` class and the scene's `:selected`, and a set of rows
    anchors the scene's selection on the list rather than on a row;
  * a selected row inside a collapsed branch of a tree stays selected, an additive press keeps it;
  * a list in the single mode reads every modifier as a plain press.

    tests/window/list-selection-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-list-selection-check.sock")

SHIFT = 1 << 0
CTRL = 1 << 2
ROWS = 30


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


def start_app(binary):
    env = dict(os.environ)
    env["XL_LIST_SELECTION_TEST"] = "1"
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
    return s.invoke("list-selection.state")


def row(view, index):
    return state()[view]["rows"][index]


def click(view, index, mods=0, frames=2):
    r = row(view, index)
    ev = {"x": r["x"], "y": r["y"], "button": "MouseLeft", "modifiers": mods}
    s.ok("input", native=True, events=[dict(ev, event="Begin"), dict(ev, event="End")])
    s.ok("frame", count=frames)


def double_click(view, index, mods=0):
    """Two presses inside the double-press window: the point is taken first, since a state read
    between them can outlast the window in a debug build."""
    r = row(view, index)
    ev = {"x": r["x"], "y": r["y"], "button": "MouseLeft", "modifiers": mods}
    pair = [dict(ev, event="Begin"), dict(ev, event="End")]
    s.ok("input", native=True, events=pair)
    s.ok("input", native=True, events=pair)
    s.ok("frame", count=2)


def wait_for(predicate, tries=20):
    """A key is answered on a frame of its own; the answer is waited for, not assumed."""
    for _ in range(tries):
        if predicate():
            return True
        s.ok("frame", count=1)
        time.sleep(0.02)
    return predicate()


def press_key(code, mods=0):
    s.ok("input", native=True, events=key(code, mods))
    s.ok("frame", count=2)


def selected(view):
    return state()[view]["selected"]


def after(view, rows):
    """The state once `view` shows these rows selected, or as it is when it never does."""
    wait_for(lambda: selected(view) == rows)
    return state()


def settle():
    # Past the double-press window, so the next press on the same row is a press of its own
    time.sleep(0.45)


try:
    s.ok("frame", count=4)
    st = state()
    check("the table and the tree are in the multiple mode, the third list in the single one",
            st["table"]["mode"] == "multiple" and st["tree"]["mode"] == "multiple"
                    and st["single"]["mode"] == "single", st["table"]["mode"])
    check("the table carries its rows", st["table"]["rowCount"] == ROWS, st["table"]["rowCount"])

    print("-- presses")
    click("table", 2)
    st = after("table", [2])
    check("a press picks one row", st["table"]["selected"] == [2] and st["table"]["current"] == 2,
            st["table"])
    check("... and is reported as a replace", st["lastOp"] == "replace" and st["selects"] == 1,
            (st["lastOp"], st["selects"]))
    check("one row anchors the scene's selection on the row",
            st["owner"] == "table" and st["items"] == 1 and st["anchorType"] == "table-row",
            (st["owner"], st["items"], st["anchorType"]))

    click("table", 5, CTRL)
    st = after("table", [2, 5])
    check("Ctrl adds a row", st["table"]["selected"] == [2, 5] and st["table"]["current"] == 5,
            st["table"]["selected"])
    check("... reported as a toggle, with the whole set in hand",
            st["lastOp"] == "toggle" and st["lastRows"] == [2, 5], (st["lastOp"], st["lastRows"]))
    check("a set of rows anchors the scene's selection on the list",
            st["items"] == 2 and st["anchor"] == "table", (st["items"], st["anchor"]))
    nodes = {n["index"]: n for n in st["table"]["nodes"]}
    check("every selected row carries the class and the scene's :selected",
            all(nodes[i]["selectedClass"] and nodes[i]["selectedState"] for i in (2, 5))
                    and not nodes[3]["selectedClass"] and not nodes[3]["selectedState"],
            {i: nodes[i] for i in (2, 3, 5)})

    click("table", 7, SHIFT)
    st = after("table", [5, 6, 7])
    check("Shift takes the run from the anchor instead of the rest",
            st["table"]["selected"] == [5, 6, 7] and st["lastOp"] == "range", st["table"]["selected"])

    click("table", 1, CTRL | SHIFT)
    check("Ctrl+Shift adds the run from the same anchor",
            after("table", [1, 2, 3, 4, 5, 6, 7])["table"]["selected"] == [1, 2, 3, 4, 5, 6, 7],
            selected("table"))

    click("table", 3, CTRL)
    st = after("table", [1, 2, 4, 5, 6, 7])
    check("Ctrl on a selected row takes it out, and it stays current",
            st["table"]["selected"] == [1, 2, 4, 5, 6, 7] and st["table"]["current"] == 3,
            st["table"])

    settle()
    double_click("table", 3, CTRL)
    wait_for(lambda: state()["selects"] >= 7)
    st = state()
    check("a Ctrl double press is two toggles and no activation",
            st["table"]["selected"] == [1, 2, 4, 5, 6, 7] and st["activates"] == 0,
            (st["table"]["selected"], st["activates"]))

    settle()
    double_click("table", 9)
    wait_for(lambda: state()["activates"] >= 1)
    st = state()
    check("a plain double press still activates", st["activates"] == 1 and st["table"]["selected"] == [9],
            (st["activates"], st["table"]["selected"]))

    print("-- keys")
    press_key("DOWN", SHIFT)
    press_key("DOWN", SHIFT)
    wait_for(lambda: selected("table") == [9, 10, 11])
    st = state()
    check("Shift+Down extends the run from the anchor",
            st["table"]["selected"] == [9, 10, 11] and st["table"]["current"] == 11,
            st["table"])
    check("... reported from the keyboard, as a range",
            st["lastKeyboard"] is True and st["lastOp"] == "range", (st["lastKeyboard"], st["lastOp"]))
    press_key("UP", SHIFT)
    check("Shift+Up gives a row back", wait_for(lambda: selected("table") == [9, 10]),
            selected("table"))
    press_key("DOWN")
    wait_for(lambda: selected("table") == [11])
    st = state()
    check("a plain arrow collapses the run onto the next row",
            st["table"]["selected"] == [11] and st["table"]["current"] == 11, st["table"])
    press_key("A", CTRL)
    wait_for(lambda: selected("table") == list(range(ROWS)))
    st = state()
    check("Ctrl+A takes every row", st["table"]["selected"] == list(range(ROWS)), st["table"]["selected"])

    print("-- the tree")
    s.invoke("list-selection.expand", row=0, open=True)
    s.ok("frame", count=2)
    s.invoke("list-selection.expand", row=5, open=True)
    s.ok("frame", count=2)
    check("both branches are open", state()["tree"]["rowCount"] == 10, state()["tree"]["rowCount"])

    click("tree", 1)
    st = after("tree", [1])
    check("a press in the tree takes the scene's selection, and the table's goes",
            st["owner"] == "tree" and st["tree"]["selected"] == [1] and st["table"]["selected"] == [],
            (st["owner"], st["tree"]["selected"], st["table"]["selected"]))
    click("tree", 7, CTRL)
    check("Ctrl adds a row of the other branch", after("tree", [1, 7])["tree"]["selected"] == [1, 7],
            selected("tree"))

    s.invoke("list-selection.expand", row=0, open=False)
    s.ok("frame", count=2)
    st = state()
    check("collapsing a branch hides a selected row and keeps the visible one where it went",
            st["tree"]["rowCount"] == 6 and st["tree"]["selected"] == [3], st["tree"])
    click("tree", 4, CTRL)
    check("an additive press adds to what is shown",
            after("tree", [3, 4])["tree"]["selected"] == [3, 4], selected("tree"))
    s.invoke("list-selection.expand", row=0, open=True)
    s.ok("frame", count=2)
    check("... and kept the hidden row: reopened, it is selected again",
            after("tree", [1, 7, 8])["tree"]["selected"] == [1, 7, 8], selected("tree"))
    click("tree", 2)
    after("tree", [2])
    s.invoke("list-selection.expand", row=0, open=False)
    s.ok("frame", count=2)
    s.invoke("list-selection.expand", row=0, open=True)
    s.ok("frame", count=2)
    check("a plain press drops what was hidden", after("tree", [2])["tree"]["selected"] == [2],
            selected("tree"))

    print("-- the single mode")
    click("single", 3)
    after("single", [3])
    click("single", 5, CTRL)
    check("in the single mode Ctrl is a plain press",
            after("single", [5])["single"]["selected"] == [5], selected("single"))
    click("single", 8, SHIFT)
    check("... and so is Shift", after("single", [8])["single"]["selected"] == [8],
            selected("single"))
    press_key("DOWN", SHIFT)
    press_key("A", CTRL)
    check("Shift+Down and Ctrl+A do nothing to it", selected("single") == [8], selected("single"))

    print("-- a field with the caret")
    click("table", 0)
    after("table", [0])
    s.invoke("list-selection.focus-field")
    s.ok("frame", count=2)
    press_key("A", CTRL)
    wait_for(lambda: state()["fieldSelection"] > 0)
    st = state()
    check("Ctrl+A in the field selects its text",
            st["fieldFocused"] and st["fieldSelection"] == len(st["fieldText"]),
            (st["fieldFocused"], st["fieldSelection"]))
    check("... and not the rows", st["table"]["selected"] == [0], st["table"]["selected"])
finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
