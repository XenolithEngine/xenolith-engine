#!/usr/bin/env python3
"""Drive the ui::ColorField stand (XL_COLOR_TEST) over the inspector socket.

A colour field has two pickers and one value, and what is worth checking is never how it looks:

  * WHICH picker a tap opens. Headless advertises no colour dialog at all, so `auto` has to resolve
    to the widget's own surface - and `system`, asked for explicitly, has to fail with a reason
    rather than open nothing and go quiet;
  * that the hex line reads whatever sprt::geom::readColor reads, because it IS that function and
    not a second parser written for this widget;
  * that Enter and blur mean different things about a refusal;
  * that inside a form the whole control is ONE field whose value is hex text.

    tests/window/color-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, re, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-color-check.sock")

# What the stand declares. Duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
PLAIN = "#1e88e5"
ALPHA = "#ff00aa80"
FORM = "#43a047"
ALPHA_PALETTE = ["#ff0000", "#00ff00", "#0000ff"]


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
    env["XL_COLOR_TEST"] = "1"
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


def settle(n=3):
    # Focus and dialogs are not a frame away: they travel app thread -> platform -> back.
    s.ok("frame", count=n)
    time.sleep(0.15)


def full():
    return s.invoke("color.state")


def state(name):
    return full()[name]


def popups():
    return [w["id"] for w in s.ok("windows")["windows"] if w["type"] == "Popup"]


# The picker's own window carries its geometry in its scene dump, in that window's coordinates: a
# node line is "<type> #<name> ... sz=WxH pos=(X,Y)", anchored top-left.
NODE_RE = re.compile(r"#([\w-]+).*sz=([\d.]+)x([\d.]+) pos=\(([-\d.]+),([-\d.]+)\)")


def picker_nodes(window):
    s.ok("frame", count=3, window=window)
    out = {}
    for line in s.ok("scene", window=window)["text"].split("\n"):
        m = NODE_RE.search(line)
        if m:
            name, w, h, x, y = m.group(1), *(float(v) for v in m.groups()[1:])
            out.setdefault(name, (x + w / 2.0, y - h / 2.0))
    return out


try:
    s.ok("frame", count=3)

    print("== what it holds ==")
    st = full()
    check("the value is printed as canonical hex", st["plain"]["value"] == PLAIN,
            st["plain"]["value"])
    check("the hex line shows it", st["plain"]["text"] == PLAIN, st["plain"]["text"])
    check("and the swatch paints it", st["plain"]["swatchColor"].startswith(PLAIN),
            st["plain"]["swatchColor"])
    check("an alpha field prints eight digits", st["alpha"]["value"] == ALPHA,
            st["alpha"]["value"])
    check("a plain one prints six", len(st["plain"]["value"]) == 7, st["plain"]["value"])
    check("and the swatch of a half-transparent colour carries its alpha",
            st["alpha"]["swatchColor"] == ALPHA, st["alpha"]["swatchColor"])

    print("== which picker, and why ==")
    check("headless has no system colour dialog", st["plain"]["systemAvailable"] is False)

    s.invoke("color.open", target="plain")
    settle()
    st = full()
    ids = popups()
    check("so `auto` opens the widget's own surface", len(ids) == 1, ids)
    check("the field knows it is open", st["plain"]["open"] is True)
    check("and says so to a stylesheet", "open" in st["plain"]["classes"], st["plain"]["classes"])
    check("the surface is named after the field", st["plain"].get("pickerWindow") in ids,
            st["plain"].get("pickerWindow"))

    nodes = picker_nodes(ids[0])
    palette = state("plain")["palette"]
    swatches = [n for n in nodes if n.startswith("swatch-")]
    check("it holds one swatch per palette entry", len(swatches) == len(palette),
            f"{len(swatches)} vs {len(palette)}")
    check("and a hex line of its own", "hex" in nodes, sorted(nodes))
    check("and a preview of the current colour", "preview" in nodes)

    s.invoke("color.close", target="plain")
    settle()
    check("closing takes the surface down", popups() == [], popups())
    check("and clears the class", "open" not in state("plain")["classes"])

    print("== the system picker, where there is none ==")
    s.invoke("color.set-mode", target="plain", value="system")
    r = s.invoke("color.open", target="plain")
    settle()
    st = state("plain")
    check("asking for it explicitly refuses", r["ok"] is False)
    check("nothing opens", popups() == [], popups())
    check("the field is not left thinking it is open", st["open"] is False)
    check("the value did not move", st["value"] == PLAIN, st["value"])
    # The refusal is reported through its OWN channel, not through validation. This assertion
    # moved here from `message` when the two were split: a platform without a colour dialog says
    # nothing whatever about the colour the field is holding, and marking the field `invalid`
    # sent the author looking for a typo in a value that had none.
    check("and the refusal says why", "system" in st["unavailableMessage"],
            st["unavailableMessage"])
    check("through the unavailable channel", st["unavailable"] is True)
    check("and CSS can see it", "unavailable" in st["classes"], st["classes"])
    check("the field is NOT marked invalid - the value is fine, the way in is missing",
            st["valid"] is True and not st["invalidState"], st)
    check("and validation has nothing to say", st["message"] == "", st["message"])
    s.invoke("color.set-mode", target="plain", value="auto")
    s.invoke("color.set", target="plain", value=PLAIN)

    print("== choosing from the surface ==")
    s.invoke("color.reset-counters")
    s.invoke("color.open", target="alpha")
    settle()
    ids = popups()
    nodes = picker_nodes(ids[0])
    x, y = nodes["swatch-1"]
    s.ok("input", window=ids[0], native=True, events=tap(x, y))
    settle()
    st = state("alpha")
    check("clicking a swatch takes that colour", st["value"].startswith(ALPHA_PALETTE[1]),
            st["value"])
    check("the surface goes away with it", popups() == [], popups())
    check("and it is reported exactly once", st["callbacks"] == 1, st["callbacks"])

    s.invoke("color.reset-counters")
    s.invoke("color.open", target="alpha")
    settle()
    ids = popups()
    s.ok("input", window=ids[0], native=True, events=key("ESCAPE"))
    settle()
    st = state("alpha")
    check("Escape closes the surface", popups() == [], popups())
    check("and chooses nothing", st["callbacks"] == 0, st["callbacks"])

    print("== the hex line reads what CSS reads ==")
    for text, expect in (("#f0a", "#ff00aa"), ("#ff00aa", "#ff00aa"),
            ("rgb(255,0,170)", "#ff00aa"), ("teal", "#008080")):
        s.invoke("color.set", target="plain", value=text)
        settle(1)
        check(f"`{text}` is read", state("plain")["value"] == expect, state("plain")["value"])

    print("== Enter keeps a refusal, blur puts the value back ==")
    s.invoke("color.set", target="plain", value=PLAIN)
    s.invoke("color.reset-counters")
    s.invoke("color.focus", target="plain", value=True)
    settle()
    s.invoke("color.set-text", target="plain", value="not a colour")
    s.ok("input", native=True, events=key("ENTER", "\r"))
    settle()
    st = state("plain")
    check("Enter on rubbish refuses it", st["value"] == PLAIN, st["value"])
    check("the callback does not fire", st["callbacks"] == 0, st["callbacks"])
    check("the field is marked", st["valid"] is False and st["invalidState"],
            st["classes"])
    check("with a reason", st["message"] != "", st["message"])
    check("and the text the user typed is still there", st["text"] == "not a colour", st["text"])

    s.invoke("color.focus", target="plain", value=False)
    settle()
    st = state("plain")
    check("blur puts the value's own text back", st["text"] == PLAIN, st["text"])
    check("and clears the mark", st["valid"] is True and "invalid" not in st["classes"],
            st["classes"])

    s.invoke("color.focus", target="plain", value=True)
    settle()
    s.invoke("color.set-text", target="plain", value="#00897b")
    s.ok("input", native=True, events=key("ENTER", "\r"))
    settle()
    st = state("plain")
    check("Enter on a colour takes it", st["value"] == "#00897b", st["value"])
    check("and reports it once", st["callbacks"] == 1, st["callbacks"])
    s.invoke("color.focus", target="plain", value=False)
    settle()

    print("== printing and reading are inverse ==")
    values = ["#000000", "#ffffff", "#1e88e5", "#010203", "#fedcba"]
    trips = s.invoke("color.roundtrip", target="plain", values=values)
    ok = all(t.get("reread") and t["out"] == t["text"] for t in trips)
    check("readColor(formatColor(c)) == c for every colour", ok, trips)
    alpha_trips = s.invoke("color.roundtrip", target="alpha",
            values=["#ff00aa80", "#00000000", "#ffffffff"])
    ok = all(t.get("reread") and t["out"] == t["text"] for t in alpha_trips)
    check("and with an alpha channel too", ok, alpha_trips)

    print("== one form field, and its value is text ==")
    st = full()
    check("the form collects one key for the whole control",
            sorted(st["collected"].keys()) == ["form-color", "neighbour"],
            list(st["collected"].keys()))
    check("and its value is the hex", st["collected"]["form-color"] == FORM,
            st["collected"]["form-color"])

    s.invoke("color.assign", value={"form-color": "#8e24aa"})
    settle()
    check("assigning parses the text", state("formField")["value"] == "#8e24aa",
            state("formField")["value"])
    check("and is silent", state("formField")["callbacks"] == 0,
            state("formField")["callbacks"])

    s.invoke("color.assign", value={"form-color": "nonsense"})
    settle()
    check("a string no colour reader takes leaves the value alone",
            state("formField")["value"] == "#8e24aa", state("formField")["value"])

    print("== the field beside it ==")
    s.invoke("color.focus", target="form-color", value=True)
    settle()
    st = full()
    check("focusing the hex line focuses the FIELD", st.get("formFocus") == "form-color",
            st.get("formFocus"))
    check("and the control paints :focus", st["formField"]["focused"] is True)

    before = st["neighbourCursor"]
    s.ok("input", native=True, events=key("HOME") + key("LEFT"))
    settle()
    st = full()
    check("the caret of the neighbour does not move while the colour holds the keyboard",
            st["neighbourCursor"] == before, f'{st["neighbourCursor"]} != {before}')

    s.ok("input", native=True, events=key("TAB", "\t"))
    settle()
    st = full()
    check("Tab out of the hex line reaches the neighbour", st["neighbourFocused"] is True)
    check("which the form agrees about", st.get("formFocus") == "neighbour", st.get("formFocus"))

    x, y = state("formField")["inputRect"]["cx"], state("formField")["inputRect"]["cy"]
    s.ok("input", native=True, events=tap(x, y))
    settle(4)
    st = full()
    check("a tap in the hex line moves the form's focus back", st.get("formFocus") == "form-color",
            st.get("formFocus"))

finally:
    print(f"{checks} checks, {failures} failures")
    s.close()
    proc.kill()

sys.exit(1 if failures else 0)
