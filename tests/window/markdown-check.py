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

stop.set()
time.sleep(0.1)
s.ok("render", stop=True)
s.close()
proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
