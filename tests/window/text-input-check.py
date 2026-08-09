#!/usr/bin/env python3
"""Drive the ui::TextInput stand (XL_TEXT_INPUT_TEST) over the inspector socket and assert.

Everything here runs headless: no display, no compositor, no mouse. Typed characters go in through
`input` with native=true, which is what puts them in front of the platform text-input processor;
composition goes in through `text`, which no keystroke can express; and the layout's own
`text-input.state` command reports back what the widget ended up with.

    tests/window/text-input-check.py [path-to-testapp]

With no argument it builds nothing and expects the debug x86_64-linux binary in place. It starts
its own app instance, runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-text-input.sock")


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


def typed(text):
    events = []
    for ch in text:
        code = ch.upper() if ch.isalpha() else "SPACE" if ch == " " else "UNKNOWN"
        events += key(code, ch)
    return events

def start_app(binary):
    """Launch a private headless instance so the checks start from a known state."""
    env = dict(os.environ)
    env["XL_TEXT_INPUT_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    return subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


DEFAULT_BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")


#!/usr/bin/env python3
"""End-to-end check of ui::TextInput over the inspector socket."""

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


def state(widget="plain"):
    return s.invoke("text-input.state", widget=widget, settle=0.0)


def step(n=1):
    s.ok("frame", count=n)
    time.sleep(0.15)


print("== 1. initial ==")
st = state()
expect(st["text"] == "", "empty on start")
expect(st["placeholderVisible"], "placeholder shown")
expect(not st["focused"], "not focused")
expect(st["backgroundColor"] == {"r": 41, "g": 41, "b": 41, "a": 255}, "css background applied")

print("== 2. focus (the platform must grant it) ==")
s.invoke("text-input.focus", widget="plain", settle=0.0)
step()
st = state()
expect(st["focused"], "focused after acquire", st)
expect(st["interactive"]["focus"], "InteractiveComponent :focus set")
expect(st["outlineColor"] == {"r": 252, "g": 180, "b": 0, "a": 255},
       "text-input:focus outline applied", st["outlineColor"])
expect(st["caretVisible"], "caret visible")
expect(not st["placeholderVisible"], "placeholder hidden while focused")

print("== 3. typing through the real keyboard path ==")
s.ok("input", events=typed("Hello"), native=True)
step()
st = state()
expect(st["text"] == "Hello", "typed text arrived", st["text"])
expect(st["cursorStart"] == 5, "cursor at end", st["cursorStart"])
expect(st["changeCallbacks"] == 5, "change callback per character", st["changeCallbacks"])

print("== 4. backspace ==")
s.ok("input", events=key("BACKSPACE"), native=True)
step()
st = state()
expect(st["text"] == "Hell", "backspace deleted one char", st["text"])

print("== 5. shift+left selection ==")
s.ok("input", events=key("LEFT", mods=1) + key("LEFT", mods=1), native=True)
step()
st = state()
expect(st["cursorLength"] == 2, "two characters selected", st)
expect(st["cursorStart"] == 2, "selection anchored at the end", st)
expect(not st["caretVisible"], "caret hidden while a range is selected")

print("== 6. home / end ==")
s.ok("input", events=key("HOME"), native=True)
step()
expect(state()["cursorStart"] == 0, "HOME moves to start")
s.ok("input", events=key("END"), native=True)
step()
expect(state()["cursorStart"] == 4, "END moves to end")

print("== 7. ctrl+A ==")
s.ok("input", events=key("A", mods=4), native=True)
step()
st = state()
expect(st["cursorLength"] == 4, "ctrl+A selects everything", st)

print("== 8. composition (marked text) ==")
before = state()["changeCallbacks"]
s.invoke("text-input.set-cursor", widget="plain", start=4, length=0, settle=0.0)
step()
s.ok("text", op="marked", text="にほ", markedStart=4, markedLength=2)
step()
st = state()
expect(st["markedLength"] == 2, "marked range reported", st)
expect(st["changeCallbacks"] == before, "no change callback while composing",
       f"{st['changeCallbacks']} vs {before}")
s.ok("text", op="unmark")
step()
st = state()
expect(st["markedLength"] == 0, "unmark clears the marked range", st)

print("== 9. enter and tab are stripped ==")
s.invoke("text-input.set-text", widget="plain", text="abc", settle=0.0)
step()
s.invoke("text-input.focus", widget="plain", settle=0.0)
step()
enter_before = state()["enterCallbacks"]
s.ok("input", events=key("ENTER", "\r"), native=True)
step()
st = state()
expect("\n" not in st["text"] and "\r" not in st["text"], "no newline in the text", repr(st["text"]))
expect(st["enterCallbacks"] == enter_before + 1, "enter callback fired", st["enterCallbacks"])

print("== 10. max chars ==")
s.invoke("text-input.set-max-chars", widget="plain", value=3, settle=0.0)
s.invoke("text-input.set-text", widget="plain", text="0123456789", settle=0.0)
step(2)
st = state()
expect(st["text"] == "012", "text truncated to maxChars", st["text"])
s.invoke("text-input.set-max-chars", widget="plain", value=0, settle=0.0)

print("== 11. password masking ==")
s.invoke("text-input.set-text", widget="password", text="secret", settle=0.0)
step()
st = state("password")
expect(st["text"] == "secret", "password keeps the real text", st["text"])
expect(st["displayText"] == "\u2022" * 6, "password rendered as bullets", repr(st["displayText"]))
expect(not st["placeholderVisible"], "placeholder hidden once the field has text")

print("== 12. read-only ==")
s.invoke("text-input.focus", widget="readonly", settle=0.0)
step()
st = state("readonly")
expect(not st["focused"], "read-only field never focuses")
expect(not st["caretVisible"], "read-only field shows no caret")
s.invoke("text-input.select-all", widget="readonly", settle=0.0)
step()
expect(state("readonly")["cursorLength"] > 0, "read-only text can still be selected")

print("== 13. horizontal overflow ==")
st = state("long")
expect(st["overflow"], "long field overflows")
s.invoke("text-input.focus", widget="long", settle=0.0)
step()
s.invoke("text-input.set-cursor", widget="long", start=86, length=0, settle=0.0)
step(3)
st = state("long")
expect(st["labelOffsetX"] < 0.0, "label slid to follow the caret", st["labelOffsetX"])

print("== 14. escape releases input ==")
s.invoke("text-input.focus", widget="plain", settle=0.0)
step()
expect(state()["focused"], "focused before escape")
s.ok("input", events=key("ESCAPE"), native=True)
step()
st = state()
expect(not st["focused"], "escape released input")
expect(not st["interactive"]["focus"], "InteractiveComponent :focus cleared")

print("== 15. mouse ==")
# `plain` is anchored top-left at (48,652), so its box is y 613..652, x 48..398; the text starts
# at x=60 (12px padding). A tap needs several rendered frames to leave the multi-tap window.
def click(x, y, count=1):
    ev = []
    for i in range(count):
        ev.append({"event": "Begin", "id": 1, "button": "MouseLeft", "x": x, "y": y})
        ev.append({"event": "End", "id": 1, "button": "MouseLeft", "x": x, "y": y})
    return ev


def settle(t=1.0):
    for _ in range(int(t / 0.1)):
        s.ok("frame", count=1)
        time.sleep(0.1)


s.invoke("text-input.blur", widget="plain", settle=0.0)
s.invoke("text-input.set-text", widget="plain", text="Hello world", settle=0.0)
settle(0.5)

s.ok("input", events=click(64, 632))
settle(1.0)
st = state()
expect(st["focused"], "click focuses the field", st["focused"])
expect(st["cursorStart"] <= 1, "caret placed near the click", st["cursorStart"])

s.ok("input", events=click(100, 632, count=2))
settle(1.0)
st = state()
expect(st["cursorLength"] > 1, "double click selects a word", st["cursorLength"])

s.ok("input", events=click(300, 632))
settle(1.0)
st = state()
expect(st["cursorLength"] == 0, "single click collapses the selection", st["cursorLength"])
expect(st["cursorStart"] == 11, "click past the text goes to the end", st["cursorStart"])

s.ok("input", events=click(700, 400))
settle(1.0)
expect(not state()["focused"], "tap outside blurs the field")

print("== 16. long press ==")
# Holding still is what the touch idiom offers instead of a double click: one period selects the
# word under the finger, the next one the whole text. The recognizer counts its periods in
# update(), so the frames below are what makes the time pass at all.
s.invoke("text-input.blur", widget="plain", settle=0.0)
s.invoke("text-input.set-text", widget="plain", text="Hello world", settle=0.0)
settle(0.5)

s.ok("input", events=[{"event": "Begin", "id": 1, "button": "MouseLeft", "x": 100, "y": 632}])
settle(0.7)
st = state()
expect(st["cursorStart"] == 0 and st["cursorLength"] == 5, "long press selects the word", st)
expect(st["focused"], "long press takes input")

settle(0.7)
st = state()
expect(st["cursorLength"] == 11, "holding on selects everything", st["cursorLength"])

s.ok("input", events=[{"event": "End", "id": 1, "button": "MouseLeft", "x": 100, "y": 632}])
settle(1.0)
expect(state()["cursorLength"] == 11, "the release does not drop the selection",
        state()["cursorLength"])

s.ok("input", events=click(300, 632))
settle(1.0)
expect(state()["cursorLength"] == 0, "a plain tap after it still collapses the selection")

print("== 17. the viewport follows the end of the selection the user is MOVING ==")
# The `long` field is `text-input.wide`: 520px with 12px of horizontal padding, so 496px of visible
# text. `caretX` is the caret in label space and `labelOffsetX` is how far the label is slid, so
# their sum is where the moving end of the selection sits inside that box - and it has to stay in
# it, no matter which end of the range is growing.
LONG_TEXT_LEN = 87
VIEWPORT = 520.0 - 24.0


def in_view(st):
    return st["caretX"] + st["labelOffsetX"]


s.invoke("text-input.focus", widget="long", settle=0.0)
settle(0.5)
s.ok("input", events=key("HOME"), native=True)
settle(0.8)
expect(abs(state("long")["labelOffsetX"]) < 1.0, "caret at 0 needs no offset",
       state("long")["labelOffsetX"])

s.ok("input", events=key("RIGHT", mods=1) * 60, native=True)
settle(1.2)
st = state("long")
expect(st["cursorStart"] == 0 and st["cursorLength"] == 60, "60 characters selected rightwards",
       (st["cursorStart"], st["cursorLength"]))
expect(st["labelOffsetX"] < -50.0, "text slid left to follow the growing right edge",
       st["labelOffsetX"])
expect(0.0 <= in_view(st) <= VIEWPORT, "the moving right edge stays visible", in_view(st))

s.ok("input", events=key("RIGHT", mods=1) * (LONG_TEXT_LEN - 60), native=True)
settle(1.2)
st = state("long")
expect(st["cursorLength"] == LONG_TEXT_LEN, "selection reaches the end", st["cursorLength"])
expect(0.0 <= in_view(st) <= VIEWPORT, "the end of the text stays visible", in_view(st))
end_offset = st["labelOffsetX"]

# The other direction: END clears the anchor, so the following Shift+Left anchors at the end and
# moves the LEFT edge - and now that is what the viewport must show.
s.ok("input", events=key("END"), native=True)
settle(0.8)
s.ok("input", events=key("LEFT", mods=1) * 60, native=True)
settle(1.2)
st = state("long")
expect(st["cursorStart"] == LONG_TEXT_LEN - 60 and st["cursorLength"] == 60,
       "60 characters selected leftwards", (st["cursorStart"], st["cursorLength"]))
expect(st["labelOffsetX"] > end_offset + 50.0, "text slid back right to follow the left edge",
       (st["labelOffsetX"], end_offset))
expect(0.0 <= in_view(st) <= VIEWPORT, "the moving left edge stays visible", in_view(st))

# Shrinking a rightwards selection is the same rule read backwards: Shift+Left continues from the
# moving right edge instead of jumping to (or collapsing at) the anchor.
s.ok("input", events=key("HOME"), native=True)
settle(0.8)
s.ok("input", events=key("RIGHT", mods=1) * LONG_TEXT_LEN, native=True)
settle(1.2)
far_offset = state("long")["labelOffsetX"]
s.ok("input", events=key("LEFT", mods=1) * 60, native=True)
settle(1.2)
st = state("long")
expect(st["cursorStart"] == 0 and st["cursorLength"] == LONG_TEXT_LEN - 60,
       "Shift+Left shrinks the selection instead of collapsing it",
       (st["cursorStart"], st["cursorLength"]))
expect(st["labelOffsetX"] > far_offset + 50.0, "text slid back with the shrinking right edge",
       (st["labelOffsetX"], far_offset))
expect(0.0 <= in_view(st) <= VIEWPORT, "the shrinking right edge stays visible", in_view(st))

print("== 18. drag-selecting an overflowing field ==")
# `long` is anchored top-left at (48,436), so its box is y 397..436, x 48..568.
def pointer(event, x, y):
    return [{"event": event, "id": 1, "button": "MouseLeft", "x": x, "y": y}]


s.ok("input", events=key("HOME"), native=True)
settle(0.8)
s.ok("input", events=pointer("Begin", 60, 416))
settle(0.3)
for x in (120, 220, 320, 420):
    s.ok("input", events=pointer("Move", x, 416))
    settle(0.3)
st = state("long")
expect(st["cursorLength"] > 10, "the drag selects a range", st["cursorLength"])
expect(st["cursorStart"] <= 1, "anchored where the drag began", st["cursorStart"])
expect(0.0 <= in_view(st) <= VIEWPORT, "the dragged right edge stays visible", in_view(st))

# Parked past the right edge there are no more gesture events: the timed auto-scroll in
# TextInputContainer::update() is the only thing that can keep the text moving.
s.ok("input", events=pointer("Move", 640, 416))
parked_offset = state("long")["labelOffsetX"]
settle(1.0)
st = state("long")
expect(st["labelOffsetX"] < parked_offset - 10.0, "a pointer parked outside keeps pulling the text",
       (st["labelOffsetX"], parked_offset))
s.ok("input", events=pointer("End", 640, 416))
settle(0.5)
released_offset = state("long")["labelOffsetX"]
settle(1.0)
expect(state("long")["labelOffsetX"] == released_offset, "the release stops the auto-scroll",
       (state("long")["labelOffsetX"], released_offset))

print("== 19. a single tap is not delayed by the multi-tap interval ==")
# The field's tap recognizer counts up to 3 (caret / word / everything). Without
# InputTapFlags::Immediate it can only report tap 1 once TapIntervalAllowed (300ms) has proven that
# no second tap follows, and that wait is visible on every single click. With it the caret moves on
# the release, so the measured latency is a couple of frames.
s.invoke("text-input.blur", widget="plain", settle=0.0)
s.invoke("text-input.set-text", widget="plain", text="Hello world", settle=0.0)
settle(0.5)

s.ok("input", events=click(100, 632))
started = time.monotonic()
elapsed = None
while time.monotonic() - started < 2.0:
    s.ok("frame", count=1)
    st = state()
    if st["focused"] and st["cursorStart"] != 11:
        elapsed = time.monotonic() - started
        break
    time.sleep(0.01)

expect(elapsed is not None, "the tap was reported at all")
expect(elapsed is not None and elapsed < 0.2, "the caret moved before the 300ms tap interval",
       f"{elapsed * 1000.0:.0f}ms" if elapsed else "never")
settle(1.0)

# A real keyboard sends a keychar with a chord: Wayland reports the untransformed 'c', xcb the
# control code 0x03. Either one used to be claimed by TextInputProcessor and typed into the field,
# which is why the Ctrl+A binding above only ever passed with the synthetic keychar-less event.
print("== 20. ctrl chords carry a keychar and are still not text ==")
s.invoke("text-input.set-text", widget="plain", text="abcd", settle=0.0)
step()
s.invoke("text-input.focus", widget="plain", settle=0.0)
step()

s.ok("input", events=key("C", "c", mods=4), native=True)
step()
st = state()
expect(st["text"] == "abcd", "ctrl+C did not type a character", repr(st["text"]))

s.ok("input", events=key("A", "a", mods=4), native=True)
step()
st = state()
expect(st["text"] == "abcd", "ctrl+A did not type a character", repr(st["text"]))
expect(st["cursorLength"] == 4, "ctrl+A with a keychar still selects everything", st)

# Same for Tab: it has to stay a key event, or Shift+Tab cannot be told from Tab
s.invoke("text-input.set-text", widget="plain", text="abcd", settle=0.0)
step()
s.invoke("text-input.focus", widget="plain", settle=0.0)
step()
s.ok("input", events=key("TAB", "\t"), native=True)
step()
st = state()
expect("\t" not in st["text"], "tab did not type a character", repr(st["text"]))
expect(st["text"] == "abcd", "tab left the text alone", repr(st["text"]))
expect(not st["focused"], "tab blurs a field with no navigate callback", st["focused"])
settle(0.5)

print()
print(f"SUMMARY: {CHECKS} checks, {len(FAIL)} failures")
for f in FAIL:
    print("  -", f)
APP.terminate()
APP.wait(timeout=10)
raise SystemExit(1 if FAIL else 0)
