#!/usr/bin/env python3
"""Drive the ui::TextView undo stand (XL_TEXT_VIEW_TEST) over the inspector socket and assert.

Everything here runs headless: no display, no compositor, no clipboard manager. Typed characters go
in through `input` with native=true, which puts them in FRONT of the platform text-input processor -
the path that matters, because the processor owns printable keys and a typed character reaches the
widget only as an echo, never through insertText. Ctrl chords go the same way on purpose: the
processor declines every Ctrl chord that has no Alt, and that decline is what lets Ctrl+Z through.

    tests/window/text-undo-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-text-undo.sock")

# InputModifier bits, as the runtime spells them.
SHIFT = 1
CTRL = 4


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
        if ch == "\n":
            code = "ENTER"
        elif ch == " ":
            code = "SPACE"
        elif ch.isalpha():
            code = ch.upper()
        else:
            code = "UNKNOWN"
        events += key(code, ch)
    return events


def start_app(binary):
    """Launch a private headless instance so the checks start from a known state."""
    env = dict(os.environ)
    env["XL_TEXT_VIEW_TEST"] = "1"
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
    return s.invoke("text-view.state", settle=0.0)


def view():
    return state()["view"]


def field():
    return state()["field"]


def step(n=1):
    s.ok("frame", count=n)
    time.sleep(0.15)


def reset(text="", widget="view"):
    """A known starting point: set-text also drops the history, which is itself asserted below."""
    s.invoke("text-view.set-text", text=text, widget=widget, settle=0.0)
    s.invoke("text-view.focus", widget=widget, settle=0.0)
    s.invoke("text-view.reset-counters", settle=0.0)
    step(2)


try:
    step(3)

    # 1. Defaults. The view undoes; the field beside it, with the same machinery, does not.
    print("\n-- 1. defaults --")
    v = view()
    f = field()
    expect(v["undoEnabled"] is True, "the view has a history")
    expect(f["undoEnabled"] is False, "a plain field does NOT, by default")
    expect(v["canUndo"] is False, "nothing to undo yet")
    expect(v["canRedo"] is False, "nothing to redo yet")
    expect(v["undoName"] == "", "and nothing to name")

    # 2. Type, undo, redo. One run of keystrokes is ONE entry.
    print("\n-- 2. type, undo, redo --")
    reset()
    s.ok("input", events=typed("hello"), native=True)
    step(2)
    v = view()
    expect(v["text"] == "hello", "typing lands", v["text"])
    expect(v["canUndo"] is True, "and can be undone while still being typed")
    expect(v["undoName"] == "typing", "named for what it is", v["undoName"])
    expect(v["historyDepth"] == 0, "the run is not committed yet", v["historyDepth"])

    s.invoke("text-view.undo", settle=0.0)
    step(2)
    v = view()
    expect(v["text"] == "", "one undo takes back the WHOLE run", repr(v["text"]))
    expect(v["historyDepth"] == 1, "which committed it as one entry", v["historyDepth"])
    expect(v["historyPosition"] == 0, "and the cursor moved to its start")
    expect(v["canRedo"] is True, "redo is available")

    s.invoke("text-view.redo", settle=0.0)
    step(2)
    v = view()
    expect(v["text"] == "hello", "redo puts it back", v["text"])
    expect(v["cursorStart"] == 5, "with the caret after it", v["cursorStart"])
    expect(v["historyDepth"] == 1, "and adds no entry of its own", v["historyDepth"])

    # 3. Undo must not record itself. The re-entry this guards against is the one that turns a
    #    single undo into an endless one, so it is counted rather than eyeballed.
    print("\n-- 3. undo does not record itself --")
    before = view()["historyDepth"]
    for _ in range(3):
        s.invoke("text-view.undo", settle=0.0)
        s.invoke("text-view.redo", settle=0.0)
    step(2)
    v = view()
    expect(v["historyDepth"] == before, "three undo/redo pairs add no entries", v["historyDepth"])
    expect(v["text"] == "hello", "and leave the text where it was", v["text"])

    # 4. Undo restores the CARET, not only the characters.
    print("\n-- 4. the caret comes back too --")
    reset("abcdef")
    s.invoke("text-view.set-cursor", start=1, length=3, settle=0.0)
    step()
    s.ok("input", events=typed("X"), native=True)
    step(2)
    v = view()
    expect(v["text"] == "aXef", "typing replaces the selection", v["text"])

    s.invoke("text-view.undo", settle=0.0)
    step(2)
    v = view()
    expect(v["text"] == "abcdef", "undo puts the text back", v["text"])
    expect(v["cursorStart"] == 1 and v["cursorLength"] == 3,
           "and the SELECTION with it",
           f'{v["cursorStart"]}+{v["cursorLength"]}')

    # 5. Coalescing. A run stays one entry until something ends it.
    print("\n-- 5. runs --")
    reset()
    s.ok("input", events=typed("abc"), native=True)
    step(2)
    s.invoke("text-view.history-break", settle=0.0)
    s.ok("input", events=typed("def"), native=True)
    step(2)
    v = view()
    expect(v["text"] == "abcdef", "six characters, two runs", v["text"])

    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "abc", "the second run undoes alone", view()["text"])
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "", "and then the first", repr(view()["text"]))

    # 6. The idle window itself. The one claim a counter cannot make, so this one really waits -
    #    with a window shortened to 120 ms so the wait is a fifth of a second, not most of one.
    print("\n-- 6. the idle window --")
    reset()
    s.invoke("text-view.history-idle", value=120000, settle=0.0)
    s.ok("input", events=typed("gh"), native=True)
    step(2)
    time.sleep(0.4)
    step(2)
    s.ok("input", events=typed("ij"), native=True)
    step(2)
    expect(view()["text"] == "ghij", "four characters across the pause", view()["text"])
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "gh", "a pause ends the run", view()["text"])
    s.invoke("text-view.history-idle", value=700000, settle=0.0)

    # 7. A newline ends the thought.
    print("\n-- 7. newline --")
    reset()
    s.ok("input", events=typed("ab"), native=True)
    step(2)
    s.ok("input", events=key("ENTER", "\n"), native=True)
    step(2)
    s.ok("input", events=typed("cd"), native=True)
    step(2)
    expect(view()["text"] == "ab\ncd", "two lines", repr(view()["text"]))
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "ab\n", "the second line undoes alone", repr(view()["text"]))
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "ab", "the newline is its own entry", repr(view()["text"]))

    # 8. A paste is ONE entry, and it is named. This goes through the real asynchronous clipboard
    #    read, because a paste that arrived character by character would coalesce into typing.
    print("\n-- 8. paste --")
    reset("start ")
    s.invoke("text-view.set-cursor", start=6, length=0, settle=0.0)
    s.invoke("text-view.clipboard-write", text="pasted", settle=0.0)
    step(2)
    s.invoke("text-view.paste", settle=0.0)
    step(4)
    v = view()
    expect(v["text"] == "start pasted", "the paste lands", v["text"])
    expect(v["undoName"] == "paste", "named a paste, not typing", v["undoName"])
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "start ", "and undoes in ONE step", view()["text"])

    # 9. Cut is its own entry, and it is named too.
    print("\n-- 9. cut --")
    reset("keep cut")
    s.invoke("text-view.set-cursor", start=5, length=3, settle=0.0)
    step()
    s.invoke("text-view.cut", settle=0.0)
    step(2)
    v = view()
    expect(v["text"] == "keep ", "the cut removes the selection", repr(v["text"]))
    expect(v["undoName"] == "cut", "named a cut", v["undoName"])
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "keep cut", "and undoes whole", view()["text"])

    # 10. Ctrl+Z reaches the widget, through the platform's key path.
    print("\n-- 10. the chords --")
    reset()
    s.ok("input", events=typed("chord"), native=True)
    step(2)
    s.ok("input", events=key("Z", None, CTRL), native=True)
    step(2)
    expect(view()["text"] == "", "Ctrl+Z undoes", repr(view()["text"]))
    expect(state()["undoFellThrough"] == 0, "and is consumed by the focused view")

    s.ok("input", events=key("Y", None, CTRL), native=True)
    step(2)
    expect(view()["text"] == "chord", "Ctrl+Y redoes", view()["text"])

    s.ok("input", events=key("Z", None, CTRL), native=True)
    step(2)
    expect(view()["text"] == "", "Ctrl+Z again", repr(view()["text"]))
    s.ok("input", events=key("Z", None, CTRL | SHIFT), native=True)
    step(2)
    expect(view()["text"] == "chord", "Ctrl+Shift+Z redoes as well", view()["text"])
    expect(state()["redoFellThrough"] == 0, "and neither redo chord fell through")

    # 11. An unhandled chord is NOT swallowed. Without something below to catch it this claim
    #     could not be falsified, so the stand carries a catcher and the count is the assertion.
    print("\n-- 11. nothing to undo, nothing swallowed --")
    reset("", "field")
    expect(field()["undoEnabled"] is False, "the field still has no history")
    s.ok("input", events=key("Z", None, CTRL), native=True)
    step(2)
    expect(state()["undoFellThrough"] == 1,
           "Ctrl+Z in a field with no history reaches what is below",
           state()["undoFellThrough"])

    reset()
    s.ok("input", events=key("Z", None, CTRL), native=True)
    step(2)
    expect(state()["undoFellThrough"] == 1,
           "and so does Ctrl+Z in a view whose history is empty",
           state()["undoFellThrough"])

    # 12. The seam serves the field too - it is off by policy, not because it does not work. This
    #     is the IME-owned half: the field has no document, so its undo is a REQUEST.
    print("\n-- 12. the field, when asked --")
    reset("", "field")
    s.invoke("text-view.undo-enabled", value=True, widget="field", settle=0.0)
    step()
    s.ok("input", events=typed("typed"), native=True)
    step(2)
    f = field()
    expect(f["text"] == "typed", "the field takes text", f["text"])
    expect(f["canUndo"] is True, "and now has something to undo")
    s.invoke("text-view.undo", widget="field", settle=0.0)
    step(2)
    expect(field()["text"] == "", "which the field undoes", repr(field()["text"]))
    s.invoke("text-view.undo-enabled", value=False, widget="field", settle=0.0)
    step()
    expect(field()["canUndo"] is False, "turning it off drops the log")

    # 13. A different document is a different history.
    print("\n-- 13. loading forgets --")
    reset()
    s.ok("input", events=typed("first"), native=True)
    step(2)
    expect(view()["canUndo"] is True, "there is history to lose")
    s.invoke("text-view.set-text", text="second document", settle=0.0)
    step(2)
    v = view()
    expect(v["text"] == "second document", "the text is replaced", v["text"])
    expect(v["canUndo"] is False, "and the history did not come along")
    expect(v["historyDepth"] == 0, "nothing left in the log", v["historyDepth"])

    # 14. The window push survives an undo: the next keystroke has to land against the text as it
    #     is NOW. Without the re-push the platform would diff against a base that no longer exists,
    #     and the character would land in the wrong place or not at all.
    print("\n-- 14. the platform is told --")
    reset("edge")
    s.invoke("text-view.set-cursor", start=4, length=0, settle=0.0)
    s.ok("input", events=typed("XY"), native=True)
    step(2)
    expect(view()["text"] == "edgeXY", "typed at the end", view()["text"])
    s.invoke("text-view.undo", settle=0.0)
    step(2)
    expect(view()["text"] == "edge", "undone", view()["text"])
    s.ok("input", events=typed("Z"), native=True)
    step(2)
    expect(view()["text"] == "edgeZ", "and the NEXT keystroke lands correctly", view()["text"])

finally:
    print(f"\n{CHECKS} checks, {len(FAIL)} failures")
    for f in FAIL:
        print(f"  {f}")
    try:
        s.close()
    except Exception:
        pass
    APP.terminate()
    APP.wait(timeout=10)

sys.exit(1 if FAIL else 0)
