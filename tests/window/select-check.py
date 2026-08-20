#!/usr/bin/env python3
"""Drive the ui::Select stand (XL_SELECT_TEST) over the inspector socket.

Everything here runs headless. The load-bearing facts are the ones a screenshot cannot show:

  * the open list is a WINDOW of its own, so the keys that walk it are delivered to that window and
    not to the control - which is why the navigation lives in ui::MenuSystem and not here;
  * while the list is up, the menu's exclusive focus group owns the keyboard, and the text field
    standing beside the control must not see a single arrow;
  * a form collects the option's id, not the title a person reads.

    tests/window/select-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-select-check.sock")

# The option list the stand declares, duplicated here on purpose: a check that reads its
# expectations out of the thing it is checking cannot fail.
OPTIONS = ["bool", "int", "float", "vec2", "vec3", "vec4", "color", "string", "bytes", "nil",
        "array", "map", "enum"]
DISABLED = "nil"
INITIAL = "int"

# Where the stand puts the first control: x = 48, rows 60 apart under the caption, anchored at their
# top-left, 200x34.
CAPTION = 76.0
ROW0_TOP = 768.0 - CAPTION - 40.0
SELECT_CENTER = (48.0 + 100.0, ROW0_TOP - 17.0)


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


def tap(x, y):
    return [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"},
            {"event": "End", "x": x, "y": y, "button": "MouseLeft"}]


def start_app(binary):
    env = dict(os.environ)
    env["XL_SELECT_TEST"] = "1"
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


def popups(s):
    return [w["id"] for w in s.ok("windows")["windows"] if w["type"] == "Popup"]


def rows_with(tree, cls):
    out = []
    for line in tree.split("\n"):
        if "menu-item" in line and cls in line and "#" in line:
            out.append(line.split("#")[1].split(" ")[0])
    return out


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()
try:
    s.ok("frame", count=3)

    # --- the closed face ------------------------------------------------------------------------
    st = s.invoke("select.state")
    check("the control carries every option", st["select"]["optionCount"] == len(OPTIONS),
            st["select"]["optionCount"])
    check("it shows the chosen option's title", st["select"]["label"] == "Int",
            st["select"]["label"])
    check("and reports the chosen option's id", st["select"]["value"] == INITIAL)

    s.invoke("select.set", value="color")
    s.ok("frame", count=2)
    st = s.invoke("select.state")
    check("choosing another option moves the label with it", st["select"]["label"] == "Color",
            st["select"]["label"])

    check("an id nothing carries is refused",
            s.invoke("select.set", value="nosuchtype")["ok"] is False)
    check("and the value is left alone", s.invoke("select.state")["select"]["value"] == "color")

    # --- stepping -------------------------------------------------------------------------------
    s.invoke("select.set", value="bytes")
    s.invoke("select.step", delta=1)
    st = s.invoke("select.state")
    check("a step skips a disabled option", st["select"]["value"] == "array",
            st["select"]["value"])

    s.invoke("select.set", value="enum")
    check("stepping past the last option does nothing",
            s.invoke("select.step", delta=1)["ok"] is False)
    check("and the value stays where it was",
            s.invoke("select.state")["select"]["value"] == "enum")

    s.invoke("select.set", value=INITIAL)
    s.invoke("select.reset-counters")

    # --- the list is a surface ------------------------------------------------------------------
    s.invoke("select.open")
    time.sleep(0.8)
    ids = popups(s)
    check("opening the list produces a window of its own", len(ids) == 1, ids)
    root = ids[0]

    st = s.invoke("select.state")
    check("the control knows it is open", st["select"]["open"] is True)
    check("the list is a native surface (headless emulates the WM)",
            st["select"].get("popupNative") is True)

    s.ok("frame", count=3, window=root)
    tree = s.ok("scene", window=root)["text"]
    present = [o for o in OPTIONS if f"#{o} " in tree]
    check("the list carries one row per option", present == OPTIONS, present)
    check("the disabled option is rendered, and marked for CSS",
            f"#{DISABLED} " in tree and ".disabled" in tree)

    check("the chosen option is checked", rows_with(tree, ".checked") == [INITIAL],
            rows_with(tree, ".checked"))
    check("and the keyboard starts on it", rows_with(tree, ".highlighted") == [INITIAL],
            rows_with(tree, ".highlighted"))

    # --- the keyboard walks the list, in the list's own window ----------------------------------
    s.ok("input", window=root, native=True, events=key("DOWN"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    tree = s.ok("scene", window=root)["text"]
    check("Down moves the highlight", rows_with(tree, ".highlighted") == ["float"],
            rows_with(tree, ".highlighted"))

    # float is the third option; six more reach the last one before the disabled row
    s.ok("input", window=root, native=True, events=key("DOWN") * 6)
    time.sleep(0.4)
    s.ok("frame", count=3, window=root)
    check("further steps walk on down the list",
            rows_with(s.ok("scene", window=root)["text"], ".highlighted") == ["bytes"],
            rows_with(s.ok("scene", window=root)["text"], ".highlighted"))

    s.ok("input", window=root, native=True, events=key("DOWN"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    check("the next one skips the disabled option",
            rows_with(s.ok("scene", window=root)["text"], ".highlighted") == ["array"],
            rows_with(s.ok("scene", window=root)["text"], ".highlighted"))

    s.ok("input", window=root, native=True, events=key("END"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    check("End goes to the last option",
            rows_with(s.ok("scene", window=root)["text"], ".highlighted") == ["enum"])

    s.ok("input", window=root, native=True, events=key("HOME"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    check("Home goes to the first",
            rows_with(s.ok("scene", window=root)["text"], ".highlighted") == ["bool"])

    # --- the neighbour must not see any of this --------------------------------------------------
    before = s.invoke("select.state")["neighbourCursor"]
    s.ok("input", window=root, native=True, events=key("END") + key("HOME"))
    time.sleep(0.3)
    check("the field beside the control never saw the arrows",
            s.invoke("select.state")["neighbourCursor"] == before, before)

    # --- choosing --------------------------------------------------------------------------------
    # The highlight is on the first option and the value is on the second, so this also says that
    # Enter takes what the KEYBOARD is on rather than what was already chosen.
    s.ok("input", window=root, native=True, events=key("ENTER"))
    time.sleep(0.8)
    st = s.invoke("select.state")
    check("Enter chooses the highlighted option", st["select"]["value"] == "bool",
            st["select"]["value"])
    check("the label followed", st["select"]["label"] == "Bool", st["select"]["label"])
    check("the change was reported exactly once", st["changes"] == 1, st["changes"])
    check("and the list is gone", popups(s) == [] and st["select"]["open"] is False)

    # --- Escape ----------------------------------------------------------------------------------
    s.invoke("select.reset-counters")
    s.invoke("select.open")
    time.sleep(0.8)
    root = popups(s)[0]
    s.ok("frame", count=3, window=root)
    s.ok("input", window=root, native=True, events=key("DOWN") + key("DOWN"))
    time.sleep(0.3)
    s.ok("input", window=root, native=True, events=key("ESCAPE"))
    time.sleep(0.8)
    st = s.invoke("select.state")
    check("Escape closes the list", popups(s) == [] and st["select"]["open"] is False)
    check("and changes nothing", st["select"]["value"] == "bool" and st["changes"] == 0,
            (st["select"]["value"], st["changes"]))

    # --- the closed control answers the keyboard itself -------------------------------------------
    s.invoke("select.set", value=INITIAL)
    s.invoke("select.reset-counters")
    s.invoke("select.focus", value=True)
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("DOWN"))
    time.sleep(0.3)
    st = s.invoke("select.state")
    check("Down on the closed control steps the value", st["select"]["value"] == "float",
            st["select"]["value"])
    check("without opening anything", popups(s) == [])

    s.ok("input", native=True, events=key("UP"))
    time.sleep(0.3)
    check("Up steps back", s.invoke("select.state")["select"]["value"] == "int")

    st = s.invoke("select.state")
    check("focus is written once, not twice", st["select"]["focusCounter"] == 1,
            st["select"]["focusCounter"])
    check("and CSS can see it", st["select"]["focusFlag"] is True)

    s.ok("input", native=True, events=key("SPACE"))
    time.sleep(0.8)
    check("Space opens the list", len(popups(s)) == 1)
    s.invoke("select.close")
    time.sleep(0.5)

    # --- a tap is the other way in -----------------------------------------------------------------
    s.ok("input", native=True, events=tap(SELECT_CENTER[0], SELECT_CENTER[1]))
    time.sleep(0.8)
    check("a tap opens the list", len(popups(s)) == 1, popups(s))
    s.invoke("select.close")
    time.sleep(0.5)

    # --- disabled ----------------------------------------------------------------------------------
    s.invoke("select.set-enabled", value=False)
    s.ok("frame", count=2)
    check("a disabled control refuses to open", s.invoke("select.open")["ok"] is False)
    check("and it is marked for CSS",
            "disabled" in s.ok("scene")["text"].split("select #select")[1].split("\n")[0])
    s.invoke("select.set-enabled", value=True)

    # --- the form ------------------------------------------------------------------------------------
    st = s.invoke("select.state")
    check("a form collects the id, not the title", st["collected"]["form-select"] == "",
            st["collected"])

    s.invoke("select.set", target="form-select", value="map")
    st = s.invoke("select.state")
    check("choosing puts the id into what the form collects",
            st["collected"]["form-select"] == "map", st["collected"])

    s.invoke("select.assign", value="color")
    st = s.invoke("select.state")
    check("and assigning the form's value moves the control",
            st["formSelect"]["value"] == "color" and st["formSelect"]["label"] == "Color",
            st["formSelect"])

    # --- a plain list of names -----------------------------------------------------------------------
    # Last, and on the form control rather than #select: the disabled check above splits the scene
    # dump on the FIRST "select #select", so nothing here may disturb what precedes it.
    NAMES = ["alpha", "beta", "gamma"]
    s.invoke("select.set-string-options", target="form-select", values=NAMES)
    s.ok("frame", count=2)
    st = s.invoke("select.state")
    check("a list of names becomes that many options",
            st["formSelect"]["optionCount"] == len(NAMES), st["formSelect"])
    check("choosing a name by id works, and the face shows the same word",
            s.invoke("select.set", target="form-select", value="beta")["ok"] is True)
    st = s.invoke("select.state")
    check("id and title are the same word",
            st["formSelect"]["value"] == "beta" and st["formSelect"]["label"] == "beta",
            st["formSelect"])
    check("and the form collects that id", st["collected"]["form-select"] == "beta",
            st["collected"])

    # The invariant the helper must not have bypassed: setOptions drops a value the new list can no
    # longer resolve. "beta" is not in this one.
    s.invoke("select.set-string-options", target="form-select", values=["one", "two"])
    s.ok("frame", count=2)
    st = s.invoke("select.state")
    check("a value the new list cannot resolve is dropped", st["formSelect"]["value"] == "",
            st["formSelect"])

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
