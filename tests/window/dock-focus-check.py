#!/usr/bin/env python3
"""Drive the current dock frame (DockSystem::getCurrentFrame) and select-on-press.

The stand (XL_DOCK_FOCUS_TEST) is a tree beside a nested dock; this script presses and types into
it and reads the state back:

  * a press on a selectable panel selects it, and only the deepest frame on the chain is current,
    with its outline node resolved from the stylesheet and sized to the frame;
  * a press on a plain panel selects its frame;
  * a press on a tree row leaves the selection to the tree; arrows move the row and are reported as
    keyboard picks; Enter activates the row;
  * a press on the tree's empty area keeps a selection already in the tree, and otherwise selects
    the frame, from which an arrow enters the tree;
  * a text input taking focus selects the frame it is in;
  * with select-on-press off, a press selects nothing.

    tests/window/dock-focus-check.py [path-to-testapp]

Exits non-zero on any failure.
"""
import json, os, socket, struct, subprocess, sys, time

ENV_NAME = "XL_DOCK_FOCUS_TEST"


class Session:
    def __init__(self, path, timeout=25.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
        # the greeting is a LINE, before any frame - read it to the newline or the first length
        # word is consumed as part of it
        line = b""
        while not line.endswith(b"\n"):
            line += self.s.recv(1)
        assert line.startswith(b"# xenolith/1 ok"), line
        self.serial = 0

    def call(self, cmd, **kw):
        self.serial += 1
        req = {"serial": self.serial, "cmd": cmd}
        req.update(kw)
        payload = json.dumps(req).encode()
        self.s.sendall(struct.pack("<I", len(payload)) + payload)
        size = struct.unpack("<I", self._read(4))[0]
        return json.loads(self._read(size).decode())

    def _read(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.s.recv(n - len(buf))
            if not chunk:
                raise EOFError
            buf += chunk
        return buf

    def invoke(self, command, **args):
        return self.call("invoke", name=command, args=args)["result"]

    def frames(self, count=2):
        self.call("frame", count=count)

    def state(self):
        return self.invoke("dock-focus.state")

    def settle(self, until=None, timeout=3.0):
        """Frames until `until(state)` holds. A tap is reported only once the double-tap window
        has passed, so a fixed frame count would bake this machine's pacing into the test."""
        deadline = time.time() + timeout
        while True:
            self.frames(2)
            st = self.state()
            if until is None or until(st) or time.time() > deadline:
                return st
            time.sleep(0.05)

    def press(self, point, until=None):
        ev = {"x": point[0], "y": point[1], "button": "MouseLeft", "id": 1}
        self.call("input", events=[dict(ev, event="Begin")])
        self.frames(1)
        self.call("input", events=[dict(ev, event="End")])
        return self.settle(until)

    def key(self, code):
        ev = [{"event": "KeyPressed", "keycode": code}, {"event": "KeyReleased", "keycode": code}]
        self.call("input", native=True, events=ev)
        return self.settle()

    def close(self):
        self.s.close()


def logs(path):
    """The one-shot text protocol: send a word, read until EOF."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(path)
    s.sendall(b"logs\n")
    out = b""
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        out += chunk
    s.close()
    return out.decode("utf-8", "replace")


def start_app(binary, addr):
    env = dict(os.environ)
    env[ENV_NAME] = "1"
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


class Checks:
    def __init__(self):
        self.failures = []
        self.count = 0

    def expect(self, cond, what):
        self.count += 1
        if not cond:
            self.failures.append(what)


def wait_ready(s):
    deadline = time.time() + 30.0
    while time.time() < deadline:
        s.frames(2)
        probe = s.invoke("dock-focus.probe")
        if "canvas" in probe and "row0" in probe:
            return probe
        time.sleep(0.05)
    return None


def drive(s, c, probe):
    st = s.state()
    c.expect(st.get("selectOnPress") is True, "select-on-press is not on")
    c.expect(not st.get("outerCurrent") and not st.get("innerCurrent"),
            "a frame is current with nothing selected: %s" % (st,))

    # --- a selectable panel in a nested dock ----------------------------------
    st = s.press(probe["canvas"], lambda st: st.get("innerCurrent"))
    c.expect(st.get("owner") == "canvas", "a press did not select the canvas: %s" % st.get("owner"))
    c.expect(st.get("innerCurrent") == "inner-top", "the canvas frame is not current: %s" % (st,))
    c.expect(not st.get("outerCurrent"), "the frame holding the nested dock is current too")
    top = st.get("inner-top", {})
    c.expect(top.get("outlineVisible") is True, "the current frame's outline is hidden")
    c.expect(top.get("outlineWidth") == 2.0, "the outline rule did not reach the outline node: %s" % top)
    c.expect(top.get("outlineFits") is True, "the outline is not sized to its frame")
    c.expect(top.get("outlineAbove") is True, "the outline is not above the body")
    c.expect(top.get("outlineFillAlpha") == 0, "the outline node paints a fill over the panel")
    # A NON-CURRENT FRAME HAS NO EDGE BECAUSE THE SHEET GIVES IT NONE, not because its outline node
    # is hidden: the node is always visible and paints a transparent fill, so `outline-width` is the
    # whole answer. This stand styles only `.current`, so here that width is zero.
    right = st.get("outer-right", {})
    c.expect(right.get("outlineWidth") == 0.0,
            "a frame that is not current shows an outline: %s" % right)

    # --- a plain panel: its frame is the selection ------------------------------
    st = s.press(probe["plain"], lambda st: st.get("innerCurrent") == "inner-bottom")
    c.expect(st.get("owner") == "inner-bottom", "a press on a plain panel did not select its frame: %s"
            % st.get("owner"))
    c.expect(st.get("innerCurrent") == "inner-bottom", "the plain panel's frame is not current")
    c.expect(st.get("inner-top", {}).get("outlineWidth") == 0.0,
            "the previous frame kept its outline")

    # --- a tree row: the tree selects -----------------------------------------
    st = s.press(probe["row1"], lambda st: st.get("row") == 1 and st.get("outerCurrent"))
    c.expect(st.get("owner") == "tree" and st.get("row") == 1,
            "a press on a row did not select it: %s" % st)
    c.expect(st.get("outerCurrent") == "outer-left" and not st.get("innerCurrent"),
            "the tree's frame is not the only current one: %s" % st)
    c.expect(st.get("keyboardSelects") == 0, "a tap was reported as a keyboard pick")

    selects = st.get("selects", 0)
    st = s.key("DOWN")
    c.expect(st.get("row") == 2, "Down did not move the row: %s" % st.get("row"))
    c.expect(st.get("keyboardSelects") == 1 and st.get("selects") == selects + 1,
            "the arrow pick was not reported as a keyboard pick: %s" % st)
    st = s.key("ENTER")
    c.expect(st.get("activations") == 1, "Enter did not activate the selected row")

    st = s.press(probe["treeEmpty"])
    time.sleep(0.5)
    st = s.settle()
    c.expect(st.get("owner") == "tree" and st.get("row") == 2,
            "a press on the tree's empty area lost the tree's selection: %s" % st)

    # --- the tree's empty area from outside: the frame, then into the tree ----
    s.press(probe["canvas"], lambda st: st.get("innerCurrent"))
    st = s.press(probe["treeEmpty"], lambda st: st.get("outerCurrent"))
    c.expect(st.get("owner") == "outer-left", "the empty area did not select the tree's frame: %s"
            % st.get("owner"))
    c.expect(st.get("outerCurrent") == "outer-left", "the tree's frame is not current")
    st = s.key("DOWN")
    c.expect(st.get("owner") == "tree", "Down from the frame did not enter the tree: %s"
            % st.get("owner"))

    # --- a focused field selects its panel --------------------------------------
    s.invoke("dock-focus.focus-input", value=True)
    st = s.settle(lambda st: st.get("innerCurrent") == "inner-bottom")
    c.expect(st.get("inputFocused") is True, "the text input did not take focus")
    c.expect(st.get("owner") == "inner-bottom" and st.get("innerCurrent") == "inner-bottom"
            and not st.get("outerCurrent"),
            "a focused field did not make its frame current: %s" % st)
    s.invoke("dock-focus.focus-input", value=False)
    s.press(probe["row0"], lambda st: st.get("row") == 0)

    # --- switched off ---------------------------------------------------------
    s.invoke("dock-focus.press-select", value=False)
    s.frames(2)
    st = s.press(probe["canvas"])
    time.sleep(0.5)
    st = s.settle()
    c.expect(st.get("owner") == "tree", "a press selected with select-on-press off: %s"
            % st.get("owner"))
    s.invoke("dock-focus.press-select", value=True)
    s.invoke("dock-focus.clear")
    st = s.settle(lambda st: not st.get("outerCurrent"))
    c.expect(not st.get("outerCurrent") and not st.get("innerCurrent"),
            "a frame stayed current after the selection was cleared")


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

addr = "/tmp/xl-dock-focus-check.sock"
proc = start_app(binary, addr)
s = Session(addr)
c = Checks()
probe = None
try:
    probe = wait_ready(s)
    if probe is not None:
        drive(s, c, probe)
finally:
    s.close()
    proc.kill()
    try:
        os.unlink(addr)
    except OSError:
        pass

if probe is None:
    print("  FAIL dock-focus never built its panels")
    sys.exit(1)

print("  %s dock-focus: %d checks, %d failures" % (
        "ok  " if not c.failures else "FAIL", c.count, len(c.failures)))
for f in c.failures:
    print("       " + f)

sys.exit(0 if not c.failures else 1)
