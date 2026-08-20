#!/usr/bin/env python3
"""Drive the ui::VectorField stand (XL_VECTOR_TEST) over the inspector socket.

A row of number fields is not interesting for what it looks like; it is interesting for what it is
from the outside, and every one of those claims is a number rather than a picture:

  * it is ONE form field - one array, under one name, in one stop of the tab ring;
  * Tab walks the components and only leaves the row at its ends, and a Shift+Tab that ENTERS the
    row lands on its LAST component (which is what FormFieldSlots::setFocused's `backwards` says);
  * a refusal in one component marks the ROW, names the component, and leaves the rest alone;
  * changing the arity keeps the values that still have an index.

    tests/window/vector-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-vector-check.sock")

# What the stand declares. Duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
RANGE_MIN, RANGE_MAX = 0.0, 999.0


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


def key(code, char=None, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    if char is not None:
        ev["keychar"] = char
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def tap(x, y):
    return [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"},
            {"event": "End", "x": x, "y": y, "button": "MouseLeft"}]


def drag(x, y, steps, dx=8.0):
    # A press, a run of moves and a release. The moves have to be several: a swipe is recognized
    # only after the pointer has travelled far enough to be one.
    ev = [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"}]
    for i in range(1, steps + 1):
        ev.append({"event": "Move", "x": x + i * dx, "y": y, "button": "MouseLeft"})
    ev.append({"event": "End", "x": x + steps * dx, "y": y, "button": "MouseLeft"})
    return ev


def start_app(binary):
    env = dict(os.environ)
    env["XL_VECTOR_TEST"] = "1"
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


def near(a, b, eps=1e-9):
    return abs(a - b) <= eps


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()


def settle(n=3):
    # Focus is not a frame away: it travels app thread -> TextInputManager -> back as an echo, and
    # only the echo makes it true. Frames alone let a check read the state one Tab behind.
    s.ok("frame", count=n)
    time.sleep(0.15)


def full():
    return s.invoke("vector.state")


def state(name):
    return full()[name]


def center(row, index):
    r = state(row)["rects"][index]
    return (r["cx"], r["cy"])


try:
    s.ok("frame", count=3)

    print("== what a row holds ==")
    st = full()
    check("a row reports one value per component", st["real"]["values"] == [1.0, 2.0, 3.0],
            st["real"]["values"])
    check("and carries its width as a style class", "arity-3" in st["real"]["classes"],
            st["real"]["classes"])
    check("the components are named x/y/z by default", st["real"]["labels"] == ["x", "y", "z"],
            st["real"]["labels"])
    check("a whole-number row prints no fractional part", st["formField"]["texts"] == ["1", "2",
            "3", "4"], st["formField"]["texts"])

    print("== one form field, not N ==")
    check("the form collects ONE key for the whole row", sorted(st["collected"].keys()) ==
            ["form-vector", "neighbour"], list(st["collected"].keys()))
    check("and its value is an array", st["collected"]["form-vector"] == [1, 2, 3, 4],
            st["collected"]["form-vector"])
    check("a whole-number row collects integers, not reals",
            all(isinstance(v, int) for v in st["collected"]["form-vector"]),
            st["collected"]["form-vector"])

    s.invoke("vector.assign", value={"form-vector": [9, 8]})
    s.ok("frame", count=2)
    check("assigning the wrong length moves nothing",
            state("formField")["values"] == [1.0, 2.0, 3.0, 4.0], state("formField")["values"])

    s.invoke("vector.assign", value={"form-vector": [5, 6, 7, 8]})
    s.ok("frame", count=2)
    check("assigning the right length takes", state("formField")["values"] == [5.0, 6.0, 7.0, 8.0],
            state("formField")["values"])
    check("and it is silent - the form filling a field is not somebody editing it",
            state("formField")["callbacks"] == 0, state("formField")["callbacks"])

    print("== a component is edited, and only it ==")
    s.invoke("vector.reset-counters")
    s.invoke("vector.set-text", target="real", index=2, value="42.5")
    s.ok("frame", count=2)
    st = state("real")
    check("the component that was typed into took the value", near(st["values"][2], 42.5),
            st["values"])
    check("and the others did not move", st["values"][0:2] == [1.0, 2.0], st["values"])
    check("the row reported the change once", st["callbacks"] == 1, st["callbacks"])
    check("with the WHOLE vector, which is what the row means",
            st["lastValue"] == [1.0, 2.0, 42.5], st.get("lastValue"))

    print("== a refusal marks the row and names the component ==")
    s.invoke("vector.reset-counters")
    s.invoke("vector.set-text", target="ranged", index=1, value="1000")
    s.ok("frame", count=2)
    st = state("ranged")
    check("a number past the declared range is refused", near(st["values"][1], 20.0), st["values"])
    check("the callback does not fire for a refusal", st["callbacks"] == 0, st["callbacks"])
    check("the ROW says it is invalid", st["valid"] is False)
    check("and takes the class a stylesheet paints that with", "invalid" in st["classes"],
            st["classes"])
    check("the message names the component that refused", st["message"].startswith("y:"),
            st["message"])
    check("and keeps the reason the component gave", "between" in st["message"], st["message"])
    check("the component beside it is untouched", st["componentValid"][0] is True and
            near(st["values"][0], 10.0), st["values"])

    s.invoke("vector.set-text", target="ranged", index=1, value="500")
    s.ok("frame", count=2)
    st = state("ranged")
    check("fixing the component clears the row's mark", st["valid"] is True and
            "invalid" not in st["classes"], st["classes"])
    check("and the value is taken", near(st["values"][1], 500.0), st["values"])

    print("== the keyboard walks the row before it leaves it ==")
    s.invoke("vector.focus", target="form-vector", index=0)
    settle()
    st = full()
    check("focusing a component focuses the row", st["formField"]["focused"] == 0,
            st["formField"]["focused"])
    check("and the FORM's focused field is the row itself", st.get("formFocus") == "form-vector",
            st.get("formFocus"))
    check("and the ROW paints :focus, not only the component in it",
            st["formField"]["focusState"] is True)

    s.ok("input", events=key("TAB", "\t"), native=True)
    settle()
    st = full()
    check("tab steps to the next component", st["formField"]["focused"] == 1,
            st["formField"]["focused"])
    check("and stays inside the row", st["neighbourFocused"] is False)

    s.ok("input", events=key("TAB", "\t") + key("TAB", "\t"), native=True)
    settle()
    check("two more tabs reach the last component", state("formField")["focused"] == 3,
            state("formField")["focused"])

    s.ok("input", events=key("TAB", "\t"), native=True)
    settle()
    st = full()
    check("tab off the last component leaves the row", st["formField"]["focused"] == -1,
            st["formField"]["focused"])
    check("and lands on the field after it", st["neighbourFocused"] is True)
    check("which the form agrees about", st.get("formFocus") == "neighbour", st.get("formFocus"))

    print("== shift+tab enters the row at its LAST component ==")
    s.ok("input", events=key("TAB", "\t", mods=1), native=True)
    settle()
    st = full()
    check("backwards navigation entered the row", st["formField"]["focused"] == 3,
            st["formField"]["focused"])
    check("and the neighbour gave the keyboard up", st["neighbourFocused"] is False)

    s.ok("input", events=key("TAB", "\t", mods=1), native=True)
    settle()
    check("shift+tab walks back inside the row", state("formField")["focused"] == 2,
            state("formField")["focused"])

    s.invoke("vector.focus", target="form-vector", index=0)
    settle()
    s.ok("input", events=key("TAB", "\t", mods=1), native=True)
    settle()
    st = full()
    check("shift+tab off the first component leaves the row", st["formField"]["focused"] == -1,
            st["formField"]["focused"])
    check("and wraps to the other end of the ring", st["neighbourFocused"] is True)

    print("== the row owns the keyboard while it holds it ==")
    before = full()["neighbourCursor"]
    s.invoke("vector.focus", target="form-vector", index=1)
    settle()
    s.ok("input", events=key("HOME") + key("LEFT"), native=True)
    settle()
    st = full()
    check("the caret of the field beside the row did not move",
            st["neighbourCursor"] == before, f'{st["neighbourCursor"]} != {before}')
    check("and its text is intact", st["neighbourText"] == "abc", st["neighbourText"])

    print("== a tap makes the form follow ==")
    s.invoke("vector.focus", target="form-vector", value=False)
    s.invoke("vector.focus-neighbour")
    settle()
    check("the neighbour has the keyboard again", full()["neighbourFocused"] is True)

    x, y = center("formField", 2)
    s.ok("input", events=tap(x, y), native=True)
    settle(4)
    st = full()
    check("tapping a component focuses it", st["formField"]["focused"] == 2,
            st["formField"]["focused"])
    check("and the form's focused field follows the tap", st.get("formFocus") == "form-vector",
            st.get("formFocus"))

    # The point of the claim above: without it the form would go on filtering keys to the field it
    # focused last, and the key below would never reach the component under the pointer.
    s.invoke("vector.reset-counters")
    s.ok("input", events=key("UP"), native=True)
    settle()
    st = state("formField")
    check("so the arrows reach the tapped component", near(st["values"][2], 8.0), st["values"])
    check("and only that component stepped", st["values"][0] == 5.0 and st["values"][1] == 6.0,
            st["values"])

    print("== the arity can change under the values ==")
    s.invoke("vector.set", target="real", values=[1.0, 2.0, 3.0])
    s.invoke("vector.set-arity", target="real", value=4)
    s.ok("frame", count=3)
    st = state("real")
    check("widening keeps every value that still has an index",
            st["values"] == [1.0, 2.0, 3.0, 0.0], st["values"])
    check("and the fourth component is named w", st["labels"] == ["x", "y", "z", "w"],
            st["labels"])
    check("the style class follows the width", "arity-4" in st["classes"] and
            "arity-3" not in st["classes"], st["classes"])

    s.invoke("vector.set-arity", target="real", value=2)
    s.ok("frame", count=3)
    st = state("real")
    check("narrowing keeps the values that remain", st["values"] == [1.0, 2.0], st["values"])
    r = s.invoke("vector.set-arity", target="real", value=0)
    check("a row of nothing is refused", r["ok"] is False)

    print("== labels are the row's, and can be taken away ==")
    s.invoke("vector.set-labels", target="real", values=["lo", "hi"])
    s.ok("frame", count=2)
    check("labels can be replaced", state("real")["labels"] == ["lo", "hi"],
            state("real")["labels"])
    s.invoke("vector.set-labels", target="real", values=[])
    s.ok("frame", count=2)
    check("and removed entirely", state("real")["labels"] == ["", ""], state("real")["labels"])

    print("== dragging is clamped, not refused ==")
    s.invoke("vector.set", target="ranged", values=[10.0, 995.0])
    s.invoke("vector.reset-counters")
    s.ok("frame", count=2)
    x, y = center("ranged", 1)
    s.ok("input", events=drag(x, y, 12), native=True)
    settle()
    st = state("ranged")
    check("a drag past the end of the range stops at it", near(st["values"][1], RANGE_MAX),
            st["values"])
    check("the row is NOT marked - a gesture has no wrong state to be in", st["valid"] is True,
            st["message"])
    check("the component beside it did not move", near(st["values"][0], 10.0), st["values"])
    check("and the drag reported continuously", st["callbacks"] > 1, st["callbacks"])

    print("== the unit belongs to the ROW, once ==")
    before = state("real")
    check("a row with no unit shows none", before["unitText"] == "", before["unitText"])
    labels_before = before["labels"]
    right_before = before["rects"][-1]["x"] + before["rects"][-1]["width"]

    s.invoke("vector.set-unit", target="real", values=[], value="m")
    s.ok("frame", count=2)
    st = state("real")
    check("the row shows the unit it was told", st["unitText"] == "m", st["unitText"])
    check("exactly once - it is not a label per component",
            st["labels"] == labels_before, (labels_before, st["labels"]))
    check("and it did not join the component labels",
            len(st["labels"]) == st["arity"], (st["arity"], st["labels"]))

    # It has to take room from the components, not sit on top of the last one.
    right_after = st["rects"][-1]["x"] + st["rects"][-1]["width"]
    check("the last component gave up room for it", right_after < right_before,
            (right_before, right_after))
    widths = [r["width"] for r in st["rects"]]
    check("and the components stayed equal width", max(widths) - min(widths) < 1.0, widths)

    # setArity tears down the components and their labels; the row's unit is not one of them.
    s.invoke("vector.set-arity", target="real", value=4)
    s.ok("frame", count=2)
    st = state("real")
    check("widening the row does not lose the unit", st["unitText"] == "m", st["unitText"])
    check("and the labels grew with the arity", len(st["labels"]) == 4, st["labels"])

    s.invoke("vector.set-unit", target="real", value="")
    s.ok("frame", count=2)
    check("clearing takes it away", state("real")["unitText"] == "")

finally:
    print(f"{checks} checks, {failures} failures")
    s.close()
    proc.kill()

sys.exit(1 if failures else 0)
