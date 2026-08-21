#!/usr/bin/env python3
"""Drive the ui::Slider stand (XL_SLIDER_TEST) over the inspector socket.

The claim this widget is built on is that it carries a step INDEX and not a fraction, and that
claim is only visible in the differences:

  * a press on the track and an arrow key that land on the same notch produce the SAME number, not
    two that agree to six places - so every value here is compared for equality, never with a
    tolerance;
  * a declared maximum that is not a whole number of steps from the minimum is REPORTED rather than
    trimmed: the last notch of 0..10 by 3 is 9, and both 9 and 10 stay readable;
  * a horizontal slider ignores Up/Down and a vertical one ignores Left/Right, because on a
    horizontal track "up" names no direction;
  * a form collects the VALUE the notch stands for, and collects it as an integer or as a real
    according to what the widget was DECLARED to hold.

    tests/window/slider-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-slider-check.sock")

# What the stand declares. Duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
TRACK_W, TRACK_H = 220.0, 20.0
THUMB = 16.0
TRAVEL = TRACK_W - THUMB          # 204: the handle's centre runs from THUMB/2 to TRACK_W - THUMB/2
V_TRAVEL = TRACK_W - THUMB        # the vertical track is 220 tall and 20 wide

STEPS_MIN, STEPS_MAX, STEPS_STEP = 0.0, 100.0, 5.0
STEPS_MAXINDEX = 20
REAL_STEP = 0.25
REAL_MAXINDEX = 4
UNREACHABLE_MAXINDEX = 3          # 0..10 by 3 -> notches at 0, 3, 6, 9
PAGE_STEPS = 10

# Row geometry of the stand: x = 48, rows 56 apart under the caption, anchored TOP-left.
CAPTION = 76.0
ROW_LEFT = 48.0
ROW_STRIDE = 56.0
ROW0_TOP = 768.0 - CAPTION - 40.0
V_LEFT = ROW_LEFT + TRACK_W + 60.0


def row_top(index):
    return ROW0_TOP - float(index) * ROW_STRIDE


def track_x(fraction):
    """Where the handle's CENTRE stands at `fraction` of the travel - the point a press must
    produce that notch from, computed here rather than asked of the widget."""
    return ROW_LEFT + THUMB / 2.0 + TRAVEL * fraction


def row_y(index):
    return row_top(index) - TRACK_H / 2.0


def vertical_y(fraction):
    # The vertical track spans [top - 220, top]; upward, so the minimum is at the bottom.
    bottom = row_top(0) - TRACK_W
    return bottom + THUMB / 2.0 + V_TRAVEL * fraction


V_X = V_LEFT + TRACK_H / 2.0


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


def press(x, y):
    """A click that does not travel: the track's own gesture, and not a drag."""
    return [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"},
            {"event": "End", "x": x, "y": y, "button": "MouseLeft"}]


def drag(x0, y0, x1, y1, steps=8):
    # A press, a run of moves and a release. The moves have to be several: a swipe is recognized
    # only once the pointer has actually moved, so a Begin alone never starts one.
    ev = [{"event": "Begin", "x": x0, "y": y0, "button": "MouseLeft"}]
    for i in range(1, steps + 1):
        t = float(i) / float(steps)
        ev.append({"event": "Move", "x": x0 + (x1 - x0) * t, "y": y0 + (y1 - y0) * t,
                "button": "MouseLeft"})
    ev.append({"event": "End", "x": x1, "y": y1, "button": "MouseLeft"})
    return ev


def start_app(binary):
    env = dict(os.environ)
    env["XL_SLIDER_TEST"] = "1"
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


def state(name):
    return s.invoke("slider.state")[name]


def settle():
    # Headless renders on demand: a style pass, a relayout and a gesture all need frames to have
    # happened before what they produced can be read back.
    s.ok("frame", count=3)
    time.sleep(0.15)
    s.ok("frame", count=2)


