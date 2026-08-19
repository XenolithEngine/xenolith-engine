#!/usr/bin/env python3
"""Drive the ui::InlineEditor stand (XL_INLINE_EDIT_TEST) over the inspector socket.

Everything here runs headless. The load-bearing facts are the ones a screenshot cannot show:

  * the editor is NOT a child of what it edits. A ui::TableView destroys a row node when it scrolls
    out and rebuilds every one of them on invalidateSource(), while a focused ui::TextInput holds
    the IME - so an editor parented into a cell loses the typed text to a rebuild the author never
    asked for. Rebuilding the whole table under an open editor must change nothing about it;
  * scrolling ends the edit by KEEPING what was typed, not by dropping it: the rectangle the editor
    was placed on now belongs to a different row, but what the author wrote is still theirs;
  * Escape restores the text the edit started from - no field here does that on its own, only
    ui::NumberField restores anything, and it restores a different thing;
  * a commit is a QUESTION: refused, the session stays open with the text intact;
  * and the commit arrives exactly ONCE even when Enter and the press that follows it land in the
    same interaction, which is the normal way of finishing an edit with the mouse.

    tests/window/inline-edit-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-inline-edit-check.sock")

# What the stand declares, duplicated here on purpose: a check that reads its expectations out of
# the thing it is checking cannot fail.
LABEL_TEXT = "Transform"
ROW_COUNT = 40
ROW_NAME = "field2"
EDITED_ROW = 2


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
                chunk = self.s.recv(65536)
                if not chunk:
                    raise SystemExit("the app closed the connection - it crashed")
                self.buf += chunk
            size = struct.unpack("<I", self.buf[:4])[0]
            while len(self.buf) < 4 + size:
                chunk = self.s.recv(65536)
                if not chunk:
                    raise SystemExit("the app closed the connection - it crashed")
                self.buf += chunk
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
    env["XL_INLINE_EDIT_TEST"] = "1"
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


def state():
    return s.invoke("inline-edit.state")


def values(st):
    return st.get("values") or []


try:
    s.ok("frame", count=3)

    st = state()
    check("nothing is being edited at rest",
            st["labelEditing"] is False and st["cellEditing"] is False)
    check("the label shows what it was seeded with", st["labelText"] == LABEL_TEXT, st["labelText"])
    check("the table carries its rows", st["rowCount"] == ROW_COUNT, st["rowCount"])
    check("and no overlay is up", s.invoke("inline-edit.overlays")["count"] == 0)

    # --- the plain case: open, type, commit -------------------------------------------------------
    check("an edit opens", s.invoke("inline-edit.begin", target="label")["ok"] is True)
    s.ok("frame", count=3)
    st = state()
    check("the editor holds what the target held", st.get("editorText") == LABEL_TEXT,
            st.get("editorText"))
    check("and it is on an overlay of its own",
            st.get("overlay") is True and s.invoke("inline-edit.overlays")["count"] == 1)

    s.invoke("inline-edit.type", value="Rotate")
    s.ok("frame", count=2)
    check("typing reaches it", state().get("editorText") == "Rotate")

    check("committing is accepted", s.invoke("inline-edit.commit")["ok"] is True)
    s.ok("frame", count=3)
    st = state()
    check("the value landed", st["labelText"] == "Rotate", st["labelText"])
    check("the session closed", st["labelEditing"] is False)
    check("exactly one commit and one close", st["commits"] == 1 and st["closes"] == 1,
            (st["commits"], st["closes"]))
    check("and the overlay is gone", s.invoke("inline-edit.overlays")["count"] == 0)

    # --- Escape restores ---------------------------------------------------------------------------
    s.invoke("inline-edit.reset-counters")
    s.invoke("inline-edit.begin", target="label")
    s.ok("frame", count=3)
    s.invoke("inline-edit.type", value="Scaled")
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("ESCAPE"))
    time.sleep(0.4)
    s.ok("frame", count=3)
    st = state()
    check("Escape ends the edit", st["labelEditing"] is False)
    check("and puts the original back", st["labelText"] == "Rotate", st["labelText"])
    check("it cancelled rather than committed", st["cancels"] == 1 and st["commits"] == 0,
            (st["cancels"], st["commits"]))

    # --- a refused commit keeps the session --------------------------------------------------------
    s.invoke("inline-edit.reset-counters")
    s.invoke("inline-edit.refuse", value=True)
    s.invoke("inline-edit.begin", target="label")
    s.ok("frame", count=3)
    s.invoke("inline-edit.type", value="Refused")
    s.ok("frame", count=2)
    check("a refused commit reports the refusal",
            s.invoke("inline-edit.commit")["ok"] is False)
    s.ok("frame", count=3)
    st = state()
    check("the editor is still open", st["labelEditing"] is True)
    check("with what was typed still in it", st.get("editorText") == "Refused",
            st.get("editorText"))
    check("nothing was committed and nothing closed",
            st["commits"] == 0 and st["closes"] == 0, (st["commits"], st["closes"]))
    check("the value did not move", st["labelText"] == "Rotate", st["labelText"])

    s.invoke("inline-edit.refuse", value=False)
    check("and accepting afterwards works", s.invoke("inline-edit.commit")["ok"] is True)
    s.ok("frame", count=3)
    check("with the refused text finally landing", state()["labelText"] == "Refused")

    # --- the commit arrives exactly once -----------------------------------------------------------
    #
    # Enter fires the field's accept callback while the priority-1 outside-tap listener inside
    # ui::TextInput independently calls blur() - both in one interaction, which is how an edit
    # finished with the mouse normally ends.
    s.invoke("inline-edit.reset-counters")
    s.invoke("inline-edit.begin", target="label")
    s.ok("frame", count=3)
    s.invoke("inline-edit.type", value="Once")
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("ENTER") + tap(700.0, 700.0))
    time.sleep(0.5)
    s.ok("frame", count=3)
    st = state()
    check("Enter followed by a press outside commits ONCE", st["commits"] == 1, st["commits"])
    check("and closes once", st["closes"] == 1, st["closes"])
    check("nothing was cancelled along the way", st["cancels"] == 0, st["cancels"])
    check("the value is the one that was typed", st["labelText"] == "Once", st["labelText"])

    # --- the neighbour sees nothing ----------------------------------------------------------------
    s.invoke("inline-edit.focus-neighbour")
    s.ok("frame", count=2)
    before = state()["neighbourCursor"]
    s.invoke("inline-edit.begin", target="label")
    s.ok("frame", count=3)
    s.ok("input", native=True, events=key("HOME") + key("END"))
    time.sleep(0.4)
    s.ok("frame", count=2)
    st = state()
    check("the field beside the editor never saw the keys",
            st["neighbourCursor"] == before, (before, st["neighbourCursor"]))
    check("while the editor was up the whole time", st["labelEditing"] is True)
    s.invoke("inline-edit.cancel")
    s.ok("frame", count=2)

    # --- the reason the widget exists: a rebuild underneath ----------------------------------------
    s.invoke("inline-edit.reset-counters")
    st = state()
    check("the row starts with its seeded name", values(st)[EDITED_ROW] == ROW_NAME,
            values(st)[EDITED_ROW])

    check("an editor opens over a cell",
            s.invoke("inline-edit.begin", target="cell", row=EDITED_ROW)["ok"] is True)
    s.ok("frame", count=3)
    st = state()
    check("over the row that was asked for", st["editedRow"] == EDITED_ROW, st["editedRow"])
    check("seeded from the row's value", st.get("editorText") == ROW_NAME, st.get("editorText"))

    s.invoke("inline-edit.type", value="renamed")
    s.ok("frame", count=2)

    # invalidateSource() forces a rebuild of EVERY row - including the one under the editor. An
    # editor parented into that row would be destroyed here, taking the IME and the text with it.
    check("rebuilding every row is accepted", s.invoke("inline-edit.rebuild")["ok"] is True)
    s.ok("frame", count=4)
    st = state()
    check("the edit survives a rebuild of every row", st["cellEditing"] is True)
    check("and the typed text is untouched", st.get("editorText") == "renamed",
            st.get("editorText"))
    check("the rebuild committed nothing and cancelled nothing",
            st["commits"] == 0 and st["cancels"] == 0, (st["commits"], st["cancels"]))

    # --- scrolling ends it, and KEEPS what was typed ------------------------------------------------
    check("scrolling the table is accepted", s.invoke("inline-edit.scroll", delta=60)["ok"] is True)
    s.ok("frame", count=4)
    st = state()
    check("scrolling ends the edit", st["cellEditing"] is False)
    check("by COMMITTING, not by dropping it", st["commits"] == 1 and st["cancels"] == 0,
            (st["commits"], st["cancels"]))
    check("so the typed value reached the row", values(st)[EDITED_ROW] == "renamed",
            values(st)[EDITED_ROW])
    check("and the overlay came down with it", s.invoke("inline-edit.overlays")["count"] == 0)

    # --- closeOnScroll is an option, not a decoration ------------------------------------------------
    s.invoke("inline-edit.reset-counters")
    s.invoke("inline-edit.close-on-scroll", value=False)
    s.invoke("inline-edit.begin", target="cell", row=5)
    s.ok("frame", count=3)
    s.invoke("inline-edit.type", value="stays")
    s.ok("frame", count=2)
    s.invoke("inline-edit.scroll", delta=60)
    s.ok("frame", count=4)
    st = state()
    check("with closeOnScroll off, scrolling leaves the edit open", st["cellEditing"] is True)
    check("and commits nothing", st["commits"] == 0, st["commits"])
    check("the typed text is still there", st.get("editorText") == "stays", st.get("editorText"))
    s.invoke("inline-edit.cancel")
    s.invoke("inline-edit.close-on-scroll", value=True)
    s.ok("frame", count=2)

    # Back to the top: the section below edits a row by index, and a row that has scrolled out has
    # no node for the stand to take a rectangle from - which is exactly what E4's getCellRect is for.
    s.invoke("inline-edit.scroll", delta=-int(round(state().get("scroll", 0) / 100.0)))
    s.ok("frame", count=4)

    # --- the anchor leaving the scene ----------------------------------------------------------------
    s.invoke("inline-edit.reset-counters")
    s.invoke("inline-edit.begin", target="cell", row=1)
    s.ok("frame", count=3)
    s.invoke("inline-edit.type", value="detached")
    s.ok("frame", count=2)
    check("an overlay is up before the anchor goes",
            s.invoke("inline-edit.overlays")["count"] == 1)

    s.invoke("inline-edit.detach-table")
    s.ok("frame", count=4)
    st = state()
    check("the anchor leaving the scene ends the edit", st["cellEditing"] is False)
    check("keeping what was typed", st["commits"] == 1 and st["lastCommit"] == "detached",
            (st["commits"], st["lastCommit"]))
    check("and leaves no overlay behind", s.invoke("inline-edit.overlays")["count"] == 0)
finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
