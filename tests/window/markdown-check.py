#!/usr/bin/env python3
"""Drive the ui::MarkdownView stand (XL_MARKDOWN_TEST) over the inspector socket.

What this script is for is the half of the milestone a C++ assertion cannot reach: the document
is a TREE that a layout produced, and the questions worth asking about it are questions about
that tree - which node each markdown construct became, what text ended up in it, and whether the
boxes the layout gave them are the boxes the text needs.

The stand answers all of it through one command, `markdown.dump`, so the checks below are reads
of a Value rather than screen-scraping. Each one stands for a decision the design rests on:

  * a block became a node NAMED AFTER ITS TAG - that is what makes the CSS in the widget's own
    sheet reach it, and what an application overrides;
  * an inline construct became a STYLE RANGE inside its block's label rather than a node beside
    it - counted as `ranges`, because a label with the right text and no ranges is a paragraph
    that lost its bold;
  * every block that carries text has a box taller than zero and no wider than the document -
    the two ways a wrapped label goes wrong;
  * every source run lies inside the source and inside its label's string, which is the map a
    selection will copy markup through;
  * the checkbox of a task item is checked when, and only when, the source says `[x]`.

    tests/window/markdown-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It prints "N checks, M
failures"; the exit status is the result.

HEADLESS DRAWS ONLY WHEN ASKED, and a stand command settles by running an action - which ticks on
frames. So a pump runs alongside every call here; without it a command that waits for its own
settle never answers at all.
"""
import json, os, socket, struct, subprocess, sys, threading, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-markdown-check.sock")


class Session:
    def __init__(self, path=ADDR, timeout=60.0):
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
        self.lock = threading.Lock()

    def call(self, cmd, **kw):
        with self.lock:
            self.serial += 1
            want = self.serial
            req = {"serial": want, "cmd": cmd}
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
                if resp.get("serial") == want:
                    return resp

    def ok(self, cmd, **kw):
        r = self.call(cmd, **kw)
        if r.get("status") != "ok":
            raise SystemExit(f"{cmd} failed: {r.get('error')}")
        return r.get("result")

    def close(self):
        self.s.close()


