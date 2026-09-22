#!/usr/bin/env python3
"""Check the two-pane navigator in examples/window/fileexplorer, headless.

The example is the first consumer of ui::FilesystemModel, and what is worth checking about it is
what no unit test can reach: that a directory on disk becomes the right rows, that the two view
modes and the two size settings move what they claim to, and that an image on disk ends up as
pixels of the right colour inside a tile.

So the check builds its own directory tree in a temporary directory - subdirectories, files of
several kinds, a dot-file and a directory of single-colour PNGs it writes itself - and then drives
the app over the inspector socket. Nothing here reads the user's home directory: the only thing
asserted about the places tree is the one root every target has.

Two things about how it asks:

  * a listing is walked on a worker thread, so `entryCount` right after `navigate` is 0 and every
    read of a listing polls rather than sleeps;
  * the places tree's titles are LOCALE TAGS ("@Locale:FE:Places:Filesystem"), because the model
    holds what the example gave it and the expansion happens in the label. That is what makes this
    check independent of the language the app started in.

Colours are checked by presence, not by position: the four generated PNGs are pure, widely spaced
colours, and the assertion is that enough pixels of each are on screen inside the right pane. A
tile that moved by a few pixels is not a failure; a thumbnail that never decoded is.

    tests/window/filesystem-explorer-check.py [path-to-fileexplorer]

With no argument it expects the debug x86_64-linux build of examples/window/fileexplorer. It starts
its own app instance, runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import os, shutil, struct, subprocess, sys, tempfile, time, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests/parity"))
import xlclient  # noqa: E402  (the inspector protocol, shared with the parity harness)

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-filesystem-explorer-check.sock")
WIDTH, HEIGHT = 1400, 900

# The right pane starts after the places frame; everything sampled for colour is inside it.
PANE_LEFT = 420

# Channel offsets of the raw screenshot formats (sprt::window ImageFormat): RGBA, BGRA
RAW_ORDER = {37: (0, 1, 2), 43: (0, 1, 2), 44: (2, 1, 0), 50: (2, 1, 0)}

# The pictures the check writes, and what it expects to find on screen again.
COLORS = [(220, 40, 40), (40, 200, 60), (50, 90, 230), (240, 200, 30)]
COLOR_TOLERANCE = 40
MIN_TILE_PIXELS = 400  # a 96px thumbnail is ~9000 px; a stray antialiased edge is not one

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


# --- the tree this check is about ---------------------------------------------------------------

def write_png(path, width, height, rgb):
    """A minimal 8-bit RGB PNG of one solid colour."""
    def chunk(tag, body):
        data = tag + body
        return struct.pack(">I", len(body)) + data + struct.pack(">I", zlib.crc32(data) & 0xffffffff)

    raw = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6))
                + chunk(b"IEND", b""))


def build_tree(root):
    """Directories first by name, then files by name - which is the order the pane must show."""
    for name in ("docs", "empty", "pictures"):
        os.makedirs(os.path.join(root, name))

    for i, color in enumerate(COLORS):
        write_png(os.path.join(root, "pictures", "img%d.png" % i), 96, 96, color)

    with open(os.path.join(root, "docs", "note.md"), "w") as f:
        f.write("# note\n")
    with open(os.path.join(root, "data.bin"), "wb") as f:
        f.write(b"\0" * 4096)
    with open(os.path.join(root, "readme.txt"), "w") as f:
        f.write("hello\n")
    with open(os.path.join(root, ".hidden"), "w") as f:
        f.write("secret\n")


# --- the app ------------------------------------------------------------------------------------

class App:
    def __init__(self, binary):
        env = dict(os.environ)
        env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
        try:
            os.unlink(ADDR)
        except OSError:
            pass
        self.proc = subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height",
                str(HEIGHT)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.session = None
        for _ in range(600):
            if self.proc.poll() is not None:
                break
            if os.path.exists(ADDR):
                try:
                    self.session = xlclient.Session("unix:" + ADDR, 60)
                    break
                except OSError:
                    pass
            time.sleep(0.05)
        if not self.session:
            self.proc.kill()
            raise SystemExit("app did not come up")

    def invoke(self, command, **args):
        return self.session.call("invoke", name="fileexplorer." + command, args=args)

    def step(self, count=2):
        """Frames are a REQUEST; `presented` is the receipt. Never sleep for one."""
        before = self.session.call("frame", count=0).get("presented", 0)
        self.session.call("frame", count=count)
        for _ in range(400):
            if self.session.call("frame", count=0).get("presented", 0) > before:
                return
            time.sleep(0.01)

    def settle(self, predicate, tries=40):
        """Poll a command until it answers what is being waited for, stepping frames between."""
        result = None
        for _ in range(tries):
            self.step(2)
            result = self.invoke("state")
            if predicate(result):
                return result
        return result

    def screenshot(self):
        """(width, height, rows of (r, g, b))"""
        shot = self.session.call("screenshot", format="raw")
        order = RAW_ORDER.get(shot.get("pixelFormat"))
        if order is None:
            raise SystemExit("unexpected screenshot pixel format: %r" % shot.get("pixelFormat"))
        data = shot["data"]
        if isinstance(data, str):
            data = xlclient.decode_bytes(data)
        width, height = shot["width"], shot["height"]
        r, g, b = order
        rows = []
        for y in range(height):
            line = data[y * width * 4:(y + 1) * width * 4]
            rows.append(list(zip(line[r::4], line[g::4], line[b::4])))
        return width, height, rows

    def close(self):
        try:
            self.session.call("quit", graceful=True)
            self.session.close()
            self.proc.wait(timeout=15)
        except (OSError, subprocess.TimeoutExpired):
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def count_colors(rows, left):
    """How many pixels right of `left` are near each of COLORS."""
    counts = [0] * len(COLORS)
    for row in rows:
        for pixel in row[left:]:
            for i, want in enumerate(COLORS):
                if (abs(pixel[0] - want[0]) <= COLOR_TOLERANCE
                        and abs(pixel[1] - want[1]) <= COLOR_TOLERANCE
                        and abs(pixel[2] - want[2]) <= COLOR_TOLERANCE):
                    counts[i] += 1
                    break
    return counts


# --- the sections -------------------------------------------------------------------------------

def check_places(app):
    print("places")
    result = app.invoke("places")
    places = result.get("places", [])
    paths = [p["path"] for p in places]
    titles = [p["title"] for p in places]

    check("places/any", len(places) > 0, f"got {len(places)} rows")
    # The one root every target has. The standard locations differ per machine, so nothing else
    # about the list is asserted.
    check("places/root", "/" in paths, f"paths={paths}")
    # Tags, not words: the model holds what the example gave it, and the label expands it. This is
    # what keeps the check independent of the language the app started in.
    check("places/tags", "@Locale:FE:Places:Filesystem" in titles, f"titles={titles}")
    check("places/depth", all(p["depth"] == 0 for p in places), "a root is nested")


def check_listing(app, tree):
    print("listing")
    app.invoke("navigate", path=tree)
    state = app.settle(lambda s: s["rows"] == 5)
    check("listing/path", state["path"] == tree, f"at {state['path']}")
    check("listing/count", state["entryCount"] == 5, f"{state['entryCount']} entries")

    entries = app.invoke("entries")["entries"]
    names = [e["name"] for e in entries]
    check("listing/order", names == ["docs", "empty", "pictures", "data.bin", "readme.txt"],
            f"names={names}")
    check("listing/dirs-first", [e["dir"] for e in entries] == [True, True, True, False, False],
            f"dirs={[e['dir'] for e in entries]}")

    sizes = {e["name"]: e["size"] for e in entries}
    check("listing/size", sizes.get("data.bin") == 4096, f"data.bin is {sizes.get('data.bin')}")
    check("listing/mtime", all(e["mtime"] > 0 for e in entries), "an entry has no mtime")

    # The dot-file is out until it is asked for, and back out afterwards.
    app.invoke("hidden", value=True)
    state = app.settle(lambda s: s["rows"] == 6)
    hidden_names = [e["name"] for e in app.invoke("entries")["entries"]]
    check("listing/hidden-shown", ".hidden" in hidden_names, f"names={hidden_names}")

    app.invoke("hidden", value=False)
    state = app.settle(lambda s: s["rows"] == 5)
    check("listing/hidden-gone",
            ".hidden" not in [e["name"] for e in app.invoke("entries")["entries"]],
            "the dot-file survived being turned off")


def check_navigation(app, tree):
    print("navigation")
    app.invoke("navigate", path=os.path.join(tree, "pictures"))
    state = app.settle(lambda s: s["rows"] == 4)
    check("navigation/down", state["path"] == os.path.join(tree, "pictures"), f"at {state['path']}")

    app.invoke("up")
    # `rows` and not `entryCount`: the first is what the view has derived, the second what the
    # model holds. A listing lands on the app thread and the view picks it up in the components
    # phase after, so waiting on the model lands one frame before there is anything to select.
    state = app.settle(lambda s: s["path"] == tree and s["rows"] == 5)
    check("navigation/up", state["path"] == tree, f"at {state['path']}")

    result = app.invoke("navigate", path=os.path.join(tree, "no-such-directory"))
    check("navigation/refused", "error" in result, f"answered {result}")
    state = app.invoke("state")
    check("navigation/unmoved", state["path"] == tree, f"at {state['path']}")

    result = app.invoke("select", path=os.path.join(tree, "readme.txt"))
    check("navigation/select-accepted", "error" not in result, f"answered {result}")
    state = app.invoke("state")
    check("navigation/select", state["selection"] == os.path.join(tree, "readme.txt"),
            f"selection={state['selection']}")


def check_settings(app, tree):
    print("settings")
    app.invoke("navigate", path=tree)
    app.settle(lambda s: s["rows"] == 5)

    app.invoke("mode", mode="table")
    check("settings/table", app.invoke("state")["mode"] == "table", "did not switch to table")
    app.invoke("mode", mode="icons")
    check("settings/icons", app.invoke("state")["mode"] == "icons", "did not switch to icons")

    result = app.invoke("mode", mode="mosaic")
    check("settings/mode-refused", "error" in result, f"answered {result}")

    # A bigger picture means fewer of them across the pane. The exact counts depend on the window
    # size, so what is asserted is the relation.
    app.invoke("tilesize", value=48)
    wide = app.settle(lambda s: s["tileSize"] == 48)["columns"]
    app.invoke("tilesize", value=192)
    narrow = app.settle(lambda s: s["tileSize"] == 192)["columns"]
    check("settings/columns", wide > narrow > 0, f"{wide} columns at 48, {narrow} at 192")
    app.invoke("tilesize", value=96)

    # The text size is not only a style: both views need a row height before a row node exists.
    small = app.invoke("fontsize", value=10)["rowHeight"]
    large = app.invoke("fontsize", value=20)["rowHeight"]
    check("settings/row-height", large > small > 0, f"{small} at 10pt, {large} at 20pt")
    app.invoke("fontsize", value=13)

    before = app.invoke("state")["locale"]
    after = app.invoke("locale")["locale"]
    check("settings/locale", after != before, f"{before} -> {after}")
    app.invoke("locale")


def check_reflow(app, tree):
    """The grid's column count follows the width of the pane, not only the picture size.

    This is the one mechanism here with no precedent in the repository: the reflow runs from
    ScrollController::setRebuildCallback, which nothing else uses. It is also the only path that
    keeps the user's scroll position across a resize, so it is worth an assertion of its own."""
    print("reflow")
    app.invoke("navigate", path=tree)
    app.settle(lambda s: s["rows"] == 5)
    app.invoke("mode", mode="icons")

    wide = app.invoke("state")["columns"]
    app.session.call("window", op="resize", width=WIDTH // 2, height=HEIGHT)
    narrow = app.settle(lambda s: s["columns"] != wide)["columns"]
    check("reflow/narrower", 0 < narrow < wide, f"{wide} columns at {WIDTH}, {narrow} at half")

    app.session.call("window", op="resize", width=WIDTH, height=HEIGHT)
    back = app.settle(lambda s: s["columns"] == wide)["columns"]
    check("reflow/restored", back == wide, f"{back} columns after resizing back, was {wide}")


def check_thumbnails(app, tree):
    print("thumbnails")
    pictures = os.path.join(tree, "pictures")
    app.invoke("navigate", path=pictures)
    app.settle(lambda s: s["rows"] == 4)

    stats = {}
    for _ in range(60):
        app.step(2)
        stats = app.invoke("thumbnails")
        if stats["cached"] >= len(COLORS) and stats["queued"] == 0 and stats["inFlight"] == 0:
            break
    check("thumbnails/decoded", stats.get("cached", 0) >= len(COLORS), f"stats={stats}")
    check("thumbnails/no-failures", stats.get("failed", 0) == 0, f"stats={stats}")

    app.step(3)
    _, _, rows = app.screenshot()
    counts = count_colors(rows, PANE_LEFT)
    check("thumbnails/on-screen", all(c >= MIN_TILE_PIXELS for c in counts), f"counts={counts}")

    # The same pictures again, three of them, on the face of the directory that holds them.
    app.invoke("navigate", path=tree)
    app.settle(lambda s: s["rows"] == 5)
    for _ in range(60):
        app.step(2)
        if app.invoke("thumbnails")["folders"] >= 1:
            break
    app.step(3)
    _, _, rows = app.screenshot()
    counts = count_colors(rows, PANE_LEFT)
    present = sum(1 for c in counts if c >= 100)
    check("thumbnails/folder-mosaic", present >= 3, f"counts={counts}")


def check_selfcheck(app):
    print("selfcheck")
    app.invoke("selfcheck")
    state = app.settle(lambda s: s["checks"] > 0, tries=60)
    check("selfcheck/ran", state["checks"] > 0, f"{state['checks']} checks")
    check("selfcheck/passed", state["failures"] == 0, f"{state['failures']} failures")


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "examples/window/fileexplorer",
            "stappler-build/x86_64-unknown-linux-gnu/debug/cc/fileexplorer")
    if not os.path.exists(binary):
        print("SKIP: %s is not built" % binary)
        return 0

    tree = tempfile.mkdtemp(prefix="xl-fileexplorer-")
    app = None
    try:
        build_tree(tree)
        app = App(binary)
        app.step(3)

        check_places(app)
        check_listing(app, tree)
        check_navigation(app, tree)
        check_settings(app, tree)
        check_reflow(app, tree)
        check_thumbnails(app, tree)
        check_selfcheck(app)
    finally:
        if app:
            app.close()
        shutil.rmtree(tree, ignore_errors=True)

    print(f"{checks} checks, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
