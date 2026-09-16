#!/usr/bin/env python3
"""Drive the ui::Chip / ui::ChipRow stand (XL_CHIP_TEST) over the inspector socket.

A row of chips is several values that are ONE value, and everything worth checking about it is
invisible in a screenshot:

  * the ORDER survives removal, assignment and wrapping - an element chain read back in a different
    order describes a different type;
  * the declared limits are declared: at the maximum the "+" is dead and opens nothing, and with
    unique ids the options already in the row come up disabled IN THE MENU rather than being
    refused after the press;
  * removal always has a visible target - Backspace with nothing selected selects, and only the
    next one deletes;
  * the row is one form field: one array under one name, one refusal when Required and empty, and
    Tab that LEAVES it instead of walking its chips;
  * the height the row reports is the height it draws at, at any width.

    tests/window/chip-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-chip-check.sock")

# What the stand declares. Duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
OPTIONS = ["int", "float", "string", "array", "map"]
FREE = ["int", "float", "string"]
LIMITED = ["int", "float"]
NARROW = ["int", "float", "string", "array", "map"]
NARROW_WIDTH = 150.0
WIDE = 420.0


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


def start_app(binary):
    env = dict(os.environ)
    env["XL_CHIP_TEST"] = "1"
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
                sess = Session()
                sess.close()
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


def settle(n=4):
    # A wrap costs two frames on purpose: the placement reports a height, and the measure phase of
    # the NEXT frame is what commits it.
    s.ok("frame", count=n)
    time.sleep(0.15)


def full():
    return s.invoke("chip.state")


# document::InteractiveFlags, duplicated on purpose
F_ENABLED = 1 << 0
F_INVALID = 1 << 5


def row(name="free"):
    return full()[name]


def ids(name="free"):
    return row(name)["ids"]


def popups():
    return [w["id"] for w in s.ok("windows")["windows"] if w["type"] == "Popup"]


def rows_with(tree, cls):
    out = []
    for line in tree.split("\n"):
        if "menu-item" in line and cls in line and "#" in line:
            out.append(line.split("#")[1].split(" ")[0])
    return out


def body(chip):
    # The left quarter of the chip: solidly off the remove button, which sits at the right edge and
    # answers for itself.
    r = chip["rect"]
    return (r["cx"] - r["width"] / 4.0, r["cy"])


def cross(chip):
    r = chip["removeRect"]
    return (r["cx"], r["cy"])


def line_tops(r):
    return sorted({round(c["rect"]["cy"] + c["rect"]["height"] / 2.0, 2) for c in r["chips"]})


try:
    s.ok("frame", count=4)
    time.sleep(0.2)

    print("== what a row holds ==")
    st = full()
    check("the row holds the ids it was given", st["free"]["ids"] == FREE, st["free"]["ids"])
    check("one chip node per member", len(st["free"]["chips"]) == len(FREE))
    check("each one named by its index",
            [c["name"] for c in st["free"]["chips"]] == [f"chip-{i}" for i in range(len(FREE))])
    check("and carrying its title", [c["text"] for c in st["free"]["chips"]] == FREE)
    check("nothing is selected until something selects it", st["free"]["selected"] == -1)
    check("and the row does not hold the keyboard", st["free"]["focused"] is False)
    check("the \"+\" is there, because there are options to offer",
            st["free"]["addVisible"] is True and st["free"]["addEnabled"] is True)

    print("== the wrap ==")
    check("a row that fits is one line", st["free"]["lines"] == 1, st["free"]["lines"])
    narrow = st["narrow"]
    check("a row that does not fit wraps", narrow["lines"] > 1, narrow["lines"])
    check("the chips stand on exactly that many lines",
            len(line_tops(narrow)) == narrow["lines"], line_tops(narrow))
    check("the height it reports is the height it stands at",
            abs(narrow["intrinsicHeight"] - narrow["height"]) < 0.01,
            f'{narrow["intrinsicHeight"]} != {narrow["height"]}')
    occupied = max(c["rect"]["cy"] + c["rect"]["height"] / 2.0 for c in narrow["chips"]) - min(
            c["rect"]["cy"] - c["rect"]["height"] / 2.0 for c in narrow["chips"])
    check("and it covers every chip it wrapped", narrow["height"] >= occupied - 0.01,
            f'{narrow["height"]} < {occupied}')

    s.invoke("chip.reset-counters")
    s.invoke("chip.set-width", target="narrow", value=WIDE)
    settle()
    narrow = row("narrow")
    check("widening it puts them back on one line", narrow["lines"] == 1, narrow["lines"])
    check("and the height comes down with them", narrow["height"] < 60.0, narrow["height"])
    check("the height listener was told, once", narrow["heightReports"] == 1,
            narrow["heightReports"])

    s.invoke("chip.set-width", target="narrow", value=NARROW_WIDTH)
    settle()
    narrow = row("narrow")
    check("narrowing it wraps again", narrow["lines"] > 1, narrow["lines"])
    check("with the same members in the same order", narrow["ids"] == NARROW, narrow["ids"])

    print("== removing ==")
    s.invoke("chip.reset-counters")
    s.invoke("chip.remove", target="free", index=1)
    settle()
    r = row()
    check("removing the middle member leaves the order of the rest",
            r["ids"] == ["int", "string"], r["ids"])
    check("the change was reported once", r["callbacks"] == 1, r["callbacks"])
    check("and it carried the whole row", r.get("lastValue") == "int,string", r.get("lastValue"))
    check("the chips were renumbered with it",
            [c["name"] for c in r["chips"]] == ["chip-0", "chip-1"])

    s.invoke("chip.set-items", target="free", ids=FREE, silent=True)
    settle()
    s.invoke("chip.reset-counters")
    r = row()
    x, y = cross(r["chips"][1])
    s.ok("input", native=True, events=tap(x, y))
    settle()
    r = row()
    check("a tap on the cross takes THAT chip off", r["ids"] == ["int", "string"], r["ids"])
    check("reported once", r["callbacks"] == 1, r["callbacks"])
    check("and it does not select what moved into the gap", r["selected"] == -1, r["selected"])

    x, y = body(r["chips"][1])
    s.ok("input", native=True, events=tap(x, y))
    settle()
    r = row()
    check("a tap on the chip itself selects it", r["selected"] == 1, r["selected"])
    check("which the chip wears", r["chips"][1]["selected"] is True)
    check("and only that one", [c["selected"] for c in r["chips"]] == [False, True])
    check("the tap also gave the row the keyboard", r["focused"] is True)

    print("== the keyboard ==")
    s.invoke("chip.set-items", target="free", ids=FREE, silent=True)
    s.invoke("chip.select", target="free", index=-1)
    s.invoke("chip.focus", target="free", value=True)
    settle()

    s.ok("input", native=True, events=key("RIGHT"))
    settle()
    check("Right from nothing selects the first", row()["selected"] == 0, row()["selected"])
    s.ok("input", native=True, events=key("RIGHT"))
    settle()
    check("and steps on", row()["selected"] == 1, row()["selected"])
    s.ok("input", native=True, events=key("LEFT"))
    settle()
    check("Left steps back", row()["selected"] == 0, row()["selected"])
    s.ok("input", native=True, events=key("LEFT"))
    settle()
    check("and stops at the end rather than wrapping round", row()["selected"] == 0,
            row()["selected"])
    s.ok("input", native=True, events=key("END"))
    settle()
    check("End is the last one", row()["selected"] == len(FREE) - 1, row()["selected"])
    s.ok("input", native=True, events=key("HOME"))
    settle()
    check("Home is the first", row()["selected"] == 0, row()["selected"])

    s.invoke("chip.select", target="free", index=1)
    s.invoke("chip.reset-counters")
    settle()
    s.ok("input", native=True, events=key("DELETE"))
    settle()
    r = row()
    check("Delete takes the selected one off", r["ids"] == ["int", "string"], r["ids"])
    check("and leaves the selection on what took its place", r["selected"] == 1, r["selected"])
    s.ok("input", native=True, events=key("DELETE"))
    settle()
    r = row()
    check("so Delete twice running works", r["ids"] == ["int"], r["ids"])
    check("with the selection on the last one left", r["selected"] == 0, r["selected"])

    s.invoke("chip.set-items", target="free", ids=FREE, silent=True)
    s.invoke("chip.select", target="free", index=-1)
    s.invoke("chip.reset-counters")
    settle()
    s.ok("input", native=True, events=key("BACKSPACE"))
    settle()
    r = row()
    check("Backspace with nothing selected deletes NOTHING", r["ids"] == FREE, r["ids"])
    check("it selects the last one instead", r["selected"] == len(FREE) - 1, r["selected"])
    check("so nothing was reported", r["callbacks"] == 0, r["callbacks"])
    s.ok("input", native=True, events=key("BACKSPACE"))
    settle()
    r = row()
    check("and the next one does take it off", r["ids"] == FREE[:-1], r["ids"])

    s.invoke("chip.set-items", target="free", ids=FREE, silent=True)
    s.invoke("chip.focus", target="free", value=False)
    settle()
    check("blurring clears the selection, which only existed to be deleted",
            row()["selected"] == -1, row()["selected"])
    s.ok("input", native=True, events=key("RIGHT") + key("DELETE"))
    settle()
    r = row()
    check("an unfocused row answers no keys at all",
            r["ids"] == FREE and r["selected"] == -1, f'{r["ids"]} {r["selected"]}')

    print("== the declared limit ==")
    r = row("limited")
    check("the limited row starts under its limit",
            r["ids"] == LIMITED and r["full"] is False, r["ids"])
    check("adding the third one fills it",
            s.invoke("chip.add", target="limited", id="string")["ok"] is True)
    settle()
    r = row("limited")
    check("and it says so", r["full"] is True and "full" in r["classes"], r["classes"])
    check("the \"+\" goes dead", r["addEnabled"] is False)
    check("adding another is refused",
            s.invoke("chip.add", target="limited", id="array")["ok"] is False)
    check("and so is opening the list",
            s.invoke("chip.open", target="limited")["ok"] is False)
    check("nothing was opened", popups() == [], popups())
    s.invoke("chip.remove", target="limited", index=2)
    settle()
    r = row("limited")
    check("back under the limit the \"+\" lives again",
            r["full"] is False and r["addEnabled"] is True and "full" not in r["classes"])

    print("== duplicates ==")
    check("a unique row refuses a member it already has",
            s.invoke("chip.add", target="limited", id="int")["ok"] is False)
    check("a plain row does not - a row is a list, and a chain repeats",
            s.invoke("chip.add", target="free", id="int")["ok"] is True)
    settle()
    check("so the same id can stand twice", ids() == FREE + ["int"], ids())
    s.invoke("chip.remove", target="free", index=0)
    settle()
    check("and taking one off leaves the other", ids() == ["float", "string", "int"], ids())

    print("== the list the \"+\" opens ==")
    s.invoke("chip.open", target="limited")
    settle()
    ps = popups()
    check("opening it makes a window of its own", len(ps) == 1, ps)
    r = row("limited")
    check("the row says it is open", r["open"] is True and "open" in r["classes"], r["classes"])
    if ps:
        s.ok("frame", count=3, window=ps[0])
        tree = s.ok("scene", window=ps[0])["text"]
        present = [o for o in OPTIONS if f"#{o} " in tree]
        check("the list carries one row per option", present == OPTIONS, present)
        # int and float are in the row; with unique ids they are not on offer
        check("what is already in the row comes up DISABLED, not refused after the press",
                sorted(rows_with(tree, ":disabled")) == sorted(LIMITED),
                rows_with(tree, ":disabled"))
    s.invoke("chip.close", target="limited")
    settle()
    r = row("limited")
    check("closing it takes the window with it", popups() == [], popups())
    check("and the class", r["open"] is False and "open" not in r["classes"], r["classes"])

    s.invoke("chip.reset-counters")
    s.invoke("chip.open", target="free")
    settle()
    ps = popups()
    if ps:
        s.ok("frame", count=3, window=ps[0])
        tree = s.ok("scene", window=ps[0])["text"]
        check("a row that allows duplicates offers everything",
                rows_with(tree, ".disabled") == [], rows_with(tree, ".disabled"))
        # Picked with the keyboard, in the list's OWN window - the same route select-check takes,
        # and the only one that does not depend on where a row happens to land.
        s.ok("input", window=ps[0], native=True, events=key("DOWN"))
        # Polled rather than assumed: the key goes app thread -> platform -> the popup's own scene,
        # and three frames are not always enough for the highlight to be in the dump.
        highlighted = []
        for _ in range(10):
            s.ok("frame", count=3, window=ps[0])
            time.sleep(0.1)
            highlighted = rows_with(s.ok("scene", window=ps[0])["text"], ".highlighted")
            if highlighted:
                break
        check("the keyboard walks it", len(highlighted) == 1, highlighted)
        before = ids()
        s.ok("input", window=ps[0], native=True, events=key("ENTER"))
        settle(6)
        r = row()
        check("picking a row adds THAT member, at the end",
                r["ids"] == before + highlighted, f'{r["ids"]} != {before + highlighted}')
        check("reported once", r["callbacks"] == 1, r["callbacks"])
        check("and the list went away with the choice",
                r["open"] is False and popups() == [], popups())
        s.invoke("chip.remove", target="free", index=len(r["ids"]) - 1)
    s.invoke("chip.close", target="free")
    settle()

    print("== the form ==")
    s.invoke("chip.set-items", target="form-chips", ids=FREE, silent=True)
    settle()
    st = full()
    check("the form collects ONE array under one name",
            st["collected"].get("form-chips") == FREE, st["collected"])
    check("beside the neighbour's own value", st["collected"].get("neighbour") == "abc")

    s.invoke("chip.assign", value={"form-chips": ["map", "array", "mystery"]})
    settle()
    st = full()
    check("assigning restores the order it was given",
            st["formRow"]["ids"] == ["map", "array", "mystery"], st["formRow"]["ids"])
    check("an id nothing declares still becomes a chip, titled by itself",
            st["formRow"]["chips"][2]["text"] == "mystery",
            st["formRow"]["chips"][2]["text"])
    check("and is collected back rather than dropped",
            st["collected"].get("form-chips") == ["map", "array", "mystery"], st["collected"])

    s.invoke("chip.reset-counters")
    s.invoke("chip.set-items", target="form-chips", ids=[], silent=True)
    settle()
    check("an empty Required row is refused", s.invoke("chip.submit")["ok"] is False)
    settle()
    st = full()
    check("once, not once per chip", st["invalids"] == 1, st["invalids"])
    check("and the row is the thing marked", (st["formRow"]["stateBits"] & F_INVALID) != 0,
            hex(st["formRow"]["stateBits"]))
    check("nothing was submitted", st["submits"] == 0, st["submits"])

    s.invoke("chip.set-items", target="form-chips", ids=["int", "map"], silent=True)
    settle()
    check("with a member it goes through", s.invoke("chip.submit")["ok"] is True)
    settle()
    st = full()
    check("and what it submitted is the array",
            st["lastSubmit"].get("form-chips") == ["int", "map"], st["lastSubmit"])

    print("== the form's keyboard ==")
    s.invoke("chip.focus-neighbour")
    settle()
    check("the neighbour can be given the caret", full()["neighbourFocused"] is True)

    s.invoke("chip.focus", target="form-chips", value=True)
    settle()
    st = full()
    check("focusing the row moves the form's focus to it", st.get("formFocus") == "form-chips",
            st.get("formFocus"))
    before = st["neighbourCursor"]
    s.ok("input", native=True, events=key("HOME") + key("RIGHT"))
    settle()
    st = full()
    check("the neighbour's caret does not move while the row holds the keyboard",
            st["neighbourCursor"] == before, f'{st["neighbourCursor"]} != {before}')

    s.ok("input", native=True, events=key("TAB", "\t"))
    settle()
    st = full()
    check("Tab LEAVES the row rather than walking its chips",
            st["neighbourFocused"] is True and st.get("formFocus") == "neighbour",
            st.get("formFocus"))
    check("and the row let its selection go with the keyboard",
            st["formRow"]["selected"] == -1, st["formRow"]["selected"])

    s.ok("input", native=True, events=key("TAB", "\t", 1))
    settle()
    st = full()
    check("Shift+Tab comes back to the row", st.get("formFocus") == "form-chips",
            st.get("formFocus"))
    check("and enters it at its LAST chip - the direction the slot carries",
            st["formRow"]["selected"] == len(st["formRow"]["ids"]) - 1,
            st["formRow"]["selected"])

    s.ok("input", native=True, events=key("TAB", "\t"))
    settle()
    st = full()
    check("and Tab takes it away again", st.get("formFocus") == "neighbour", st.get("formFocus"))
    x, y = body(st["formRow"]["chips"][0])
    s.ok("input", native=True, events=tap(x, y))
    settle()
    st = full()
    check("and a tap on a chip moves the form's focus back to the row",
            st.get("formFocus") == "form-chips", st.get("formFocus"))
    check("selecting what was tapped", st["formRow"]["selected"] == 0,
            st["formRow"]["selected"])

    print("== disabled ==")
    s.invoke("chip.set-enabled", target="narrow", value=False)
    settle()
    r = row("narrow")
    check("a disabled row says so", (r["stateBits"] & F_ENABLED) == 0, hex(r["stateBits"]))
    check("its \"+\" is dead with it", r["addEnabled"] is False)
    check("it refuses to open", s.invoke("chip.open", target="narrow")["ok"] is False)
    s.invoke("chip.focus", target="narrow", value=True)
    settle()
    check("and it cannot be focused", row("narrow")["focused"] is False)
    before = ids("narrow")
    x, y = cross(row("narrow")["chips"][0])
    s.ok("input", native=True, events=tap(x, y))
    settle()
    check("a tap on a cross of a disabled row removes nothing", ids("narrow") == before,
            ids("narrow"))
    s.invoke("chip.set-enabled", target="narrow", value=True)
    settle()
    check("turning it back on restores the \"+\"", row("narrow")["addEnabled"] is True)

finally:
    print(f"{checks} checks, {failures} failures")
    s.close()
    proc.kill()

sys.exit(1 if failures else 0)
