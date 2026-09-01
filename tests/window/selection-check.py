#!/usr/bin/env python3
"""Drive the scene-wide selection (XLSelectionSystem.h) over the inspector.

The stand (XL_SELECTION_TEST) checks the API side itself and signs off with

    [I][SelectionTest] SUMMARY: 88 checks, 0 failures

so the first thing this does is give it frames until that line appears - in headless there is no
time except the frames a driver asks for.

What the stand cannot check is anything that only exists ACROSS FRAMES, and that is the rest of
this script:

  * the chain as a list of node names, deepest first. That is the exact thing a later increment
    publishes into InputListenerStorage for the hotkey pass to walk, so it is worth pinning to a
    literal now, while it is still cheap to change;

  * that holding a selection still for a hundred frames restyles NOTHING. SelectionSystem
    re-resolves the projection on every visit - it has to, because the row a virtualized list
    shows for an identity changes underneath it - and a version that rewrote the markers instead
    of diffing them would pass every check in the stand while dirtying the whole chain at frame
    rate. Only a driver that lets frames pass can see that;

  * that recycling the selected row away and back is likewise stable rather than oscillating: the
    anchor has to change exactly twice, not once per frame.

    tests/window/selection-check.py [path-to-testapp]

Exits non-zero on any failure.
"""
import json, os, socket, struct, subprocess, sys, time

# mods: 4 = Ctrl (see InputModifier)
CTRL = 4

ENV_NAME = "XL_SELECTION_TEST"
TAG = "SelectionTest"
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

    # --- the stand's own vocabulary ---------------------------------------

    def invoke(self, name, **args):
        return self.call("invoke", name=name, args=args)["result"]

    def frames(self, count=2):
        self.call("frame", count=count)

    def state(self):
        return self.invoke("selection.state")

    def select(self, owner, item=None):
        args = {"owner": owner}
        if item is not None:
            args["item"] = item
        return self.invoke("selection.select", **args)

    def materialize(self, owner, item, value):
        return self.invoke("selection.materialize", owner=owner, item=item, value=value)

    def node(self, owner, item=None):
        args = {"owner": owner}
        if item is not None:
            args["item"] = item
        return self.invoke("selection.node", **args)

    def press(self, code, mods=0):
        """Send a chord and read back who was offered it, in order."""
        self.invoke("selection.clear-log")
        self.call("input", events=key(code, mods))
        self.frames(2)
        return self.invoke("selection.hotkey-log")["log"]

    def close(self):
        self.s.close()


