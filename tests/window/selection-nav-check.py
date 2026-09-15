#!/usr/bin/env python3
"""Drive arrow navigation of the scene's selection (SelectionSystem::moveSelection).

The stand (XL_SELECTION_NAV_TEST) checks the direction score itself and signs off with a SUMMARY
line; this script sends real arrow keys and reads the selection back:

  * an arrow moves between plain selectable nodes by geometry, and stops at the edge;
  * auto-repeat keeps moving;
  * an arrow enters a tree on the closest row, steps through its rows, expands and collapses, and
    leaves it at its edge, including into another tree below;
  * in a right-to-left tree Left and Right swap their tree meaning;
  * the tree scrolls the selected row into view;
  * a node that is not drawn is not a candidate;
  * an arrow taken by the ordinary key route, or held back by an Exclusive focus group, does not
    move the selection.

    tests/window/selection-nav-check.py [path-to-testapp]

Exits non-zero on any failure.
"""
import json, os, socket, struct, subprocess, sys, time

ENV_NAME = "XL_SELECTION_NAV_TEST"
TAG = "SelectionNavTest"
TIMEOUT = 30.0


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
        return self.invoke("selection-nav.state")

    def select(self, **args):
        self.invoke("selection-nav.select", **args)
        self.frames(3)

    def arrow(self, code, repeats=0):
        """An arrow through the window's own input path; `repeats` auto-repeats before release."""
        ev = [{"event": "KeyPressed", "keycode": code}]
        ev += [{"event": "KeyRepeated", "keycode": code}] * repeats
        ev.append({"event": "KeyReleased", "keycode": code})
        self.call("input", native=True, events=ev)
        self.frames(3)
        return self.state()

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


def wait_for_summary(s, addr):
    """Frames until the stand signs off. Waiting a fixed number instead would bake this machine's
    frame pacing into the test."""
    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        s.frames(2)
        time.sleep(0.05)
        for line in logs(addr).splitlines():
            if "SUMMARY" in line and TAG in line:
                return line.strip()
    return None