def start_app(binary):
    env = dict(os.environ)
    env["XL_MARKDOWN_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1100", "--height", "900"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(ADDR):
            try:
                Session().close()
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


def walk(node):
    yield node
    for child in node.get("children") or []:
        yield from walk(child)


def by_type(tree, name):
    return [n for n in walk(tree) if n.get("type") == name]


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()
s.ok("render")

stop = threading.Event()


def pump():
    while not stop.is_set():
        try:
            s.call("frame", count=2)
        except Exception:
            return
        time.sleep(0.02)


threading.Thread(target=pump, daemon=True).start()


def dump():
    return s.ok("invoke", name="markdown.dump", args={"settle": 0.4})


d = dump()
tree = d["tree"]

# --- the tree is the document -------------------------------------------------------------

check("the document produced blocks", d["blocks"] > 10, d["blocks"])
check("the source is kept with the view", d["sourceLength"] > 200, d["sourceLength"])

for tag, count in (("h1", 1), ("h2", 3), ("ul", 3), ("ol", 1), ("blockquote", 1), ("pre", 1),
        ("table", 1), ("hr", 1), ("dl", 1)):
    got = len(by_type(tree, tag))
    check(f"`{tag}` became {count} node(s)", got == count, f"got {got}")

check("list items became rows", len(by_type(tree, "li")) >= 7, len(by_type(tree, "li")))
check("table cells became labels", len(by_type(tree, "td")) == 4, len(by_type(tree, "td")))

# --- document order, which the layout reads as z-order ------------------------------------

top = [c.get("type") for c in tree.get("children") or []]
check("the blocks are in the order they were written",
        top.index("pre") < top.index("table") < top.index("hr"), top)

# --- inline constructs are style ranges, not nodes -----------------------------------------

para = next((n for n in by_type(tree, "p") if n.get("text", "").startswith("A paragraph")), None)
check("the sample paragraph is one label", para is not None)
if para:
    # bold, emphasis, code, strike and the link: five deltas over one string
    check("its inline constructs are style ranges", para.get("ranges", 0) >= 5,
            para.get("ranges"))
    # A label carries two sprites of its own (the selection and the marked highlight); they have
    # no type, and a markdown node always has one.
    inner = [n.get("type") for n in walk(para) if n is not para and n.get("type")]
    check("no inline construct became a node of its own", not inner, inner)
    check("the link's target was recorded",
            para.get("links") == ["https://xenolith.studio"], para.get("links"))
    # The text is kept VERBATIM, newline included: collapsing whitespace is the renderer's job
    # (`white-space: normal`), and a string that had already lost the newline could not map back
    # to the bytes it came from. Which is the thing to check instead - that it still maps.
    src = d["source"]
    text = para.get("text") or ""
    verbatim = [r for r in (para.get("runs") or []) if r[4]]
    check("the paragraph kept a run that is verbatim", verbatim, para.get("runs"))
    mismatched = [r for r in verbatim
            if text[r[0]:r[0] + r[1]] != src[r[2]:r[2] + r[3]]]
    check("every verbatim run is the source bytes it names", not mismatched, mismatched[:2])

# --- boxes ---------------------------------------------------------------------------------

width = d["contentWidth"]
bad = [(n.get("type"), n.get("size")) for n in walk(tree)
        if n.get("text") and (n["size"][1] <= 0 or n["size"][0] > width)]
check("every block of text has a box that fits the document", not bad, bad[:4])

wrapped = [n for n in walk(tree) if n.get("lines", 0) > 1]
check("at least one block wrapped onto several lines", wrapped,
        "no block has more than one line")

code = next((n for n in by_type(tree, "code")), None)
check("the code block kept every line", code is not None and code.get("lines", 0) == 3,
        code.get("lines") if code else None)

# --- the source map, which the next milestones copy markup through -------------------------

runs_ok = True
runs_seen = 0
for n in walk(tree):
    text = n.get("text")
    for run in n.get("runs") or []:
        runs_seen += 1
        charStart, charCount, srcOffset, srcLength, _ = run
        if text is None or charStart + charCount > len(text):
            runs_ok = False
        if srcOffset + srcLength > d["sourceLength"]:
            runs_ok = False

check("every text run was recorded", runs_seen > 10, runs_seen)
check("every run lies inside its label and inside the source", runs_ok)

# --- task items ----------------------------------------------------------------------------

boxes = by_type(tree, "li-checkbox")
check("a task item became a checkbox", len(boxes) == 2, len(boxes))
check("only the `[x]` item is checked",
        len([b for b in boxes if "checked" in (b.get("classes") or [])]) <= 1)
tasks = [n for n in by_type(tree, "li") if "md-task" in (n.get("classes") or [])]
check("a task item is marked as one", len(tasks) == 2, len(tasks))

# --- the reading order ----------------------------------------------------------------------

flow = s.ok("invoke", name="markdown.flow", args={"settle": 0.2})
entries = flow["entries"]

check("the document has a reading order", len(entries) > 15, len(entries))
check("every block of text is in it",
        len([e for e in entries if e["kind"] == "text"]) >= len(by_type(tree, "p")),
        len(entries))
check("so are the bullets and the checkboxes",
        [e for e in entries if e["kind"] == "marker"]
        and [e for e in entries if e["kind"] == "atomic"])

# Positions are continuous and never overlap: entry i ends where entry i+1 begins, one position
# apart - and that one position IS the break between two blocks.
gaps = [(a["begin"], a["length"], b["begin"]) for a, b in zip(entries, entries[1:])
        if a["begin"] + a["length"] + 1 != b["begin"]]
check("its positions are continuous", not gaps, gaps[:3])
check("and they end where the document does",
        entries[-1]["begin"] + entries[-1]["length"] == flow["textLength"],
        (entries[-1], flow["textLength"]))

labels = [n for n in walk(tree) if n.get("text")]
check("no block of text was left out of the reading order",
        len([e for e in entries if e["length"] > 0]) >= len(labels), (len(entries), len(labels)))

# --- a range hands back the markup that made it ---------------------------------------------


def rng(begin, end=None, mode="normalized"):
    args = {"begin": begin, "mode": mode, "settle": 0.05}
    if end is not None:
        args["end"] = end
    return s.ok("invoke", name="markdown.range", args=args)


whole = rng(0, flow["textLength"])
src = d["source"]
check("the whole document comes back as its own source",
        whole["markup"].strip() == src.strip(), whole["markup"][:60])
# Position 0 is the first CHARACTER of the document, which is one past the `# ` of its heading -
# the marker is not text and no run covers it. That the markup above still opens with `# ` is the
# edge repair putting the block back, and this is the check that says so.
check("its byte range starts at the first character, not at the markup",
        0 < whole["srcBegin"] <= 3 and whole["srcEnd"] >= len(src) - 2,
        (whole["srcBegin"], whole["srcEnd"], len(src)))
check("and the markup opens with the marker the range does not cover",
        whole["markup"].startswith("#"), whole["markup"][:20])

# A cut through the middle of a bold word: the fragment has to close what it opened.
bold_at = src.index("**bold**")
bold_entry = next(e for e in entries
        if e["srcOffset"] <= bold_at < e["srcOffset"] + e["srcLength"] and e["length"] > 0)
para_text = next(n for n in walk(tree)
        if n.get("text", "").startswith("A paragraph")).get("text")
inside = bold_entry["begin"] + para_text.index("bold") + 1
cut = rng(inside, inside + 2)
check("a range cut inside bold text comes back balanced",
        cut["markup"].startswith("**") and cut["markup"].endswith("**"), cut["markup"])
check("and the raw mode does not repair it",
        not rng(inside, inside + 2, "raw")["markup"].startswith("**"),
        rng(inside, inside + 2, "raw")["markup"])

# The text of a range is what was on screen, and the markup is how it was written: the second
# always holds the first.
check("the range knows what it said", cut["text"] == "ol", cut["text"])

# --- re-wrapping ----------------------------------------------------------------------------

narrow = s.ok("invoke", name="markdown.width", args={"width": 520, "settle": 0.5})
time.sleep(0.4)
narrow = dump()
para_narrow = next((n for n in by_type(narrow["tree"], "p")
        if n.get("text", "").startswith("A paragraph")), None)
check("narrowing the view narrows the blocks in it",
        para_narrow is not None and para_narrow["size"][0] < para["size"][0],
        (para_narrow or {}).get("size"))
check("and the text is re-wrapped to it",
        para_narrow is not None and para_narrow.get("wrap") == para_narrow["size"][0],
        (para_narrow or {}).get("wrap"))

reflow = s.ok("invoke", name="markdown.flow", args={"settle": 0.2})
check("re-wrapping does not disturb the reading order",
        reflow["entries"] == entries and reflow["textLength"] == flow["textLength"])

# --- selection --------------------------------------------------------------------------------


def sel(name, **args):
    args.setdefault("settle", 0.0)
    return s.ok("invoke", name="markdown." + name, args=args)


def ev(name, x, y, button="MouseLeft", mods=0):
    return {"event": name, "id": 1, "button": button, "x": x, "y": y, "modifiers": mods}


def send(*events):
    s.ok("input", events=list(events))


def frames(n=1):
    s.call("frame", count=n)
    time.sleep(0.03)


def point_of(position):
    """A point ON the text of a position: the caret's base is BELOW the line."""
    p = sel("position-point", position=position)
    return p["x"], p["y"] + p["height"] * 0.6


SHIFT, CTRL, TOUCH = 1 << 0, 1 << 2, 1 << 25

state = sel("select", begin=40, end=90)
check("a range is painted by the labels it crosses", len(state["entries"]) >= 1, state["entries"])
check("and by no others",
        all(e["cursorLength"] > 0 for e in state["entries"]), state["entries"])
check("the range knows its own text", state["text"] and state["begin"] == 40, state["begin"])

# The position under a point, and the point of a position, are each other's inverse. Everything
# a drag does rests on that, and nothing else in the stand would notice if it drifted by a line.
roundtrip = []
for position in (40, 60, 78, 90):
    x, y = point_of(position)
    roundtrip.append((position, sel("point", x=x, y=y)["position"]))
check("a position and its point are the same place", all(a == b for a, b in roundtrip), roundtrip)

sel("clear-selection")
cleared = sel("selection")
check("clearing takes the highlight off every label", not cleared["entries"], cleared["entries"])

# --- gestures ----------------------------------------------------------------------------------

x, y = point_of(60)
send(ev("MouseMove", x, y, "None"))
frames()
send(ev("Begin", x, y), ev("End", x, y))
frames(2)
single = sel("selection")
check("a click puts a caret and selects nothing", not single["hasSelection"], single["begin"])

send(ev("Begin", x, y), ev("End", x, y))
frames(2)
word = sel("selection")
check("a second click takes the word", word["hasSelection"] and " " not in word["text"],
        word["text"])

send(ev("Begin", x, y), ev("End", x, y))
frames(2)
block = sel("selection")
check("a third click takes the whole block", block["end"] - block["begin"] > word["end"] - word["begin"],
        (block["begin"], block["end"]))

sel("clear-selection")
time.sleep(0.6)
frames(4)
x0, y0 = point_of(40)
x1, y1 = point_of(90)
send(ev("MouseMove", x0, y0, "None"))
frames()
send(ev("Begin", x0, y0), ev("End", x0, y0))
frames(3)
time.sleep(0.6)
frames(4)
send(ev("MouseMove", x1, y1, "None", SHIFT))
frames()
send(ev("Begin", x1, y1, "MouseLeft", SHIFT), ev("End", x1, y1, "MouseLeft", SHIFT))
frames(3)
extended = sel("selection")
check("shift extends from the caret", (extended["begin"], extended["end"]) == (40, 90),
        (extended["begin"], extended["end"]))

# --- a drag selects, and does NOT scroll ---------------------------------------------------------

sel("clear-selection")
before = sel("selection")["scrollY"]
x0, y0 = point_of(40)
send(ev("MouseMove", x0, y0, "None"))
frames()
send(ev("Begin", x0, y0))
frames()
for i in range(1, 7):
    send(ev("Move", x0 + 30.0 * i, y0 - 20.0 * i))
    frames(1)
send(ev("End", x0 + 180.0, y0 - 120.0))
frames(2)
dragged = sel("selection")
check("a drag selects what it crosses", dragged["end"] - dragged["begin"] > 30,
        (dragged["begin"], dragged["end"]))
check("and the document does not scroll under it", dragged["scrollY"] == before,
        (before, dragged["scrollY"]))

# The code block has a horizontal scroll of its own and sits DEEPER in the tree, so it is offered
# the drag first; the selection threshold is what takes it back.
code_entry = next((e for e in entries if e["type"] == "code"), None)
check("the document has a code block in its reading order", code_entry is not None)
if code_entry:
    cx, cy = point_of(code_entry["begin"] + 2)
    sel("clear-selection")
    send(ev("MouseMove", cx, cy, "None"))
    frames()
    send(ev("Begin", cx, cy))
    frames()
    for i in range(1, 5):
        send(ev("Move", cx + 25.0 * i, cy))
        frames(1)
    send(ev("End", cx + 100.0, cy))
    frames(2)
    in_code = sel("selection")
    check("a drag inside the code block selects code rather than panning it",
            in_code["end"] > in_code["begin"]
            and in_code["begin"] >= code_entry["begin"], (in_code["begin"], in_code["end"]))

# --- keys ---------------------------------------------------------------------------------------

send({"event": "KeyPressed", "keycode": "A", "modifiers": CTRL},
        {"event": "KeyReleased", "keycode": "A", "modifiers": CTRL})
frames(3)
everything = sel("selection")
check("ctrl+A takes the whole document",
        (everything["begin"], everything["end"]) == (0, flow["textLength"]),
        (everything["begin"], everything["end"]))

send({"event": "KeyPressed", "keycode": "ESCAPE", "modifiers": 0},
        {"event": "KeyReleased", "keycode": "ESCAPE", "modifiers": 0})
frames(3)
check("escape drops it again", not sel("selection")["hasSelection"])

send({"event": "KeyPressed", "keycode": "A", "modifiers": CTRL},
        {"event": "KeyReleased", "keycode": "A", "modifiers": CTRL})
frames(3)

# --- copying --------------------------------------------------------------------------------------

check("copying the whole document succeeds", sel("copy")["ok"])
frames(2)


def read_clipboard(prefer):
    before = sel("clipboard-state")["deliveries"]
    sel("clipboard-read", prefer=prefer)
    for _ in range(40):
        st = sel("clipboard-state")
        if st["deliveries"] > before:
            return st["lastRead"]
        frames(1)
    return sel("clipboard-state")["lastRead"]


markdown_rep = read_clipboard(["text/markdown"])
check("the markup went to the clipboard as text/markdown",
        markdown_rep["type"] == "text/markdown", markdown_rep["type"])
check("and it is the document's own source", markdown_rep["text"].strip() == src.strip(),
        markdown_rep["text"][:60])

plain_rep = read_clipboard(["text/plain"])
check("the readable text went with it", plain_rep["type"] == "text/plain", plain_rep["type"])
check("and it is the text, not the markup", "**" not in plain_rep["text"],
        plain_rep["text"][:60])

# --- links ----------------------------------------------------------------------------------------

para_with_link = next((n for n in walk(tree) if n.get("linkRanges")), None)
check("the paragraph recorded where its link is", para_with_link is not None)
if para_with_link:
    link_entry = next(e for e in entries
            if e["kind"] == "text" and e["length"] == len(para_with_link["text"]))
    char_start, _, href = para_with_link["linkRanges"][0]
    lx, ly = point_of(link_entry["begin"] + char_start + 1)
    sel("clear-selection")
    send(ev("MouseMove", lx, ly, "None"))
    frames()
    send(ev("Begin", lx, ly), ev("End", lx, ly))
    frames(3)
    after_link = sel("selection")
    check("a click on a link follows it", after_link["lastLink"] == href, after_link["lastLink"])
    check("and leaves no selection behind", not after_link["hasSelection"])

# --- touch ------------------------------------------------------------------------------------------

sel("clear-selection")
time.sleep(0.6)
frames(4)
tx, ty = point_of(60)
send(ev("MouseMove", tx, ty, "None", TOUCH))
frames()
send(ev("Begin", tx, ty, "MouseLeft", TOUCH))
frames(4)
time.sleep(0.7)
frames(4)
touched = sel("selection")
send(ev("End", tx, ty, "MouseLeft", TOUCH))
frames(2)
check("a long press takes the word under the finger",
        touched["touchMode"] and touched["hasSelection"], touched)
check("and puts a handle on each end of it",
        touched["handles"] == touched["carets"], (touched["handles"], touched["carets"]))

# Dragging past the bottom edge pulls the document up under the pointer - without it a selection
# could never reach past one screenful.
sel("clear-selection")
ax, ay = point_of(40)
resting = sel("selection")["scrollY"]
send(ev("MouseMove", ax, ay, "None"))
frames()
send(ev("Begin", ax, ay))
frames()
send(ev("Move", ax, ay - 40.0))
frames()
send(ev("Move", ax, 6.0))
for _ in range(20):
    frames(1)
scrolled = sel("selection")["scrollY"]
send(ev("End", ax, 6.0))
frames(2)
check("a drag held at the edge scrolls the document to it", scrolled > resting,
        (resting, scrolled))

# ==================================================================================================
# M6 - the cascade reaches an inline, a picture sits inside a paragraph, a footnote leads somewhere.
#
# Everything below installs a document of its own, because the checks above measure screen points
# against positions in the sample and any edit to it would move all of them.
# ==================================================================================================

sel("clear-selection")

# Long enough that the footnote definitions are below the fold: a jump that does not have to move
# anything proves nothing.
M6_SRC = """# Notes

A claim[^note] with **bold** and a [link](https://xenolith.studio) in it.

> A quote with **bold** in it.

""" + "".join(f"Filler paragraph number {i} to push the notes off the screen.\n\n"
        for i in range(40)) + """
[^note]: The evidence.
"""

s.ok("invoke", name="markdown.source", args={"text": M6_SRC})
time.sleep(0.5)
frames(4)

m6 = s.ok("invoke", name="markdown.dump", args={"settle": 0.6})
m6_tree = m6["tree"]
m6_text = sel("range", begin=0)["text"]


def styles_at(needle, occurrence=0):
    """The style parameters covering the first character of `needle` in the document's text."""
    at = -1
    for _ in range(occurrence + 1):
        at = m6_text.index(needle, at + 1)
    got = s.ok("invoke", name="markdown.inline-styles", args={"position": at})
    return {p["name"]: p["value"] for p in (got.get("styles") or [])}


# --- the cascade reaches an inline ----------------------------------------------------------------

bold = styles_at("bold")
check("a stylesheet rule reached an inline construct", "font-weight" in bold, bold)
check("and it is the weight the built-in sheet asks for", bold.get("font-weight") == 700, bold)

link_style = styles_at("link")
check("a link takes its colour from the sheet", "color" in link_style, link_style)
check("and its underline too", "text-decoration" in link_style, link_style)

# The whole point of resolving through the cascade rather than a table: the SAME construct answers
# differently depending on where it sits.
s.ok("invoke", name="markdown.style",
        args={"css": "p strong { color: #0000ff; } blockquote p strong { color: #00aa00; }"})
time.sleep(0.6)
frames(4)

para_bold = styles_at("bold", 0)
quote_bold = styles_at("bold", 1)
check("the same construct resolves differently in two contexts",
        para_bold.get("color") != quote_bold.get("color"), (para_bold, quote_bold))
check("a paragraph's bold took the paragraph rule", para_bold.get("color") == "0,0,255", para_bold)
check("a quote's bold took the quote rule", quote_bold.get("color") == "0,170,0", quote_bold)

# An application sheet ABOVE the view: it reaches the inlines too, and a class beats a tag.
s.ok("invoke", name="markdown.app-style", args={"css": ".md-strong { font-style: italic; }"})
time.sleep(0.6)
frames(4)
check("a sheet above the view reaches an inline as well",
        "font-style" in styles_at("bold"), styles_at("bold"))

# A reload restyles the ranges in place; nothing is rebuilt, so the selection is still there.
sel("select", begin=2, end=8)
s.ok("invoke", name="markdown.style", args={"css": "em { font-weight: bold; }"})
time.sleep(0.6)
frames(4)
kept = sel("selection")
check("a stylesheet reload leaves the selection alone",
        kept["begin"] == 2 and kept["end"] == 8, (kept["begin"], kept["end"]))

# --- footnotes ------------------------------------------------------------------------------------

footnotes = next((n for n in walk(m6_tree) if "footnotes" in (n.get("classes") or [])), None)
check("the footnote definitions became a block", footnotes is not None)
check("and the block has a height of its own",
        footnotes and footnotes["size"][1] > 10, footnotes["size"] if footnotes else None)

definition = next((n for n in walk(m6_tree) if n.get("name") == "fn_1"), None)
check("the definition kept the id it is reached by", definition is not None)

anchors = {a["id"]: a["position"] for a in sel("anchors")["anchors"]}
check("the reference site is an anchor even though it is not a node", "fnref_1" in anchors, anchors)
check("and so is the definition", "fn_1" in anchors, anchors)
check("a heading is one too, which is a table of contents for free",
        any(k not in ("fn_1", "fnref_1") for k in anchors), anchors)

# The reference is a link into the document: the view follows it itself and the application is
# never told, which is what makes a footnote work with no application code.
sel("clear-selection")
before_scroll = sel("anchors")["scrollY"]
followed = s.ok("invoke", name="markdown.activate-link", args={"position": anchors["fnref_1"]})
frames(6)
check("the footnote reference is a link", followed["found"] and followed["href"] == "#fn_1",
        followed)
check("and following it did not bother the application", followed["lastLink"] == "", followed)

after_scroll = sel("anchors")["scrollY"]
check("following it scrolled the document to the definition", after_scroll > before_scroll,
        (before_scroll, after_scroll))

# The way back is the same operation in the other direction, and it is an anchor on an INLINE -
# there is no node anywhere in the scene that carries `fnref_1`.
m6_flow = sel("flow")["entries"]
m6_dump = s.ok("invoke", name="markdown.dump", args={})["tree"]
back_pos = None
for node in walk(m6_dump):
    for char_start, _, href in (node.get("linkRanges") or []):
        if href != "#fnref_1":
            continue
        entry = next((e for e in m6_flow
                if e["kind"] == "text" and e["length"] == len(node["text"])), None)
        if entry:
            back_pos = entry["begin"] + char_start
check("the definition carries a way back", back_pos is not None)

returned = s.ok("invoke", name="markdown.activate-link", args={"position": back_pos})
frames(6)
back_scroll = sel("anchors")["scrollY"]
check("and the way back returns to the reference",
        returned["href"] == "#fnref_1" and back_scroll < after_scroll,
        (returned, after_scroll, back_scroll))

# An outward link is still the application's, which is the other half of the same decision.
outward = s.ok("invoke", name="markdown.activate-link",
        args={"position": m6_text.index("link") + 1})
frames(2)
check("an outward link still reaches the application",
        outward["lastLink"].startswith("https://"), outward)

# --- a CSS-declared selection colour ---------------------------------------------------------------

# On the view's own class rather than its tag: a custom property declared on a selector the
# built-in sheet already uses does not survive the re-parse - see the note in MARKDOWN_UI_PLAN.md.
s.ok("invoke", name="markdown.style",
        args={"css": ".xl-ui-markdown-view { --md-selection-color: #ff8800; }"})
time.sleep(0.6)
frames(4)
sel("select", begin=2, end=10)
tinted = sel("selection")
check("the highlight colour comes from the stylesheet",
        [round(c * 255) for c in tinted["selectionColor"][:3]] == [255, 136, 0],
        tinted.get("selectionColor"))
sel("clear-selection")

# --- pictures ---------------------------------------------------------------------------------------

# Back to the full width first: a check above narrowed the view, and at 520 the logo no longer
# fits beside the words - which is correct behaviour but not what the next checks are about.
s.ok("invoke", name="markdown.width", args={"width": 1100, "settle": 0.5})
s.ok("invoke", name="markdown.file", args={"path": os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "images.md")})
time.sleep(0.8)
s.ok("invoke", name="markdown.dump", args={"settle": 0.8})
frames(6)