try:
    settle()

    # --- the scale, as declared -----------------------------------------------------------------
    print("\n-- the scale --")
    st = state("steps")
    check("0..100 by 5 has twenty-one notches", st["maxIndex"] == STEPS_MAXINDEX, st["maxIndex"])
    check("and starts at the first one", st["index"] == 0 and st["value"] == 0.0, st)

    st = state("real")
    check("0..1 by 0.25 has five notches", st["maxIndex"] == REAL_MAXINDEX, st["maxIndex"])

    # The claim the whole widget is built on: an author's maximum that is not a whole number of
    # steps away is neither trimmed nor rounded to - it is left unreachable, and SAID to be.
    st = state("unreachable")
    check("0..10 by 3 has four notches, not five", st["maxIndex"] == UNREACHABLE_MAXINDEX,
            st["maxIndex"])
    check("the declared maximum is kept as it was given", st["max"] == 10.0, st["max"])
    check("and the last notch does not reach it - reported, not trimmed",
            st["lastValue"] == 9.0, st["lastValue"])

    # --- an index, not a fraction ---------------------------------------------------------------
    print("\n-- an index, not a fraction --")
    s.invoke("slider.set-index", target="steps", value=10, silent=True)
    check("the notch halfway along is worth exactly 50", state("steps")["value"] == 50.0,
            state("steps")["value"])

    s.invoke("slider.set-value", target="real", value=0.5, silent=True)
    check("a value on a notch lands on it exactly", state("real")["value"] == 0.5,
            state("real")["value"])

    # 0.6 is nearer 0.5 than 0.75; 0.63 is nearer 0.75. Neither is representable as a sum of the
    # widget's own steps, and neither has to be: the widget holds the INDEX.
    s.invoke("slider.set-value", target="real", value=0.6, silent=True)
    st = state("real")
    check("a value between notches takes the nearer one", st["index"] == 2 and st["value"] == 0.5,
            st)
    s.invoke("slider.set-value", target="real", value=0.63, silent=True)
    st = state("real")
    check("and the nearer one on the other side too", st["index"] == 3 and st["value"] == 0.75, st)

    # A tie goes UP, and - the point of the rule - it goes up wherever the scale sits. A negative
    # range is what tells this apart from round(), which ties away from zero and would go DOWN here.
    s.invoke("slider.set-value", target="real", value=0.625, silent=True)
    check("a tie goes to the higher notch", state("real")["index"] == 3, state("real")["index"])

    s.invoke("slider.set-range", target="real", min=-1.0, max=0.0, step=0.25)
    s.invoke("slider.set-value", target="real", value=-0.625, silent=True)
    check("and goes the same way on a range below zero, which round() would not",
            state("real")["value"] == -0.5, state("real")["value"])
    s.invoke("slider.set-range", target="real", min=0.0, max=1.0, step=0.25)

    s.invoke("slider.set-value", target="steps", value=1000.0, silent=True)
    check("a value past the end stops at the last notch",
            state("steps")["index"] == STEPS_MAXINDEX, state("steps")["index"])
    s.invoke("slider.set-value", target="steps", value=-1000.0, silent=True)
    check("and past the other end at the first", state("steps")["index"] == 0,
            state("steps")["index"])

    # --- the geometry follows the index ---------------------------------------------------------
    print("\n-- the geometry --")
    s.invoke("slider.set-index", target="steps", value=0, silent=True)
    settle()
    m = s.invoke("slider.metrics", target="steps")
    check("the handle takes its size from CSS", m["thumbWidth"] == THUMB, m["thumbWidth"])
    check("the travel is the track less the handle", m["travel"] == TRAVEL, m["travel"])
    check("at the first notch the handle sits inside the track, not half outside it",
            m["thumbX"] == THUMB / 2.0, m["thumbX"])
    check("and the fill reaches its centre", m["fillWidth"] == THUMB / 2.0, m["fillWidth"])

    s.invoke("slider.set-index", target="steps", value=STEPS_MAXINDEX, silent=True)
    settle()
    m = s.invoke("slider.metrics", target="steps")
    check("at the last notch the handle stops inside the other end",
            m["thumbX"] == TRACK_W - THUMB / 2.0, m["thumbX"])
    check("and the fill is the whole travel plus the half-handle",
            m["fillWidth"] == TRACK_W - THUMB / 2.0, m["fillWidth"])

    s.invoke("slider.set-index", target="steps", value=10, silent=True)
    settle()
    m = s.invoke("slider.metrics", target="steps")
    check("halfway along, the handle is halfway along the travel",
            m["thumbX"] == THUMB / 2.0 + TRAVEL / 2.0, m["thumbX"])

    # --- a press on the track -------------------------------------------------------------------
    print("\n-- a press on the track --")
    s.invoke("slider.set-index", target="steps", value=0, silent=True)
    s.invoke("slider.reset-counters")
    settle()
    s.ok("input", native=True, events=press(track_x(0.5), row_y(0)))
    settle()
    st = state("steps")
    check("a press halfway along picks the tenth notch", st["index"] == 10, st["index"])
    check("and its value is exactly 50, not near it", st["value"] == 50.0, st["value"])
    check("the press takes focus", st["focused"] is True)
    check("and reports the move once", st["callbacks"] == 1, st["callbacks"])

    s.invoke("slider.reset-counters")
    s.ok("input", native=True, events=press(track_x(0.75), row_y(0)))
    settle()
    st = state("steps")
    check("a press at three quarters picks the fifteenth notch", st["index"] == 15, st["index"])
    check("worth exactly 75", st["value"] == 75.0, st["value"])

    # A press past the end of the track is still a press ON the track: it means the end.
    s.ok("input", native=True, events=press(ROW_LEFT + 2.0, row_y(0)))
    settle()
    check("a press at the very left edge means the first notch", state("steps")["index"] == 0,
            state("steps")["index"])

    # --- a drag -----------------------------------------------------------------------------
    print("\n-- a drag --")
    s.invoke("slider.set-index", target="steps", value=0, silent=True)
    s.invoke("slider.reset-counters")
    settle()
    s.ok("input", native=True,
            events=drag(track_x(0.05), row_y(0), track_x(0.75), row_y(0)))
    settle()
    st = state("steps")
    check("a drag to three quarters ends on the fifteenth notch", st["index"] == 15, st["index"])

    # THE CENTRAL CLAIM: the drag and the key agree EXACTLY, which they could not if the widget
    # held a fraction and multiplied.
    check("and lands on the same number a press did - exactly, not nearly",
            st["value"] == 75.0, st["value"])
    check("the gesture is over when the pointer is up", st["dragging"] is False)
    check("and it reported continuously rather than once at the end", st["callbacks"] > 1,
            st["callbacks"])
    check("the dragging class is gone with it", "dragging" not in st["classes"], st["classes"])

    s.ok("input", native=True,
            events=drag(track_x(0.75), row_y(0), track_x(0.75) + 400.0, row_y(0)))
    settle()
    check("dragging past the end stops at the last notch",
            state("steps")["index"] == STEPS_MAXINDEX, state("steps")["index"])

    # --- the keys ------------------------------------------------------------------------------
    print("\n-- the keys --")
    s.invoke("slider.set-index", target="steps", value=10, silent=True)
    s.invoke("slider.focus", target="steps", value=True)
    s.invoke("slider.reset-counters")
    settle()

    s.ok("input", native=True, events=key("RIGHT"))
    settle()
    st = state("steps")
    check("Right adds exactly one step", st["value"] == 55.0, st["value"])
    check("and reports it once", st["callbacks"] == 1, st["callbacks"])

    s.ok("input", native=True, events=key("LEFT"))
    settle()
    check("Left takes it back", state("steps")["value"] == 50.0, state("steps")["value"])

    # The axis rule: on a horizontal track, "up" names no direction.
    s.ok("input", native=True, events=key("UP") + key("DOWN"))
    settle()
    check("a horizontal slider ignores Up and Down", state("steps")["value"] == 50.0,
            state("steps")["value"])

    s.ok("input", native=True, events=key("PAGE_UP"))
    settle()
    check("PageUp is worth ten steps",
            state("steps")["value"] == 50.0 + STEPS_STEP * PAGE_STEPS, state("steps")["value"])
    s.ok("input", native=True, events=key("PAGE_DOWN"))
    settle()
    check("and PageDown takes them back", state("steps")["value"] == 50.0,
            state("steps")["value"])

    s.ok("input", native=True, events=key("END"))
    settle()
    check("End is the last notch", state("steps")["index"] == STEPS_MAXINDEX,
            state("steps")["index"])
    s.ok("input", native=True, events=key("HOME"))
    settle()
    check("Home is the first", state("steps")["index"] == 0, state("steps")["index"])

    # An unreachable maximum through the keyboard, which is where an author would meet it.
    s.invoke("slider.focus", target="steps", value=False)
    s.invoke("slider.focus", target="unreachable", value=True)
    settle()
    s.ok("input", native=True, events=key("END"))
    settle()
    st = state("unreachable")
    check("End on an unreachable scale stops at the last NOTCH", st["index"] == 3, st["index"])
    check("which is 9 and not the 10 the author declared", st["value"] == 9.0, st["value"])
    s.invoke("slider.focus", target="unreachable", value=False)

    # A slider nobody focused does not answer the keyboard at all.
    s.invoke("slider.set-index", target="steps", value=10, silent=True)
    settle()
    s.ok("input", native=True, events=key("RIGHT"))
    settle()
    check("an unfocused slider does not answer a key", state("steps")["value"] == 50.0,
            state("steps")["value"])

    # --- the other axis --------------------------------------------------------------------------
    print("\n-- the vertical one --")
    s.invoke("slider.set-index", target="vertical", value=0, silent=True)
    s.invoke("slider.focus", target="vertical", value=True)
    settle()

    s.ok("input", native=True, events=key("UP"))
    settle()
    check("a vertical slider answers Up", state("vertical")["value"] == 5.0,
            state("vertical")["value"])
    s.ok("input", native=True, events=key("DOWN"))
    settle()
    check("and Down", state("vertical")["value"] == 0.0, state("vertical")["value"])

    s.ok("input", native=True, events=key("RIGHT") + key("LEFT"))
    settle()
    check("but ignores Left and Right", state("vertical")["value"] == 0.0,
            state("vertical")["value"])
    s.invoke("slider.focus", target="vertical", value=False)

    s.invoke("slider.set-index", target="vertical", value=10, silent=True)
    settle()
    m = s.invoke("slider.metrics", target="vertical")
    check("its handle moves along the height", m["thumbY"] == THUMB / 2.0 + V_TRAVEL / 2.0,
            m["thumbY"])
    check("its fill grows UPWARD from the bottom", m["fillHeight"] == m["thumbY"], m)
    check("and it stays centred across the width", m["thumbX"] == TRACK_H / 2.0, m["thumbX"])

    # A press on the vertical track: the minimum is at the BOTTOM, so a high point is a high value.
    s.invoke("slider.set-index", target="vertical", value=0, silent=True)
    settle()
    s.ok("input", native=True, events=press(V_X, vertical_y(0.75)))
    settle()
    check("a press three quarters UP the vertical track means three quarters of the scale",
            state("vertical")["index"] == 15, state("vertical")["index"])

    # --- silence and no-ops ----------------------------------------------------------------------
    print("\n-- what does not report --")
    s.invoke("slider.set-index", target="steps", value=4, silent=True)
    s.invoke("slider.reset-counters")
    s.invoke("slider.set-index", target="steps", value=8, silent=True)
    check("a silent move does not fire the callback", state("steps")["callbacks"] == 0,
            state("steps")["callbacks"])
    s.invoke("slider.set-index", target="steps", value=8)
    check("and neither does moving to the notch it is already on",
            state("steps")["callbacks"] == 0, state("steps")["callbacks"])
    s.invoke("slider.set-index", target="steps", value=9)
    check("a real move does", state("steps")["callbacks"] == 1, state("steps")["callbacks"])

    # --- the lock --------------------------------------------------------------------------------
    print("\n-- the lock --")
    s.invoke("slider.set-index", target="steps", value=10, silent=True)
    s.invoke("slider.focus", target="steps", value=True)
    s.invoke("slider.lock", target="steps", reason="driven by a wire")
    settle()
    st = state("steps")
    check("a locked slider says so", st["locked"] is True)
    check("and says why", st["lockReason"] == "driven by a wire", st["lockReason"])
    check("it carries the locked class", "locked" in st["classes"], st["classes"])
    check("and the disabled one with it - a locked control IS disabled",
            "disabled" in st["classes"], st["classes"])
    check("a lock takes the focus away", st["focused"] is False)

    s.ok("input", native=True, events=key("RIGHT"))
    s.ok("input", native=True, events=press(track_x(0.9), row_y(0)))
    s.ok("input", native=True,
            events=drag(track_x(0.1), row_y(0), track_x(0.9), row_y(0)))
    settle()
    check("and nothing moves it: not a key, not a press, not a drag",
            state("steps")["index"] == 10, state("steps")["index"])

    s.invoke("slider.lock", target="steps", reason="")
    settle()
    st = state("steps")
    check("unlocking gives it back", st["locked"] is False and st["enabled"] is True, st)
    check("and the classes go with the lock",
            "locked" not in st["classes"] and "disabled" not in st["classes"], st["classes"])

    # A lock restores what the APPLICATION asked for, not "on".
    s.invoke("slider.set-enabled", target="steps", value=False)
    s.invoke("slider.lock", target="steps", reason="held")
    s.invoke("slider.lock", target="steps", reason="")
    settle()
    check("unlocking a slider the application had switched off leaves it off",
            state("steps")["enabled"] is False, state("steps")["enabled"])
    s.invoke("slider.set-enabled", target="steps", value=True)

    # --- the form (E13) ---------------------------------------------------------------------------
    print("\n-- the form --")
    st = s.invoke("slider.state")
    check("the slider is one stop of the tab ring", "form-slider" in st["ring"], st.get("ring"))
    check("the form collects the VALUE, not the index",
            st["collected"]["form-slider"] == 20, st["collected"])
    check("and collects it as a whole number, because the widget was declared to hold one",
            isinstance(st["collected"]["form-slider"], int), st["collected"])

    s.invoke("slider.assign", value=63)
    settle()
    st = s.invoke("slider.state")
    check("assigning a value between notches takes the nearer one",
            st["form"]["index"] == 13 and st["form"]["value"] == 65.0, st["form"])
    check("and does it silently - a form filling itself in is not somebody dragging",
            st["form"]["callbacks"] == 0, st["form"]["callbacks"])
    check("what is collected back is the notch's value",
            st["collected"]["form-slider"] == 65, st["collected"])

    s.invoke("slider.reset")
    settle()
    check("resetting the form clears the slider to its minimum",
            s.invoke("slider.state")["form"]["index"] == 0,
            s.invoke("slider.state")["form"]["index"])

    # A real-valued slider collects a real number even when the notch is a whole one.
    s.invoke("slider.set-value", target="real", value=0.5, silent=True)
    settle()
    st = s.invoke("slider.state")
    check("a real slider reports a real value", st["real"]["integer"] is False)

    # A tap on the track has to move the FORM's focus, or the arrows die in the widget just clicked
    s.invoke("slider.focus", target="steps", value=True)
    settle()
    s.ok("input", native=True, events=press(track_x(0.25), row_y(3)))
    settle()
    st = s.invoke("slider.state")
    check("a press on a form field takes the form's focus with it",
            st.get("formFocus") == "form-slider", st.get("formFocus"))
    check("and the press still picked its notch", st["form"]["index"] == 5, st["form"]["index"])

finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
