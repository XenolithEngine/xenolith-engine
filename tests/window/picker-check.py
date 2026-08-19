#!/usr/bin/env python3
"""Drive the ui::SearchPicker stand (XL_SEARCH_PICKER_TEST) over the inspector socket.

Everything here runs headless. The load-bearing facts are the ones a screenshot cannot show:

  * the highlighted characters are the ones the matcher named, counted in UTF-16 code units - the
    row led by two emoji is the only place where that differs from the code point indices, and it
    is the whole reason the conversion lives in the engine rather than at each call site;
  * the surface answers the arrows while the caret stays in the query line, which is the opposite
    of what ui::Select does and the reason this is a second widget;
  * the field standing beside the control must not see a single arrow while the popup is up;
  * a source in each of the three match modes answers the same widget, and typo tolerance changes
    what matches without changing what a match means.

    tests/window/picker-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-picker-check.sock")

# What the stand declares, duplicated here on purpose: a check that reads its expectations out of
# the thing it is checking cannot fail.
EMOJI_ROW = "🔴🔵BlendState"
TURKISH_ROW = "İnputListener"
PLAIN_ROW = "BlendStateCache"


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


def start_app(binary):
    env = dict(os.environ)
    env["XL_SEARCH_PICKER_TEST"] = "1"
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


def rows(state):
    # An empty array comes back as null, which is the same answer as "no rows".
    return state["content"].get("rows") or []


def titles(state):
    return [r["title"] for r in rows(state)]


def row_named(state, title):
    for r in rows(state):
        if r["title"] == title:
            return r
    return None


def ranges_of(row):
    return [(r["start"], r["length"]) for r in (row.get("ranges") or [])]


def popups(s):
    return [w["id"] for w in s.ok("windows")["windows"] if w["type"] == "Popup"]


def selected_row(s, window):
    # Which row the popup's own surface has selected. Read out of the scene rather than out of a
    # command, because the popup's content is a second instance of the widget and the stand's
    # commands address the embedded one.
    tree = s.ok("scene", window=window)["text"]
    lines = [l for l in tree.split("\n") if "table-row" in l]
    for i, l in enumerate(lines):
        if ".selected" in l:
            return i
    return -1


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()
try:
    s.ok("frame", count=3)

    # --- the list, before anything is typed -----------------------------------------------------
    st = s.invoke("search-picker.state")
    check("an empty query is not a filter", len(rows(st)) > 0, len(rows(st)))
    check("and the first row starts selected", st["content"]["selected"] == 0,
            st["content"]["selected"])
    check("the stand is in subsequence mode", st["mode"] == "subsequence", st["mode"])

    # --- typing narrows, and the order is the score ----------------------------------------------
    s.invoke("search-picker.query", value="bl")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    narrowed = titles(st)
    check("a query narrows the list", 0 < len(narrowed) < 64, len(narrowed))
    check("every row still matches", all("b" in t.lower() and "l" in t.lower() for t in narrowed))

    scores = [r["score"] for r in rows(st)]
    check("the order is descending by score", scores == sorted(scores, reverse=True), scores[:6])

    check("a contiguous prefix outranks a scattered match",
            titles(st)[0] == PLAIN_ROW, titles(st)[0])

    # --- the highlight, in the units a label counts in --------------------------------------------
    plain = row_named(st, PLAIN_ROW)
    check("a contiguous match is one range", ranges_of(plain) == [(0, 2)], ranges_of(plain))

    emoji = row_named(st, EMOJI_ROW)
    check("the row led by two emoji matched", emoji is not None)
    if emoji:
        # Two code points before the B, but FOUR UTF-16 code units: this is the number that a
        # highlight computed in code points would get wrong, and the only check that says so.
        check("its highlight is in UTF-16 units, not code points",
                ranges_of(emoji) == [(4, 2)], ranges_of(emoji))

    s.invoke("search-picker.query", value="bsc")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    plain = row_named(st, PLAIN_ROW)
    check("a gappy match is reported as separate ranges",
            plain is not None and len(ranges_of(plain)) == 3, ranges_of(plain) if plain else None)

    s.invoke("search-picker.query", value="zzqx")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    check("a query nothing matches empties the list", rows(st) == [], titles(st))
    check("and nothing is selected", st["content"]["selected"] == -1, st["content"]["selected"])

    # --- the keyboard, with the caret in the query line -------------------------------------------
    s.invoke("search-picker.query", value="bl")
    s.invoke("search-picker.focus-query")
    s.ok("frame", count=2)

    before = s.invoke("search-picker.state")["neighbourCursor"]

    s.ok("input", native=True, events=key("DOWN"))
    time.sleep(0.3)
    st = s.invoke("search-picker.state")
    check("Down moves the selection", st["content"]["selected"] == 1, st["content"]["selected"])

    s.ok("input", native=True, events=key("DOWN") + key("DOWN"))
    time.sleep(0.3)
    st = s.invoke("search-picker.state")
    check("and keeps moving", st["content"]["selected"] == 3, st["content"]["selected"])

    s.ok("input", native=True, events=key("UP"))
    time.sleep(0.3)
    st = s.invoke("search-picker.state")
    check("Up moves it back", st["content"]["selected"] == 2, st["content"]["selected"])

    check("the field beside the surface never saw the arrows",
            st["neighbourCursor"] == before, (before, st["neighbourCursor"]))

    expected = titles(st)[2]
    s.ok("input", native=True, events=key("ENTER"))
    time.sleep(0.4)
    st = s.invoke("search-picker.state")
    check("Enter reports the selected row", st["activations"] == 1 and
            st["lastActivation"] == expected, (st["activations"], st["lastActivation"]))

    s.invoke("search-picker.select", index=0)
    st = s.invoke("search-picker.state")
    check("a selection can be set outright", st["content"]["selected"] == 0)
    check("and cannot be set past the end",
            s.invoke("search-picker.select", index=999)["ok"] is False)

    # --- the three match modes --------------------------------------------------------------------
    s.invoke("search-picker.mode", value="prefix")
    s.invoke("search-picker.query", value="blend")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    check("prefix mode matches whole words from their start",
            titles(st) == [PLAIN_ROW], titles(st))
    plain = row_named(st, PLAIN_ROW)
    check("and highlights the matched prefix of the word",
            ranges_of(plain) == [(0, 5)], ranges_of(plain))

    s.invoke("search-picker.query", value="lend")
    s.ok("frame", count=2)
    check("a word's middle is not its start", titles(s.invoke("search-picker.state")) == [],
            titles(s.invoke("search-picker.state")))

    s.invoke("search-picker.mode", value="text")
    s.invoke("search-picker.query", value="attachment")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    check("text mode finds the word in the body, not just in the name",
            len(titles(st)) >= 8 and all("Attachment" in t for t in titles(st)),
            titles(st)[:4])

    s.invoke("search-picker.query", value="the")
    s.ok("frame", count=2)
    check("and a stop word finds nothing", titles(s.invoke("search-picker.state")) == [])

    # --- typo tolerance ----------------------------------------------------------------------------
    #
    # The misspelling is in the STEM, not in the ending. Expansion happens over the words the index
    # actually holds, and under a stemming language those are stems: "attachment" is stored as
    # "attach", so a typo in the "-ment" the stemmer removed has nothing to be near. That is a real
    # limit of expanding a query rather than matching it approximately, and it is asserted here
    # rather than left for someone to discover.
    s.invoke("search-picker.query", value="attech")
    s.ok("frame", count=2)
    check("a misspelling finds nothing while tolerance is off",
            titles(s.invoke("search-picker.state")) == [],
            titles(s.invoke("search-picker.state"))[:4])

    s.invoke("search-picker.typo", value=True)
    s.invoke("search-picker.query", value="attech")
    s.ok("frame", count=2)
    st = s.invoke("search-picker.state")
    check("with tolerance on, the same misspelling finds the word",
            len(titles(st)) >= 8 and all("Attachment" in t for t in titles(st)), titles(st)[:4])

    s.invoke("search-picker.query", value="attachmnet")
    s.ok("frame", count=2)
    check("but a typo in the ending the stemmer removed is not recoverable",
            titles(s.invoke("search-picker.state")) == [])

    s.invoke("search-picker.query", value="qxz")
    s.ok("frame", count=2)
    check("tolerance is not a licence: three characters earn no edits",
            titles(s.invoke("search-picker.state")) == [])

    s.invoke("search-picker.typo", value=False)
    s.invoke("search-picker.mode", value="subsequence")
    s.invoke("search-picker.query", value="bl")
    s.ok("frame", count=2)

    # --- the popup ----------------------------------------------------------------------------------
    check("nothing is open yet", popups(s) == [])

    s.invoke("search-picker.open")
    time.sleep(1.0)
    open_popups = popups(s)
    check("the control opens a surface of its own", len(open_popups) == 1, open_popups)
    st = s.invoke("search-picker.state")
    check("and says so", st["picker"]["open"] is True)
    check("the surface is a window with a name", st["picker"]["popupId"].startswith("search-picker"),
            st["picker"]["popupId"])

    if open_popups:
        window = open_popups[0]
        s.ok("frame", count=4, window=window)
        check("its list starts on the first row", selected_row(s, window) == 0,
                selected_row(s, window))

        before = s.invoke("search-picker.state")["neighbourCursor"]
        s.ok("input", window=window, native=True, events=key("DOWN") + key("DOWN"))
        time.sleep(0.4)
        s.ok("frame", count=3, window=window)
        check("the arrows walk the surface's own list", selected_row(s, window) == 2,
                selected_row(s, window))
        check("while the field in the parent window sees nothing",
                s.invoke("search-picker.state")["neighbourCursor"] == before)

        s.ok("input", window=window, native=True, events=key("ESCAPE"))
        time.sleep(1.0)
        st = s.invoke("search-picker.state")
        check("Escape closes the surface", popups(s) == [] and st["picker"]["open"] is False)
        check("and chooses nothing", st["picker"]["value"] == "", st["picker"]["value"])

    # --- Enter in the popup chooses -------------------------------------------------------------------
    s.invoke("search-picker.open")
    time.sleep(1.0)
    open_popups = popups(s)
    if open_popups:
        window = open_popups[0]
        s.ok("frame", count=4, window=window)
        s.ok("input", window=window, native=True, events=key("DOWN"))
        time.sleep(0.4)
        s.ok("input", window=window, native=True, events=key("ENTER"))
        time.sleep(1.0)
        st = s.invoke("search-picker.state")
        check("Enter chooses and closes", popups(s) == [] and st["picker"]["open"] is False)
        check("the control now carries a value", st["picker"]["value"] != "",
                st["picker"]["value"])
        check("and shows it", st["picker"]["title"] == st["picker"]["value"],
                (st["picker"]["title"], st["picker"]["value"]))
finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
