#!/usr/bin/env python3
"""Drive the ui::NumberField stand (XL_NUMBER_TEST) over the inspector socket.

None of what this widget does is visible in a screenshot. A refused edit and an accepted one that
happened to produce the same number look identical; so does a value that was clamped and one that
was typed. The three claims worth checking are all differences:

  * a number TYPED past the declared range is refused - the value does not move, the callback does
    not fire, and the node takes the `invalid` class;
  * the same number DRAGGED past it is clamped, because a gesture has no wrong state to be in;
  * parse(format(v)) comes back the same number, checked inside the widget rather than against a
    second implementation of its formatting written here.

    tests/window/number-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-number-check.sock")

# What the stand declares. Duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
RANGE_MIN, RANGE_MAX = 0.0, 999.0
REAL_STEP = 0.5

# Row geometry of the stand: x = 48, rows 56 apart under the caption, anchored top-left, 220x36.
CAPTION = 76.0
ROW0_TOP = 768.0 - CAPTION - 40.0


def row_center(index):
    return (48.0 + 110.0, ROW0_TOP - float(index) * 56.0 - 18.0)


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


def key(code, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


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
    env["XL_NUMBER_TEST"] = "1"
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


def state(name):
    return s.invoke("number.state")[name]


try:
    s.ok("frame", count=3)

    # --- what a field says it holds ---------------------------------------------------------------
    st = s.invoke("number.state")
    check("a whole-number field prints no fractional part", st["integer"]["text"] == "10",
            st["integer"]["text"])
    check("a real one does", st["real"]["text"] == "1.5", st["real"]["text"])
    check("the declared range is reported",
            near(st["ranged"]["min"], RANGE_MIN) and near(st["ranged"]["max"], RANGE_MAX))
    check("a form collects a number, not the text of one",
            st["collected"]["form-number"] == 7, st["collected"])

    # --- typing is refused, and says why ----------------------------------------------------------
    s.invoke("number.reset-counters")
    s.invoke("number.set-text", target="ranged", value="1000")
    s.ok("frame", count=2)
    st = state("ranged")
    check("a number past the range is refused", near(st["value"], 100.0), st["value"])
    check("the callback does not fire for a refusal", st["callbacks"] == 0, st["callbacks"])
    check("the field says it is invalid", st["valid"] is False)
    check("and publishes the state a stylesheet paints that with", st["invalidState"],
            st["classes"])
    check("the reason names the range", "between" in st["message"], st["message"])

    # --- blur is where the text and the value have to agree again ---------------------------------
    s.invoke("number.focus", target="ranged", value=False)
    s.ok("frame", count=2)
    st = state("ranged")
    check("losing focus restores the text of a refused edit", st["text"] == "100", st["text"])
    check("and clears the mark", st["valid"] is True and not st["invalidState"])

    # --- typing that is accepted -------------------------------------------------------------------
    s.invoke("number.set-text", target="ranged", value="250")
    s.ok("frame", count=2)
    st = state("ranged")
    check("a number inside the range is taken", near(st["value"], 250.0), st["value"])
    check("and reported once", st["callbacks"] == 1, st["callbacks"])
    check("with the value it took", near(st["lastValue"], 250.0))

    # --- what a whole-number field refuses ----------------------------------------------------------
    s.invoke("number.reset-counters")
    s.invoke("number.set-text", target="integer", value="1.5")
    s.ok("frame", count=2)
    st = state("integer")
    check("a fractional part is refused by a whole-number field", near(st["value"], 10.0),
            st["value"])
    check("and the reason says so", "whole" in st["message"], st["message"])

    s.invoke("number.set-text", target="integer", value="12ab")
    s.ok("frame", count=2)
    st = state("integer")
    check("trailing rubbish is not read as a number", near(st["value"], 10.0), st["value"])
    check("the whole text has to be one", st["message"] == "not a number", st["message"])

    s.invoke("number.set-text", target="integer", value="")
    s.ok("frame", count=2)
    st = state("integer")
    check("an empty field is not a refusal - it is one being retyped", st["valid"] is True,
            st["message"])
    check("and it holds its value", near(st["value"], 10.0), st["value"])

    s.invoke("number.set-text", target="real", value="1.5")
    s.ok("frame", count=2)
    check("a real field takes a fractional part", state("real")["valid"] is True)

    # --- the arrows ----------------------------------------------------------------------------------
    s.invoke("number.reset-counters")
    s.invoke("number.set", target="real", value=1.5)
    s.invoke("number.focus", target="real", value=True)
    s.ok("frame", count=2)
    s.invoke("number.reset-counters")

    s.ok("input", native=True, events=key("UP"))
    time.sleep(0.3)
    st = state("real")
    check("Up adds exactly one step", near(st["value"], 1.5 + REAL_STEP), st["value"])
    check("and reports it once", st["callbacks"] == 1, st["callbacks"])

    s.ok("input", native=True, events=key("DOWN"))
    time.sleep(0.3)
    check("Down takes it back", near(state("real")["value"], 1.5))

    s.ok("input", native=True, events=key("PAGE_UP"))
    time.sleep(0.3)
    check("PageUp is worth ten steps", near(state("real")["value"], 1.5 + REAL_STEP * 10.0),
            state("real")["value"])
    s.invoke("number.focus", target="real", value=False)

    # --- the drag ------------------------------------------------------------------------------------
    s.invoke("number.set", target="ranged", value=100.0)
    s.invoke("number.reset-counters")
    x, y = row_center(2)
    s.ok("input", native=True, events=drag(x, y, 10))
    time.sleep(0.5)
    s.ok("frame", count=3)
    st = state("ranged")
    check("dragging an unfocused field moves the value", st["value"] > 100.0, st["value"])
    check("and does not focus it - a drag is not a click into the text",
            st["focused"] is False)
    check("the gesture is over when the pointer is up", st["dragging"] is False)
    check("and it reported continuously rather than once at the end", st["callbacks"] > 1,
            st["callbacks"])

    # The asymmetry the widget exists to declare: typing past the end is refused, dragging past it
    # stops there.
    s.ok("input", native=True, events=drag(x, y, 200, dx=20.0))
    time.sleep(0.8)
    s.ok("frame", count=3)
    st = state("ranged")
    check("dragging past the range CLAMPS instead of refusing", near(st["value"], RANGE_MAX),
            st["value"])
    check("so the field stays valid", st["valid"] is True and not st["invalidState"])

    s.ok("input", native=True, events=drag(x, y, 200, dx=-20.0))
    time.sleep(0.8)
    s.ok("frame", count=3)
    check("and the other end stops at the minimum", near(state("ranged")["value"], RANGE_MIN),
            state("ranged")["value"])

    s.invoke("number.set-drag", target="ranged", value=False)
    s.invoke("number.set", target="ranged", value=100.0)
    s.ok("input", native=True, events=drag(x, y, 10))
    time.sleep(0.5)
    check("a field with the gesture turned off does not move",
            near(state("ranged")["value"], 100.0), state("ranged")["value"])
    s.invoke("number.set-drag", target="ranged", value=True)

    # --- assignment is not governed by the range ------------------------------------------------------
    s.invoke("number.set", target="ranged", value=5000.0)
    st = state("ranged")
    check("what a program assigns is stored as it stands", near(st["value"], 5000.0), st["value"])
    check("because the range is guidance for whoever edits, not a filter on the data",
            st["valid"] is True)
    s.invoke("number.set", target="ranged", value=100.0)

    # --- reversibility ----------------------------------------------------------------------------------
    values = [0.0, 1.0, -1.0, 0.5, -2.25, 0.001, 12345.678, 1e12, RANGE_MAX]
    trips = s.invoke("number.roundtrip", target="real", values=values)
    bad = [t for t in trips if not near(t["in"], t["out"])]
    check("parse(format(v)) is the same number for every value", not bad, bad)
    check("and every one of them printed something", all(t["text"] for t in trips))

    ints = s.invoke("number.roundtrip", target="integer", values=[0.0, 7.0, -7.0, 1000000.0])
    check("a whole-number field prints whole numbers",
            all("." not in t["text"] and "e" not in t["text"] for t in ints),
            [t["text"] for t in ints])

    # --- a unit is a label beside the number, never part of it -----------------------------------
    before = state("real")
    check("a field with no unit shows none", before["unitText"] == "", before["unitText"])
    text_before, value_before = before["text"], before["value"]

    s.invoke("number.set-unit", target="real", value="px")
    s.ok("frame", count=2)
    st = state("real")
    check("the unit is what it was told", st["unit"] == "px", st["unit"])
    check("and it is what the node shows", st["unitText"] == "px", st["unitText"])
    check("the text did not change", st["text"] == text_before, st["text"])
    check("nor the value", near(st["value"], value_before), st["value"])
    check("and the field is still valid", st["valid"] is True, st["message"])

    # The unit has to cost WIDTH, or it is being drawn on top of the number rather than beside it.
    check("the viewport gave up room for it",
            st["viewportWidth"] < before["viewportWidth"],
            (before["viewportWidth"], st["viewportWidth"]))

    # The contract the unit must not have touched: what commit() reads is the whole text, and the
    # round trip is checked inside the widget rather than against a second formatter written here.
    trips = s.invoke("number.roundtrip", target="real", values=[0.0, 1.5, -2.25, 0.1])
    check("parse(format(v)) still comes back the same number with a unit shown",
            all(near(t["in"], t["out"]) for t in trips), trips)
    check("and the unit is in none of the printed text",
            all("px" not in t["text"] for t in trips), [t["text"] for t in trips])

    s.invoke("number.set-unit", target="real", value="")
    s.ok("frame", count=2)
    st = state("real")
    check("clearing the unit takes the label away", st["unitText"] == "", st["unitText"])
    check("and gives the width back",
            near(st["viewportWidth"], before["viewportWidth"]),
            (before["viewportWidth"], st["viewportWidth"]))

finally:
    try:
        s.call("quit")
    except OSError:
        pass
    s.close()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