def drive(s, c):
    def owner(st):
        return st.get("owner")

    # --- plain nodes, by geometry -------------------------------------------
    s.select(name="cell-1-1")
    c.expect(owner(s.arrow("RIGHT")) == "cell-1-2", "Right did not move to the next cell")
    c.expect(owner(s.arrow("LEFT")) == "cell-1-1", "Left did not come back")
    c.expect(owner(s.arrow("UP")) == "cell-0-1", "Up did not move to the row above")
    c.expect(owner(s.arrow("DOWN")) == "cell-1-1", "Down did not move to the row below")

    s.select(name="cell-1-0")
    c.expect(owner(s.arrow("LEFT")) == "cell-1-0", "Left past the edge moved the selection")
    c.expect(s.invoke("selection-nav.move", dir="left").get("ok") is False,
            "moveSelection past the edge did not answer false")

    s.select(name="cell-0-0")
    c.expect(owner(s.arrow("DOWN", repeats=1)) == "cell-2-0",
            "an auto-repeated Down did not keep moving")

    # --- the chain follows a reparented selection ----------------------------
    s.select(name="cell-2-2")
    s.invoke("selection-nav.reparent", value=True)
    s.frames(3)
    chain = s.state().get("chain", [])
    c.expect(chain[:2] == ["cell-2-2", "box"],
            "the chain did not follow the selected node into its new parent: %s" % (chain,))
    s.invoke("selection-nav.reparent", value=False)
    s.frames(3)
    chain = s.state().get("chain", [])
    c.expect("box" not in chain, "the old parent kept the chain: %s" % (chain,))

    # --- the ordinary key route and modal scopes win -------------------------
    s.select(name="cell-1-1")
    s.invoke("selection-nav.eat", value=True)
    s.frames(2)
    st = s.arrow("RIGHT")
    c.expect(owner(st) == "cell-1-1", "an arrow the key route took still moved the selection")
    c.expect(st.get("eaten", 0) >= 1, "the arrow listener never received the key")
    s.invoke("selection-nav.eat", value=False)
    s.frames(2)
    c.expect(owner(s.arrow("RIGHT")) == "cell-1-2", "an arrow nobody took did not move")

    s.invoke("selection-nav.modal", value=True)
    s.frames(3)
    c.expect(owner(s.arrow("LEFT")) == "cell-1-2",
            "an arrow moved the selection behind an Exclusive focus group")
    s.invoke("selection-nav.modal", value=False)
    s.frames(3)
    c.expect(owner(s.arrow("LEFT")) == "cell-1-1", "the arrow did not come back after the modal")

    # --- only drawn nodes are candidates -------------------------------------
    s.select(name="cell-2-0")
    st = s.arrow("DOWN")
    c.expect(owner(st) == "tree-rtl", "Down from the grid did not reach the tree below: %s" % owner(st))
    s.invoke("selection-nav.hidden", value=True)
    s.select(name="cell-2-0")
    c.expect(owner(s.arrow("DOWN")) == "cell-hidden",
            "a drawn node in the beam did not win over one outside it")
    s.invoke("selection-nav.hidden", value=False)
    s.frames(3)

    # --- into a tree, through it, and out ------------------------------------
    s.select(name="cell-1-2")
    st = s.arrow("RIGHT")
    c.expect(owner(st) == "tree" and st["tree"].get("label") == "cat-b",
            "entering the tree did not pick the closest row: %s" % (st.get("tree"),))
    st = s.arrow("UP")
    c.expect(st["tree"].get("label") == "cat-a", "Up inside the tree did not step a row")
    st = s.arrow("RIGHT")
    c.expect(st["tree"].get("label") == "cat-a" and st["tree"].get("expanded") is True,
            "Right on a closed category did not expand it in place")
    st = s.arrow("RIGHT")
    c.expect(st["tree"].get("label") == "cat-a-0", "Right on an open category did not enter it")
    st = s.arrow("LEFT")
    c.expect(st["tree"].get("label") == "cat-a", "Left on a child did not go to its parent")

    st = s.arrow("DOWN")
    for _ in range(6):
        st = s.arrow("DOWN")
    c.expect(st["tree"].get("label") == "cat-b", "Down did not walk the rows: %s" % (st["tree"],))
    c.expect(st["tree"].get("scroll", 0) > 0, "the selected row was not scrolled into view")
    c.expect(st.get("anchorType") == "tree-row", "the scrolled-to row has no node")

    s.arrow("UP")
    s.arrow("UP")
    for _ in range(6):
        s.arrow("UP")
    st = s.arrow("LEFT")
    c.expect(st["tree"].get("label") == "cat-a" and st["tree"].get("expanded") is False,
            "Left on an open category did not collapse it: %s" % (st["tree"],))
    st = s.arrow("LEFT")
    c.expect(owner(st) == "cell-0-2", "Left at a top-level row did not leave the tree: %s" % owner(st))

    s.select(tree="tree", row=1)
    st = s.arrow("DOWN")
    c.expect(owner(st) == "tree-rtl" and st["tree-rtl"].get("label") == "cat-a",
            "Down at the last row did not enter the tree below on its first row: %s" % (
                    st.get("tree-rtl"),))

    # --- right to left --------------------------------------------------------
    st = s.arrow("LEFT")
    c.expect(st["tree-rtl"].get("expanded") is True, "Left did not expand in a right-to-left tree")
    st = s.arrow("LEFT")
    c.expect(st["tree-rtl"].get("label") == "cat-a-0", "Left did not enter the category in RTL")
    st = s.arrow("RIGHT")
    c.expect(st["tree-rtl"].get("label") == "cat-a", "Right did not go to the parent in RTL")

    s.select()


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

addr = "/tmp/xl-selection-nav-check.sock"
proc = start_app(binary, addr)
s = Session(addr)
c = Checks()
summary = None
try:
    summary = wait_for_summary(s, addr)
    if summary is not None:
        drive(s, c)
finally:
    s.close()
    proc.kill()
    try:
        os.unlink(addr)
    except OSError:
        pass

if summary is None:
    print("  FAIL selection-nav never reached its summary")
    sys.exit(1)

stand_ok = " 0 failures" in summary
print("  %s selection-nav stand: %s" % ("ok  " if stand_ok else "FAIL",
        summary.split("SUMMARY: ")[-1]))
print("  %s selection-nav keys: %d checks, %d failures" % (
        "ok  " if not c.failures else "FAIL", c.count, len(c.failures)))
for f in c.failures:
    print("       " + f)

sys.exit(0 if stand_ok and not c.failures else 1)
