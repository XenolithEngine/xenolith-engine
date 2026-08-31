#!/usr/bin/env python3
"""Drive the ui::MenuSource / ui::MenuSystem stand (XL_MENU_TEST) over the inspector socket.

Everything here runs headless: no display, no compositor, no mouse. A menu is a REAL window in
headless mode - the pseudo-controller stands in for the window manager - so the popup and its
submenu are addressed by id, stepped and clicked exactly as they would be on a desktop.

The load-bearing assertion is that the metrics ARE the layout: `menu.metrics` reports the numbers
the measurement produced, `menu.state` reports the boxes the rows were actually drawn at, and the
two must agree. A menu whose reported row height and drawn row height differ is a menu whose popup
surface is the wrong size.

    tests/window/menu-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-menu-check.sock")

# The width the inline menu is pinned to by the stand, and the MenuStyle numbers it leaves at their
# defaults. Duplicated here on purpose: a check that reads its expectations out of the thing it is
# checking cannot fail.
MENU_WIDTH = 320.0
PADDING_H = 12.0
GAP = 12.0
ICON = 18.0


class Session:
    def __init__(self, path=ADDR, timeout=15.0):
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


def tap(x, y):
    return [{"event": "Begin", "x": x, "y": y, "button": "MouseLeft"},
            {"event": "End", "x": x, "y": y, "button": "MouseLeft"}]


def key(code, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def highlighted(tree):
    for line in tree.split("\n"):
        if "menu-item" in line and ".highlighted" in line and "#" in line:
            return line.split("#")[1].split(" ")[0]
    return None


def start_app(binary):
    env = dict(os.environ)
    env["XL_MENU_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(200):
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


def near(a, b, eps=0.5):
    return abs(a - b) <= eps


def item(state, name):
    for it in state["items"]:
        if it["name"] == name:
            return it
    return None


def row(metrics, name):
    for it in metrics["rows"]:
        if it["item"] == name:
            return it
    return None


def popup_id(s, index=0):
    wins = [w for w in s.ok("windows")["windows"] if w["type"] == "Popup"]
    return wins[index]["id"] if len(wins) > index else None


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()
try:
    s.ok("frame", count=3)

    # --- the columns are shared by every row -------------------------------------------------
    m = s.invoke("menu.metrics")
    check("menu is pinned to its width", near(m["width"], MENU_WIDTH), m["width"])

    total = (PADDING_H * 2 + m["leadingColumn"] + GAP + m["textColumn"] + GAP
            + m["shortcutColumn"] + GAP + m["trailingColumn"])
    check("columns add up to the width", near(total, m["width"]), f"{total} != {m['width']}")
    check("a leading column exists (icons and checks live in it)", near(m["leadingColumn"], ICON))
    check("a trailing column exists (the submenu chevron)", near(m["trailingColumn"], ICON))
    check("an accelerator column exists", m["shortcutColumn"] > 0.0)

    # --- the metrics ARE the layout ------------------------------------------------------------
    st = s.invoke("menu.state")
    mismatched = [r["item"] for r in m["rows"]
            if item(st, r["item"]) and item(st, r["item"]).get("node")
            and not near(item(st, r["item"])["node"]["height"], r["height"])]
    check("every row is drawn at the height that was reported", not mismatched, mismatched)

    widths = {it["name"]: it["node"]["width"] for it in st["items"] if it.get("node")}
    check("every row spans the whole menu",
            all(near(w, m["width"]) for w in widths.values()), widths)

    # Rows stack from the top, contiguously: the panel is padding, rows, padding.
    y = m["panel"]["height"] - 6.0
    stacked = True
    for r in m["rows"]:
        node = item(st, r["item"]).get("node") if item(st, r["item"]) else None
        if node and not near(node["y"], y):
            stacked = False
            break
        y -= r["height"]
    check("rows stack contiguously from the top", stacked)

    # --- wrapping ------------------------------------------------------------------------------
    plain = row(m, "plain")
    long_row = row(m, "long-title")
    check("a long title wraps and makes its row taller",
            long_row["height"] > plain["height"] * 2, f"{long_row['height']} vs {plain['height']}")
    check("the wrapped height is the title's, not padding",
            long_row["titleHeight"] > plain["titleHeight"])

    sub = row(m, "with-subtitle")
    check("a subtitle adds its own measured height",
            near(sub["height"], max(32.0, 12.0 + sub["titleHeight"] + sub["subtitleHeight"])),
            sub)

    s.invoke("menu.set-width", value=220.0)
    s.ok("frame", count=3)
    narrow = s.invoke("menu.metrics")
    check("a narrower menu wraps the same title further",
            row(narrow, "long-title")["height"] > long_row["height"])
    check("the text column follows the width",
            narrow["textColumn"] < m["textColumn"])
    s.invoke("menu.set-width", value=MENU_WIDTH)
    s.ok("frame", count=3)

    # --- what is and is not a row --------------------------------------------------------------
    check("a hidden item occupies no row", row(m, "hidden") is None)
    check("a hidden item has no node", item(st, "hidden").get("node") is None)
    check("a separator occupies its own height", near(row(m, "separator")["height"], 9.0))
    check("a custom item is measured by its own answer", near(row(m, "custom")["height"], 28.0))
    check("the custom node was built exactly once", st["customBuilds"] == 1, st["customBuilds"])

    # --- the accelerator ------------------------------------------------------------------------
    check("the bound hotkey is rendered as text",
            item(st, "with-hotkey").get("shortcut") == "Ctrl+S",
            item(st, "with-hotkey").get("shortcut"))

    # --- node reuse ------------------------------------------------------------------------------
    s.invoke("menu.set-checked", item="toggle", value=True)
    s.ok("frame", count=3)
    st2 = s.invoke("menu.state")
    check("checking an item does not rebuild the menu", st2["customBuilds"] == 1,
            st2["customBuilds"])
    check("the check is on the model", item(st2, "toggle")["checked"])
    s.invoke("menu.set-checked", item="toggle", value=False)

    s.invoke("menu.set-visible", item="plain", value=False)
    s.ok("frame", count=3)
    check("hiding an item removes its row",
            row(s.invoke("menu.metrics"), "plain") is None)
    s.invoke("menu.set-visible", item="plain", value=True)
    s.ok("frame", count=3)
    check("showing it back returns the row",
            row(s.invoke("menu.metrics"), "plain") is not None)
    check("and still no rebuild of the custom node",
            s.invoke("menu.state")["customBuilds"] == 1)

    # --- the popup -------------------------------------------------------------------------------
    s.invoke("menu.reset-counters")
    s.invoke("menu.open")
    time.sleep(0.6)
    root = popup_id(s)
    check("the menu opens as a window of its own", root is not None)

    st = s.invoke("menu.state")
    check("the popup is a native surface (headless emulates the WM)", st.get("popupNative") is True)

    s.ok("frame", count=3, window=root)
    tree = s.ok("scene", window=root)["text"]
    check("the popup carries the same rows", "#with-hotkey" in tree and "#submenu" in tree)
    check("a disabled row is marked for CSS",
          "#disabled .xl-ui-menu-item :disabled" in tree, tree)
    check("a submenu row is marked for CSS", "#submenu .xl-ui-menu-item .submenu" in tree)
    check("a hidden item is not in the popup either", "#hidden" not in tree)

    # Row geometry inside the popup, read from its own metrics-free tree: a row's `pos` is its TOP
    # (anchor TopLeft), so the row spans [y - height, y].
    def popup_row_center(name):
        for line in tree.split("\n"):
            if f"#{name} " in line and "menu-item" in line:
                sz = line.split("sz=")[1].split(" ")[0]
                pos = line.split("pos=(")[1].split(")")[0]
                h = float(sz.split("x")[1])
                top = float(pos.split(",")[1])
                return top - h / 2.0
        return None

    # --- the corners of a shaped window ------------------------------------------------------------
    # A popup surface IS its panel - the window is sized to it exactly - so a `border-radius` on that
    # panel leaves the window's four corners outside the shape anything draws, showing whatever the
    # scene was cleared to. That used to be WHITE, which put a bright speck at each corner of every
    # menu. Raw pixels rather than the PNG: the four bytes at the origin are the whole claim, and
    # white and transparent are the same in either channel order.
    shot = s.ok("screenshot", window=root, format="raw")
    data = shot["data"]
    if isinstance(data, str) and data.startswith("BASE64:"):
        data = base64.urlsafe_b64decode(data[7:] + "=" * (-len(data[7:]) % 4))
    corner = tuple(data[0:4])
    check("a menu's corner is not painted white", corner != (255, 255, 255, 255), corner)
    check("...it is cleared to nothing, so a compositor can show what is behind it",
            corner == (0, 0, 0, 0), corner)

    # --- pointing at a row, with a real pointer ---------------------------------------------------
    # The only check here that goes through one. A popup surface never takes the keyboard focus -
    # it has no reason to - and the mouse-over recognizer was gated on focus as well as on the
    # pointer, so no row of any menu ever reported a hover: nothing highlighted, and a submenu could
    # only be opened by clicking it. `menu.hover` further down drives the same call directly, which
    # is what lets it say anything about the delays; this one says the pointer reaches it at all.
    s.ok("input", window=root, native=True,
            events=[{"event": "MouseMove", "x": 60, "y": popup_row_center("submenu")}])
    for _ in range(12):
        time.sleep(0.08)
        s.ok("frame", count=1, window=root)
    check("pointing at a submenu row opens it, with no click", popup_id(s, 1) is not None)
    check("...and pointing is not an activation", s.invoke("menu.state")["activations"] == 0)

    # The diagonal trip into it. Crossing a row of the parent on the way is a hover like any other
    # and arms the parent's close; arriving in the level below is what has to call that off. The
    # delay alone cannot: a pointer that took longer than it, or that stopped on the way, would have
    # the level taken down from under it - which is what happened, the surface being destroyed while
    # the pointer sat in it.
    child = popup_id(s, 1)
    child_rows = s.ok("scene", window=child)["text"]

    def row_center_in(tree, name):
        for line in tree.split("\n"):
            if f"#{name} " in line and "menu-item" in line:
                sz = line.split("sz=")[1].split(" ")[0]
                pos = line.split("pos=(")[1].split(")")[0]
                return float(pos.split(",")[1]) - float(sz.split("x")[1]) / 2.0
        return None

    s.ok("input", window=root, native=True,
            events=[{"event": "MouseMove", "x": 60, "y": popup_row_center("plain")}])
    time.sleep(0.1)
    s.ok("frame", count=1, window=root)
    s.ok("input", window=child, native=True,
            events=[{"event": "MouseMove", "x": 60, "y": row_center_in(child_rows, "sub-one")}])
    for _ in range(16):
        time.sleep(0.08)
        s.ok("frame", count=1, window=root)
        if popup_id(s, 1) is not None:
            s.ok("frame", count=1, window=child)
    check("the pointer resting in a submenu keeps it open past the close delay",
            popup_id(s, 1) == child, (popup_id(s, 1), child))

    # And back out to the parent: the close it forgot is armed again by the row it returns to,
    # which is the only thing that should re-arm it
    s.ok("input", window=root, native=True,
            events=[{"event": "MouseMove", "x": 60, "y": popup_row_center("plain")}])
    for _ in range(14):
        time.sleep(0.08)
        s.ok("frame", count=1, window=root)
    check("...and going back to a row of the parent takes it down again", popup_id(s, 1) is None)

    # --- the submenu chain -------------------------------------------------------------------------
    s.ok("input", window=root, native=True, events=tap(200, popup_row_center("submenu")))
    time.sleep(0.8)
    child = popup_id(s, 1)
    check("a submenu row opens a second surface", child is not None)
    check("opening a submenu is not an activation",
            s.invoke("menu.state")["activations"] == 0)

    s.ok("frame", count=3, window=child)
    child_tree = s.ok("scene", window=child)["text"]
    check("the submenu carries its own items", "#sub-one" in child_tree and "#sub-two" in child_tree)

    def child_row_center(name):
        for line in child_tree.split("\n"):
            if f"#{name} " in line and "menu-item" in line:
                sz = line.split("sz=")[1].split(" ")[0]
                pos = line.split("pos=(")[1].split(")")[0]
                return float(pos.split(",")[1]) - float(sz.split("x")[1]) / 2.0
        return None

    s.ok("input", window=child, native=True, events=tap(80, child_row_center("sub-two")))
    time.sleep(0.8)
    st = s.invoke("menu.state")
    check("activating a leaf runs its own callback", "callback:sub-two" in (st["log"] or []))
    check("and reports through the root, once", st["activations"] == 1, st["activations"])
    check("the whole chain is taken down",
            not [w for w in s.ok("windows")["windows"] if w["type"] == "Popup"])
    check("the close callback fired", st["popupOpen"] is False)

    # --- KeepOpen and disabled ----------------------------------------------------------------------
    s.invoke("menu.reset-counters")
    s.invoke("menu.open")
    time.sleep(0.6)
    root = popup_id(s)
    s.ok("frame", count=3, window=root)
    tree = s.ok("scene", window=root)["text"]

    s.ok("input", window=root, native=True, events=tap(200, popup_row_center("disabled")))
    time.sleep(0.6)
    check("a disabled row does nothing", s.invoke("menu.state")["activations"] == 0)
    check("and does not close the menu", popup_id(s) is not None)

    s.ok("input", window=root, native=True, events=tap(200, popup_row_center("toggle")))
    time.sleep(0.6)
    st = s.invoke("menu.state")
    check("a KeepOpen item activates", st["activations"] == 1, st["activations"])
    check("and leaves the menu open", popup_id(s) is not None)
    check("its own callback flipped the model", item(st, "toggle")["checked"])

    s.ok("input", window=root, native=True, events=tap(200, popup_row_center("plain")))
    time.sleep(0.6)
    check("an ordinary item closes the menu", popup_id(s) is None)
    check("and is reported", s.invoke("menu.state")["lastActivated"] == "plain")

    # --- the keyboard, and who owns it ------------------------------------------------------------
    #
    # An inline menu is a list of commands in somebody else's panel, so it does NOT hold the
    # keyboard by default - the field beside it does. Turning the mode on gives the menu an
    # exclusive focus group, and that is what the two halves of this section are about: the arrows
    # start reaching the menu, and they stop reaching the field.
    s.invoke("menu.close")
    time.sleep(0.5)

    st = s.invoke("menu.state")
    check("an inline menu does not take the keyboard by default", st["keyboard"] is False)

    s.invoke("menu.focus-neighbour")
    s.ok("frame", count=3)
    s.ok("input", native=True, events=key("HOME"))
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("so the arrows go to the field beside it", st["neighbourCursor"] == 0,
            st["neighbourCursor"])
    check("and the menu has nothing highlighted", st["highlighted"] == "", st["highlighted"])

    s.invoke("menu.keyboard", value=True)
    s.ok("frame", count=3)
    s.ok("input", native=True, events=key("DOWN"))
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("with the keyboard on, Down lands on the first enabled row",
            st["highlighted"] == "plain", st["highlighted"])
    check("and the field no longer sees the arrows at all", st["neighbourCursor"] == 0,
            st["neighbourCursor"])

    s.ok("input", native=True, events=key("END"))
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("End goes to the last row that can be highlighted", st["highlighted"] == "submenu",
            st["highlighted"])
    check("the field still has not moved", st["neighbourCursor"] == 0, st["neighbourCursor"])

    s.invoke("menu.keyboard", value=False)
    s.ok("frame", count=3)
    s.ok("input", native=True, events=key("END"))
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("taking the keyboard away hands the arrows back",
            st["neighbourCursor"] == len("abcdef"), st["neighbourCursor"])
    check("and drops the highlight with it", st["highlighted"] == "", st["highlighted"])

    # --- the same keyboard in the popup, where it is on by default ---------------------------------
    s.invoke("menu.reset-counters")
    s.invoke("menu.open")
    time.sleep(0.6)
    root = popup_id(s)
    s.ok("frame", count=3, window=root)

    s.ok("input", window=root, native=True, events=key("DOWN"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    check("a popup menu answers the keyboard without being asked",
            highlighted(s.ok("scene", window=root)["text"]) == "plain")

    s.ok("input", window=root, native=True, events=key("END"))
    time.sleep(0.3)
    s.ok("frame", count=3, window=root)
    check("End reaches the submenu row",
            highlighted(s.ok("scene", window=root)["text"]) == "submenu")

    s.ok("input", window=root, native=True, events=key("RIGHT"))
    time.sleep(0.8)
    child = popup_id(s, 1)
    check("Right opens the submenu as a second surface", child is not None)
    check("and opening it is still not an activation",
            s.invoke("menu.state")["activations"] == 0)

    s.ok("input", window=child, native=True, events=key("LEFT"))
    time.sleep(0.8)
    check("Left takes that level down", popup_id(s, 1) is None)
    check("and leaves the menu it came from standing", popup_id(s) is not None)

    s.ok("input", window=root, native=True, events=key("ESCAPE"))
    time.sleep(0.8)
    check("Escape takes the whole chain down", popup_id(s) is None)
    check("and chooses nothing", s.invoke("menu.state")["activations"] == 0)

    # --- the pointer opens the levels ---------------------------------------------------------------
    #
    # WHAT IS DRIVEN HERE, AND WHERE THIS STOPS. `menu.hover` calls MenuSystem::handleItemHovered -
    # the very call ui::MenuItem makes on the entering edge of a real hover - so everything above
    # that call is checked: which row arms what, the two delays, the cancellation, the idempotence
    # and the depth. That a real pointer produces the call is MenuItem's hover-edge tracker, and it
    # cannot be driven from this side at all: an injected MouseMove does not put the window's
    # POINTER STATE over the row, and the mouse-over recognizer gates on exactly that. It needs a
    # real pointer, the way xcb-side-check.py needs a real keyboard.
    #
    # The delays are set by the check rather than waited out: a dwell is a number, and a check that
    # sleeps through the default one is checking the clock.
    s.invoke("menu.reset-counters")
    s.invoke("menu.hover-config", enabled=True, open=0, close=0)
    s.invoke("menu.open")
    time.sleep(0.6)
    root = popup_id(s)

    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.8)
    child = popup_id(s, 1)
    check("a hovered submenu row opens the level", child is not None)
    check("and a hover is not a choice", s.invoke("menu.state")["activations"] == 0, )

    chain = s.invoke("menu.chain")
    check("the chain knows which row the open level hangs off",
            chain["depth"] == 2 and chain["levels"][0]["child"] == "submenu", chain)

    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.5)
    check("hovering the same row again keeps the very same surface", popup_id(s, 1) == child,
            popup_id(s, 1))

    s.invoke("menu.hover", item="plain", level=1)
    time.sleep(0.5)
    check("hovering another row takes the level down", popup_id(s, 1) is None)
    check("and leaves the menu it belonged to standing", popup_id(s) is not None)
    check("the chain is one level again", s.invoke("menu.chain")["depth"] == 1)

    # The dwell is real, and it is CANCELLED rather than queued.
    s.invoke("menu.hover-config", open=4000, close=4000)
    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.4)
    check("a dwell that has not elapsed opens nothing", popup_id(s, 1) is None)
    s.invoke("menu.hover", item="plain", level=1)
    time.sleep(0.6)
    check("and a hover on another row cancels it rather than queueing it", popup_id(s, 1) is None)

    # The reason the close waits longer than the open: the pointer on its way into a submenu crosses
    # the rows below the one that opened it, and coming back must find it still there.
    s.invoke("menu.hover-config", open=0, close=4000)
    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.6)
    child = popup_id(s, 1)
    check("a zero dwell opens on the spot", child is not None)
    s.invoke("menu.hover", item="plain", level=1)
    time.sleep(0.3)
    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.5)
    check("crossing a row on the way back leaves the submenu standing, and the same one",
            popup_id(s, 1) == child, popup_id(s, 1))

    # --- nested levels, plural ----------------------------------------------------------------------
    s.invoke("menu.hover-config", open=0, close=0)
    s.invoke("menu.hover", item="sub-more", level=2)
    time.sleep(0.8)
    chain = s.invoke("menu.chain")
    check("a hover inside a submenu opens a THIRD level", chain["depth"] == 3, chain)
    check("and every level names the row the next one hangs off",
            [lv["child"] for lv in chain["levels"]] == ["submenu", "sub-more", ""], chain)
    check("three surfaces stand at once",
            len([w for w in s.ok("windows")["windows"] if w["type"] == "Popup"]) == 3)

    s.invoke("menu.hover", item="sub-one", level=2)
    time.sleep(0.5)
    check("a hover on a plain row of that submenu takes down only the level below it",
            s.invoke("menu.chain")["depth"] == 2)

    # A row that cannot be chosen cannot be entered either.
    s.invoke("menu.hover", item="plain", level=1)
    time.sleep(0.4)
    s.invoke("menu.set-enabled", item="submenu", value=False)
    s.ok("frame", count=3, window=root)
    s.invoke("menu.hover", item="submenu", level=1)
    time.sleep(0.5)
    check("a disabled submenu row does not open on hover either",
            s.invoke("menu.chain")["depth"] == 1)
    s.invoke("menu.set-enabled", item="submenu", value=True)

    s.invoke("menu.close")
    time.sleep(0.5)

    # --- the same answer with no window in play -----------------------------------------------------
    #
    # The inline menu's handlers only write down what was asked of them, which is what makes the two
    # halves of a hover separable: the submenu is decided whether or not the keyboard is in play, and
    # the highlight is not.
    s.invoke("menu.reset-counters")
    s.invoke("menu.hover", item="submenu", level=0)
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("an inline menu answers the pointer through the same seam",
            "submenu-open:submenu" in (st["log"] or []), st["log"])
    check("without taking a highlight it does not own", st["highlighted"] == "", st["highlighted"])

    s.invoke("menu.hover", item="plain", level=0)
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("and a hover elsewhere asks it to close", (st["log"] or [])[-1] == "submenu-close",
            st["log"])
    check("none of which is an activation", st["activations"] == 0, st["activations"])

    # --- and the switch turns all of it off ---------------------------------------------------------
    s.invoke("menu.hover-config", enabled=False)
    s.invoke("menu.reset-counters")
    s.invoke("menu.hover", item="submenu", level=0)
    time.sleep(0.3)
    check("with hover-open off the pointer decides nothing",
            (s.invoke("menu.state")["log"] or []) == [])

    s.invoke("menu.activate", item="submenu")
    time.sleep(0.3)
    st = s.invoke("menu.state")
    check("and the click opens it exactly as it always did",
            "submenu-open:submenu" in (st["log"] or []), st["log"])
    check("which is still not an activation", st["activations"] == 0, st["activations"])
    s.invoke("menu.hover-config", enabled=True)

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