images = s.ok("invoke", name="markdown.images", args={})["images"]
check("both pictures reserved a box", len(images) == 2, len(images))

if len(images) == 2:
    inline_img, block_img = images
    check("the inline picture's box has the size of the file",
            inline_img["size"] == [598, 480], inline_img["size"])
    check("and the box was actually placed in the line",
            inline_img["box"][2] == 598 and inline_img["box"][3] == 480, inline_img["box"])
    check("the node was moved onto its box",
            inline_img["placed"][0] == inline_img["box"][0], (inline_img["placed"],
            inline_img["box"]))

    # The text after the picture starts past it: that is what "the image is IN the line" means,
    # as opposed to drawn over it.
    before_pt = sel("position-point", position=inline_img["position"] - 1)
    after_pt = sel("position-point", position=inline_img["position"] + 1)
    check("the text after the picture begins past its box",
            after_pt["x"] - before_pt["x"] >= inline_img["box"][2], (before_pt["x"],
            after_pt["x"]))

    # The box is decided from the file HEADER, before any pixel is decoded - so it is the same
    # once the texture has arrived. This is the milestone's readiness criterion.
    frames(20)
    time.sleep(0.5)
    frames(10)
    settled = s.ok("invoke", name="markdown.images", args={})["images"]
    check("the box did not move once the picture loaded",
            settled[0]["box"] == inline_img["box"], (inline_img["box"], settled[0]["box"]))

    img_text = sel("range", begin=0)["text"]
    check("the readable text carries the alt text, not the object character",
            "the logo" in img_text and "\ufffc" not in img_text, repr(img_text[:60]))

    markup = sel("range", begin=inline_img["position"], end=inline_img["position"] + 1)["markup"]
    check("and a copy of the picture's own position is the picture's markup",
            markup.strip().startswith("![the logo]"), markup)

figure = next((n for n in walk(s.ok("invoke", name="markdown.dump", args={})["tree"])
        if n.get("type") == "figcaption"), None)
check("a picture alone in its paragraph got a caption", figure is not None,
        figure["text"] if figure else None)

stop.set()
time.sleep(0.1)
s.ok("render", stop=True)
s.close()
proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