def key(code, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


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
    # --- the chain, spelled out --------------------------------------------
    #
    # Deepest first, and all the way to the scene root rather than stopping at the widget: a
    # stylesheet may put :selection-within on any container above the selected item, and the hotkey
    # pass that walks this later has to offer the key to every one of them in this order.
    s.select("list-a", 1)
    s.frames(2)
    st = s.state()
    c.expect(st.get("owner") == "list-a", "the owner is not the list that was selected")
    c.expect(st.get("anchor") == "list-a-row-1", "the anchor is not the selected row")
    chain = st.get("chain", [])
    c.expect(chain[:3] == ["list-a-row-1", "list-a", "root"],
            "the chain does not start at the row and climb through its owner: %s" % (chain,))
    c.expect(len(chain) > 3, "the chain stopped at the widget instead of reaching the scene root")

    # ...and the same chain read out of the COMMITTED FRAME, which is the only copy a hotkey may be
    # dispatched against. The two are allowed to differ by one frame while a change is in flight;
    # once frames have passed they must agree exactly, or the dispatcher is walking a chain that
    # does not exist
    c.expect(st.get("published") == chain,
            "the published chain does not match the live one: %s vs %s" % (
                    st.get("published"), chain))

    # --- holding still must cost nothing ------------------------------------
    #
    # The projection is re-resolved on every visit. Diffed, it writes nothing; rewritten, it would
    # restyle the whole chain at frame rate - and would look perfectly correct in any single frame.
    before = s.state()
    s.frames(100)
    after = s.state()
    c.expect(after.get("dirty") == before.get("dirty"),
            "holding one selection for 100 frames restyled the chain: %s -> %s" % (
                    before.get("dirty"), after.get("dirty")))
    c.expect(after.get("chain") == before.get("chain"),
            "the chain drifted while nothing changed")
    c.expect(after.get("anchor") == before.get("anchor"), "the anchor drifted while nothing changed")

    # --- the virtualized case, across frames --------------------------------
    #
    # Recycling the row away must move the anchor up to the owner exactly once and then hold, not
    # flap between the two as each frame re-resolves it.
    s.materialize("list-a", 1, False)
    s.frames(3)
    st = s.state()
    c.expect(st.get("anchor") == "list-a",
            "an unmaterialized selection did not fall back to its owner")
    c.expect(st.get("count") == 1, "recycling the row away dropped the selection")
    c.expect(st.get("chain", [])[:2] == ["list-a", "root"],
            "the fallback chain is wrong: %s" % (st.get("chain"),))
    c.expect(st.get("published") == st.get("chain"),
            "the fallback chain was not published")

    settled = s.state()
    s.frames(100)
    c.expect(s.state().get("dirty") == settled.get("dirty"),
            "an unmaterialized selection restyled the chain every frame")

    s.materialize("list-a", 1, True)
    s.frames(3)
    st = s.state()
    c.expect(st.get("anchor") == "list-a-row-1",
            "a re-materialized row did not become the anchor again")
    c.expect(s.node("list-a", 1).get("selected") is True,
            "a re-materialized row did not get :selected back")

    # --- one selection, scene-wide ------------------------------------------
    s.select("list-b", 0)
    s.frames(2)
    st = s.state()
    c.expect(st.get("owner") == "list-b", "the second list did not take the selection")
    c.expect(s.node("list-a", 1).get("selected") is False,
            "the first list's row kept :selected after another list took the selection")
    c.expect(s.node("list-a").get("selection-within") is False,
            "the first list kept :selection-within")
    c.expect(s.node("root").get("selection-within") is True,
            "the ancestor shared by both lists lost :selection-within")

    # A plain node that is its own identity: it is both owner and item, so the chain simply starts
    # at it
    s.select("plain")
    s.frames(2)
    st = s.state()
    c.expect(st.get("owner") == "plain" and st.get("anchor") == "plain",
            "selectNode did not make the node its own owner and anchor")
    c.expect(st.get("chain", [])[:2] == ["plain", "root"],
            "the plain node's chain is wrong: %s" % (st.get("chain"),))

    # --- and down again ------------------------------------------------------
    s.select("")
    s.frames(2)
    st = s.state()
    c.expect(st.get("empty") is True, "the selection survived a clear")
    c.expect(st.get("chain", []) == [], "a cleared selection left a chain behind")
    # The storage is reused frame to frame, so a missed clear() there leaves the previous chain
    # standing in a frame with nothing selected - silent until a SelectedOnly hotkey fires later
    c.expect(st.get("published") == [], "a cleared selection left a chain in the committed frame")
    c.expect(s.node("root").get("selection-within") is False,
            "a cleared selection left :selection-within on the ancestor")

    # Nothing selected must stay nothing selected, however many frames pass
    empty = s.state()
    s.frames(50)
    c.expect(s.state().get("dirty") == empty.get("dirty"),
            "an empty selection restyled something")

    # --- the chain pass ------------------------------------------------------
    #
    # Every subscriber DECLINES, so one press walks all the way through and the log is the complete
    # delivery order rather than just whoever got there first. That is the only way the order is
    # observable at all: a consuming subscriber would stop the walk at itself.
    #
    # Ctrl+J is offered to everyone; Ctrl+H carries SelectedOnly.

    # With nothing selected there is no chain, so the ordinary walk is all there is - and it must be
    # exactly what it was before any of this existed
    s.select("")
    s.frames(3)
    plain_order = s.press("J", CTRL)
    c.expect("row-1" in plain_order and "layout" in plain_order,
            "the ordinary walk did not reach everybody: %s" % (plain_order,))
    c.expect(plain_order.index("list-a") < plain_order.index("root") < plain_order.index("layout"),
            "the ordinary walk is not in paint order: %s" % (plain_order,))

    # A SelectedOnly binding with nothing selected is offered to NOBODY. Not "offered and declined":
    # the whole point of the flag is that the chord is left for whoever is below
    c.expect(s.press("H", CTRL) == [],
            "a SelectedOnly hotkey fired with nothing selected: %s" % (s.press("H", CTRL),))

    # Now select a row. The user's requirement, literally: the selected element's own listener
    # first, then its parents, then the container - and only after all of them, the rest of the
    # scene in its ordinary order
    s.select("list-a", 1)
    s.frames(3)
    order = s.press("J", CTRL)
    c.expect(order[:4] == ["row-1", "list-a", "root", "layout"],
            "the chain was not offered the key deepest-first: %s" % (order,))
    c.expect(len(order) > 4, "the walk stopped at the chain instead of continuing past it")
    c.expect(len(order) == len(set(order)),
            "a chain listener was offered the same key twice: %s" % (order,))
    c.expect(order.index("list-b") > 3, "an off-chain listener was offered the key before the chain")

    # SelectedOnly now fires, and ONLY along the chain
    sel_order = s.press("H", CTRL)
    c.expect(sel_order == ["row-1", "list-a", "root", "layout"],
            "SelectedOnly was not confined to the chain, deepest-first: %s" % (sel_order,))

    # Declining is what lets the next one have it; consuming stops the walk exactly there. Both
    # halves matter - the engine contract is that a handler with nothing to do returns false
    s.invoke("selection.set-consume", subscriber="list-a", value=True)
    stopped = s.press("H", CTRL)
    c.expect(stopped == ["row-1", "list-a"],
            "a consuming subscriber did not stop the walk at itself: %s" % (stopped,))
    s.invoke("selection.set-consume", subscriber="list-a", value=False)

    # The deepest link of the chain comes and goes as the list virtualizes. With the row recycled
    # away the chain starts at the owner, and the container still gets its key - which is the
    # "not into the unknown" claim: an Undo reaches the history of the thing that is selected even
    # when the thing has no node on screen
    s.materialize("list-a", 1, False)
    s.frames(3)
    recycled = s.press("H", CTRL)
    c.expect(recycled == ["list-a", "root", "layout"],
            "an unmaterialized selection did not still reach its owner: %s" % (recycled,))
    s.materialize("list-a", 1, True)
    s.frames(3)

    # And the whole thing goes away again with the selection
    s.select("")
    s.frames(3)
    c.expect(s.press("H", CTRL) == [], "SelectedOnly survived the selection being cleared")
    c.expect(s.press("J", CTRL) == plain_order,
            "the ordinary walk did not return to exactly what it was before anything was selected")


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

addr = "/tmp/xl-selection-check.sock"
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
    print("  FAIL selection     never reached its summary")
    sys.exit(1)

stand_ok = " 0 failures" in summary
print("  %s selection     stand: %s" % ("ok  " if stand_ok else "FAIL",
        summary.split("SUMMARY: ")[-1]))
print("  %s selection     frames: %d checks, %d failures" % (
        "ok  " if not c.failures else "FAIL", c.count, len(c.failures)))
for f in c.failures:
    print("       " + f)

sys.exit(0 if stand_ok and not c.failures else 1)
