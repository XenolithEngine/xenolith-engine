#!/usr/bin/env python3
"""Drive the ui::FormSystem stand (XL_FORM_TEST) over the inspector socket and assert.

Everything here runs headless: no display, no compositor, no mouse.

Two timing facts shape the whole script. A focus change is DEFERRED: FormSystem::focusField only
records the request, and the focus group applies it on the next commit - after which a text field
still has to acquire the IME and wait for the echo. So anything that moves focus is followed by
step(3). And every mutating command answers with a bare ack; state is always read back separately.

    tests/window/form-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-form.sock")


class Session:
    def __init__(self, path=ADDR, timeout=10.0):
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


def start_app(binary):
    env = dict(os.environ)
    env["XL_FORM_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    return subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


DEFAULT_BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

FAIL = []
CHECKS = 0


def expect(cond, what, extra=""):
    global CHECKS
    CHECKS += 1
    if not cond:
        FAIL.append(f"{what} {extra}")
        print(f"  FAIL  {what} {extra}")
    else:
        print(f"  ok    {what}")


def connect():
    for _ in range(40):
        try:
            return Session()
        except OSError:
            time.sleep(0.5)
    raise SystemExit("app never came up")


APP = start_app(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY)
s = connect()


def state():
    return s.invoke("form.state", settle=0.0)


def field_state(name):
    return s.invoke("form.field-state", field=name, settle=0.0)


def step(n=1):
    s.ok("frame", count=n)
    time.sleep(0.15)


# The clipboard is not a frame away: a write travels app thread -> context thread -> the platform's
# selection owner, and a read travels back. A paste that runs before the write has landed simply
# reads nothing and is over - waiting afterwards cannot help, because nothing re-reads. So the
# paste itself is what gets retried, select-all first so an attempt replaces the previous one
# instead of appending to it. The ceiling still fails loudly.
def paste_until(name, expected, seconds=8.0):
    deadline = time.monotonic() + seconds
    while True:
        s.ok("input", events=key("A", "a", mods=4), native=True)
        step(2)
        s.ok("input", events=key("V", "v", mods=4), native=True)
        step(4)
        value = field_state(name)["text"]
        if value == expected or time.monotonic() > deadline:
            return value


def focus(name):
    s.invoke("form.focus", field=name, settle=0.0)
    step(3)


def fill(name, text):
    s.invoke("form.set-text", field=name, text=text, settle=0.0)
    step()


ALL_FIELDS = ["name", "email", "subscribe", "notes", "hidden", "submit", "reset"]

# The form's fields are registered as their listeners enter the scene, so the ring only exists
# after the first commit.
step(3)

print("== 1. the tab ring is in document order ==")
st = state()
expect(st["tabRing"] == ALL_FIELDS, "ring lists every field in document order", st["tabRing"])
expect(st["focused"] == "", "an untouched form takes no focus on its own", repr(st["focused"]))

# Captured HERE, where the assertion above has just established that nothing has been focused,
# hovered or clicked yet: this is what each widget looks like to the style resolver before anything
# touches it. Asserted at the end - a component created lazily on first focus would look identical
# by then, which is exactly the bug that has to stay caught.
UNTOUCHED_FIELDS = st["fields"]

print("== 2. a collapsed subtree drops out of the ring ==")
s.invoke("form.set-visible", visible=False, settle=0.0)
step(2)
st = state()
expect("hidden" not in st["tabRing"], "the collapsed field left the ring", st["tabRing"])
expect(len(st["tabRing"]) == 6, "the rest of the ring is intact", st["tabRing"])
s.invoke("form.set-visible", visible=True, settle=0.0)
step(2)
expect(state()["tabRing"] == ALL_FIELDS, "restoring it puts the field back")

print("== 3. a disabled field drops out of the ring ==")
s.invoke("form.set-enabled", field="subscribe", enabled=False, settle=0.0)
step(2)
st = state()
expect("subscribe" not in st["tabRing"], "the disabled field left the ring", st["tabRing"])
s.invoke("form.set-enabled", field="subscribe", enabled=True, settle=0.0)
step(2)
expect(state()["tabRing"] == ALL_FIELDS, "re-enabling it puts the field back")

print("== 4. tab from a focused text field ==")
fill("name", "Ann")
focus("name")
expect(state()["focused"] == "name", "the field took focus")
s.ok("input", events=key("TAB", "\t"), native=True)
step(3)
st = state()
expect(st["focused"] == "email", "tab moved to the next field", st["focused"])
fs = field_state("name")
expect(fs["text"] == "Ann", "tab did not type into the field it left", repr(fs["text"]))
expect(not fs["focused"], "the field it left released text input", fs["focused"])

# A focus change is only RECORDED by focusField; it is applied on the next commit. Two Tabs in one
# key batch therefore both see the same committed focus, and before this was fixed the second step
# was silently swallowed - Tab Tab moved one field, not two.
print("== 4b. two tabs in a single batch step twice ==")
focus("name")
s.ok("input", events=key("TAB", "\t") + key("TAB", "\t"), native=True)
step(3)
st = state()
expect(st["focused"] == "subscribe", "both tabs counted, not just the first", st["focused"])

# focus-next reports the state on its own app-thread hop, before any commit can intervene - a
# separate `state` call would arrive a frame later and only ever see the settled result.
print("== 4c. a step is recorded before it commits ==")
focus("name")
r = s.invoke("form.focus-next", backwards=False, settle=0.0)
expect(r["focused"] == "name", "focus has not moved yet", r["focused"])
expect(r["pending"] == "email", "but the request is already recorded", r["pending"])
step(3)
st = state()
expect(st["focused"] == "email", "and it lands on the next commit", st["focused"])
expect(st["pending"] == "", "the request is consumed", repr(st["pending"]))

print("== 5. shift+tab walks back ==")
s.ok("input", events=key("TAB", "\t", mods=1), native=True)
step(3)
expect(state()["focused"] == "name", "shift+tab moved back", state()["focused"])

print("== 6. tab wraps around ==")
focus("reset")
s.ok("input", events=key("TAB", "\t"), native=True)
step(3)
expect(state()["focused"] == "name", "tab from the last field wrapped to the first",
       state()["focused"])

print("== 7. tab from a widget with no key handling of its own ==")
focus("subscribe")
expect(state()["focused"] == "subscribe", "the checkbox took focus")
s.ok("input", events=key("TAB", "\t"), native=True)
step(3)
expect(state()["focused"] == "notes", "tab moved on from the checkbox", state()["focused"])

print("== 8. space toggles a focused checkbox ==")
focus("subscribe")
before = state()["fields"]["subscribe"]["value"]
s.ok("input", events=key("SPACE", " "), native=True)
step(2)
st = state()
expect(st["fields"]["subscribe"]["value"] != before, "space toggled the checkbox",
       st["fields"]["subscribe"]["value"])

print("== 9. :focus for a widget that does not paint it itself ==")
st = state()
expect(st["fields"]["subscribe"]["interactive"]["focus"], "the checkbox has :focus")
expect(st["fields"]["subscribe"]["interactive"]["focusCounter"] == 1,
       "the focus counter was written exactly once",
       st["fields"]["subscribe"]["interactive"]["focusCounter"])
focus("name")
st = state()
expect(not st["fields"]["subscribe"]["interactive"]["focus"], "the checkbox lost :focus")
expect(st["fields"]["subscribe"]["interactive"]["focusCounter"] == 0,
       "and the counter came back to zero",
       st["fields"]["subscribe"]["interactive"]["focusCounter"])

print("== 10. a text field's own :focus is not double-counted ==")
focus("name")
focus("email")
focus("name")
st = state()
expect(st["fields"]["name"]["interactive"]["focus"], "the field has :focus")
expect(st["fields"]["name"]["interactive"]["focusCounter"] == 1,
       "one writer only - the widget, not the listener too",
       st["fields"]["name"]["interactive"]["focusCounter"])

# These two are the regression guards for the focus group. A plain SingleFocus group filters by
# listener id, which starves the widget's OWN listener the moment its form listener takes focus -
# and then nothing below works.
print("== 11. the widget keeps its keys while the form holds focus ==")
fill("name", "abcdef")
focus("name")
s.ok("input", events=key("END"), native=True)
step()
s.ok("input", events=key("LEFT", mods=1), native=True)
s.ok("input", events=key("LEFT", mods=1), native=True)
step(2)
fs = field_state("name")
expect(fs["cursorLength"] == 2, "shift+left still extends the selection", fs)

print("== 12. ctrl+A still reaches the widget ==")
s.ok("input", events=key("A", "a", mods=4), native=True)
step(2)
fs = field_state("name")
expect(fs["text"] == "abcdef", "ctrl+A did not type a character", repr(fs["text"]))
expect(fs["cursorLength"] == 6, "ctrl+A selected everything", fs["cursorLength"])

print("== 13. flat collect ==")
s.invoke("form.reset", settle=0.0)
step()
fill("name", "Ann")
fill("email", "ann@example.org")
fill("notes", "ignored")
focus("subscribe")
s.ok("input", events=key("SPACE", " "), native=True)
step(2)
collected = s.invoke("form.collect", settle=0.0)
expect(collected.get("name") == "Ann", "the name was collected", collected)
expect(collected.get("email") == "ann@example.org", "the email was collected", collected)
expect(collected.get("subscribe") is True, "the checkbox was collected", collected)
expect("notes" not in collected, "a transient field is not collected", collected)
expect("submit" not in collected and "reset" not in collected, "buttons are not collected",
       collected)

print("== 14. nested collect ==")
s.invoke("form.set-field-name", field="name", value="user.name", settle=0.0)
s.invoke("form.set-field-name", field="email", value="user.email", settle=0.0)
s.invoke("form.set-value-mode", mode="nested", settle=0.0)
step()
collected = s.invoke("form.collect", settle=0.0)
expect(collected.get("user", {}).get("name") == "Ann", "a dotted name became a nested key",
       collected)
expect(collected.get("user", {}).get("email") == "ann@example.org",
       "both segments landed in the same dictionary", collected)
expect(collected.get("subscribe") is True, "an undotted name stays at the top level", collected)

print("== 15. assign round-trip ==")
saved = collected
s.invoke("form.reset", settle=0.0)
step()
expect(s.invoke("form.collect", settle=0.0).get("user", {}).get("name") == "",
       "reset emptied the fields")
s.invoke("form.assign", value=saved, settle=0.0)
step(2)
expect(s.invoke("form.collect", settle=0.0) == saved, "assign restored exactly what was collected",
       s.invoke("form.collect", settle=0.0))

# Back to flat names for the rest, so the reports read as field names
s.invoke("form.set-value-mode", mode="flat", settle=0.0)
s.invoke("form.set-field-name", field="user.name", value="name", settle=0.0)
s.invoke("form.set-field-name", field="user.email", value="email", settle=0.0)
step()

print("== 16. a required field blocks the submit ==")
s.invoke("form.reset-counters", settle=0.0)
s.invoke("form.reset", settle=0.0)
step()
fill("name", "Ann")
s.invoke("form.submit", settle=0.0)
step(3)
st = state()
expect(st["submitCount"] == 0, "nothing was submitted", st["submitCount"])
expect(st["invalidCount"] == 1, "the invalid callback fired", st["invalidCount"])
expect(st["lastInvalid"] == ["email"], "the empty required field was reported", st["lastInvalid"])
expect(st["fields"]["email"]["invalid"], "and marked invalid")
expect(st["focused"] == "email", "focus moved to the first offender", st["focused"])

print("== 17. the invalid mark is a style class on the node ==")
expect(state()["fields"]["email"]["invalidClass"], "the node carries the `invalid` class")
outline = field_state("email")["outlineColor"]
expect(outline == {"r": 229, "g": 57, "b": 53, "a": 255},
       "and CSS repainted the outline through it", outline)

print("== 18. the per-field validator ==")
fill("email", "nope")
s.invoke("form.submit", settle=0.0)
step(3)
st = state()
expect(st["submitCount"] == 0, "a malformed value is still rejected", st["submitCount"])
expect(st["lastInvalid"] == ["email"], "by the field's own validator", st["lastInvalid"])

fill("email", "ann@example.org")
s.invoke("form.submit", settle=0.0)
step(3)
st = state()
expect(st["submitCount"] == 1, "a valid form submits", st["submitCount"])
expect(not st["fields"]["email"]["invalid"], "the mark was cleared")
expect(not st["fields"]["email"]["invalidClass"], "and so was the style class")

print("== 19. enter submits from a field ==")
s.invoke("form.reset-counters", settle=0.0)
focus("name")
s.ok("input", events=key("ENTER", "\r"), native=True)
step(3)
st = state()
expect(st["submitCount"] == 1, "enter in a field submitted the form", st["submitCount"])
for f in ("name", "email", "notes"):
    fs = field_state(f)
    expect("\n" not in fs["text"] and "\r" not in fs["text"], f"no newline landed in `{f}`",
           repr(fs["text"]))

print("== 20. enter on the buttons ==")
s.invoke("form.reset-counters", settle=0.0)
focus("submit")
s.ok("input", events=key("ENTER", "\r"), native=True)
step(3)
expect(state()["submitCount"] == 1, "enter on the submit button submitted",
       state()["submitCount"])

focus("reset")
s.ok("input", events=key("ENTER", "\r"), native=True)
step(3)
st = state()
expect(st["resetCount"] == 1, "enter on the reset button reset", st["resetCount"])
expect(st["fields"]["name"]["value"] == "", "and cleared the fields", st["fields"]["name"])

print("== 21. the submit payload is what collect() reports ==")
s.invoke("form.reset-counters", settle=0.0)
fill("name", "Bob")
fill("email", "bob@example.org")
collected = s.invoke("form.collect", settle=0.0)
s.invoke("form.submit", settle=0.0)
step(3)
st = state()
expect(st["submitCount"] == 1, "the form submitted", st["submitCount"])
expect(st["lastSubmit"] == collected, "the callback got exactly what collect() returns",
       st["lastSubmit"])

print("== 22. clipboard round-trip between two fields ==")
focus("name")
s.ok("input", events=key("A", "a", mods=4), native=True)
step()
s.ok("input", events=key("C", "c", mods=4), native=True)
step(2)
expect(field_state("name")["text"] == "Bob", "ctrl+C left the source alone",
       repr(field_state("name")["text"]))

focus("notes")
pasted = paste_until("notes", "Bob")
expect(pasted == "Bob", "ctrl+V pasted the copied text", repr(pasted))

print("== 23. cut empties the selection and puts it on the clipboard ==")
# A distinct string, so the paste at the end cannot pass on what section 22 left behind
fill("notes", "Cut me")
focus("notes")
s.ok("input", events=key("A", "a", mods=4), native=True)
step(3)
# The precondition, asserted rather than assumed: selectAll is a REQUEST, and cut refuses when the
# echo has not come back yet and the cursor is still a caret
expect(field_state("notes")["cursorLength"] == 6, "the selection is in place before the cut",
       field_state("notes"))

s.ok("input", events=key("X", "x", mods=4), native=True)
step(3)
expect(field_state("notes")["text"] == "", "ctrl+X removed the selection",
       repr(field_state("notes")["text"]))

focus("name")
pasted = paste_until("name", "Cut me")
expect(pasted == "Cut me", "what was cut went to the clipboard", repr(pasted))

print("== 24. a password field never reaches the clipboard ==")
fill("notes", "s3cret")
focus("notes")
s.ok("input", events=key("A", "a", mods=4), native=True)
step(3)
s.invoke("form.set-password", field="notes", password=True, settle=0.0)
step()
s.ok("input", events=key("C", "c", mods=4), native=True)
step(3)
focus("email")
s.ok("input", events=key("A", "a", mods=4), native=True)
step(3)
s.ok("input", events=key("V", "v", mods=4), native=True)
step(8)
# Not just "!= s3cret": the field must hold what the clipboard held BEFORE, which proves the paste
# really ran and simply found no password there. A paste that silently did nothing would pass a
# bare inequality
pasted = field_state("email")["text"]
expect(pasted == "Cut me", "the paste ran and the clipboard was untouched by the password",
       repr(pasted))

print("== a locked control says why, and leaves the ring ==")
REASON = "a wire supplies this value"
s.invoke("form.set-locked", field="notes", locked=True, reason=REASON, settle=0.0)
step(2)
st = state()
f = st["fields"]["notes"]
expect(f["locked"] is True, "the field reports itself locked")
expect(f["lockReason"] == REASON, "and carries the reason it was given", repr(f["lockReason"]))
expect(f["lockedClass"] is True, "the `locked` class is on the node for a stylesheet")
expect(f["disabledClass"] is True, "and `disabled` with it - a locked control IS disabled")
expect(f["interactive"]["enabled"] is False, "so :disabled matches it")
expect(f["tooltip"] == REASON, "the reason is readable as a hint", repr(f["tooltip"]))
expect("notes" not in st["tabRing"], "the locked field left the tab ring", st["tabRing"])

s.invoke("form.set-locked", field="notes", locked=False, settle=0.0)
step(2)
st = state()
f = st["fields"]["notes"]
expect(f["locked"] is False, "unlocking clears the lock")
expect(f["lockedClass"] is False, "and the class with it")
expect(f["interactive"]["enabled"] is True, "the control is live again")
expect(f["tooltip"] == "", "and the hint the lock installed is taken away", repr(f["tooltip"]))
expect(state()["tabRing"] == ALL_FIELDS, "the field is back in the ring", state()["tabRing"])

print("== the lock and the widget's own enablement are two sources, not one ==")
# Disabled by the application, THEN locked: clearing the lock must not switch on something the
# application had switched off for its own reasons.
s.invoke("form.set-widget-enabled", field="notes", enabled=False, settle=0.0)
step(2)
expect(state()["fields"]["notes"]["interactive"]["enabled"] is False, "the widget is off")
s.invoke("form.set-locked", field="notes", locked=True, reason=REASON, settle=0.0)
step(2)
s.invoke("form.set-locked", field="notes", locked=False, settle=0.0)
step(2)
f = state()["fields"]["notes"]
expect(f["interactive"]["enabled"] is False,
       "unlocking gives back what the APPLICATION asked for, not `on`")
s.invoke("form.set-widget-enabled", field="notes", enabled=True, settle=0.0)
step(2)
expect(state()["fields"]["notes"]["interactive"]["enabled"] is True, "and enabling it works")

print("== a checkbox is finally visible to CSS ==")
# The bug this closes: with no InteractiveComponent a node reads as state 0, and `:disabled` is
# "not :enabled" - so an untouched checkbox matched `checkbox:disabled` while being enabled.
c = UNTOUCHED_FIELDS["subscribe"]["interactive"]
expect(c["hasComponent"] is True,
       "a checkbox carries the component from the FIRST frame, before anything touches it")
expect(c["enabled"] is True,
       "so :enabled matches it and :disabled does not - which was backwards before")

# The same for every other control in the form: the invariant is "a control is visible to the
# style resolver from frame zero", not "a checkbox is".
for _f in ("name", "email", "notes", "submit", "reset"):
    expect(UNTOUCHED_FIELDS[_f]["interactive"]["hasComponent"] is True,
           f"{_f} carries the component from the first frame too",
           UNTOUCHED_FIELDS[_f]["interactive"])
s.invoke("form.set-checked", field="subscribe", checked=True, settle=0.0)
step(2)
expect(state()["fields"]["subscribe"]["interactive"]["checked"] is True,
       ":checked follows the checkbox")
s.invoke("form.set-checked", field="subscribe", checked=False, settle=0.0)
step(2)
expect(state()["fields"]["subscribe"]["interactive"]["checked"] is False, "and back again")
expect(state()["fields"]["subscribe"]["interactive"]["focusCounter"] in (0, 1),
       "and the checkbox did not become a second writer of the focus counter",
       state()["fields"]["subscribe"]["interactive"]["focusCounter"])

print()
print(f"SUMMARY: {CHECKS} checks, {len(FAIL)} failures")
for f in FAIL:
    print("  -", f)
APP.terminate()
APP.wait(timeout=10)
raise SystemExit(1 if FAIL else 0)
