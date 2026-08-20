#!/usr/bin/env python3
"""Drive the ui::TableView reorder stand (XL_TABLE_REORDER_TEST) over the inspector socket.

Everything here runs headless. The load-bearing facts are the ones a screenshot cannot show:

  * row GEOMETRY answers for a row that has no node. Only the nodes of a TableView are virtualized -
    rebuildRows() commits one controller item per row with the height it resolved beforehand - and
    the drop index and the insertion line are both derived from that. A rectangle for a row that is
    scrolled out of sight is the whole reason this lives in the engine;
  * the insertion line sits on a BOUNDARY between rows. A line drawn one pixel inside a row and a
    line drawn on its edge are the same picture at a glance and different answers to "where would
    this land";
  * a refused move changes neither the order NOR the selection - and a refusal looks exactly like a
    move that had nothing to do;
  * after a move the selection follows the ROW, not the index it used to sit at. The two agree for
    one frame and diverge forever after;
  * dragging with the grip and asking for the move by name must produce the SAME order, or the
    mouse and the keyboard are two features that merely resemble each other.

    tests/window/table-reorder-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-table-reorder-check.sock")

# What the stand declares, duplicated here on purpose: a check that reads its expectations out of
# the thing it is checking cannot fail.
ROW_COUNT = 40
ROW_HEIGHT = 28.0

# Where the stand puts the table: x = 48, anchored top-left at getWorkTop() - 20, 360x280. The
# caption strip is 76 tall, so the work area starts at 768 - 76 = 692.
TABLE_X = 48.0
TABLE_TOP = 692.0 - 20.0
TABLE_W = 360.0
TABLE_H = 280.0
TABLE_BOTTOM = TABLE_TOP - TABLE_H

# The grip column is the first track of `grid-template-columns: 24px 1fr 80px`.
GRIP_X = TABLE_X + 12.0

# Alt, as InputModifier: 1 << 3. Not 1 << 2, which is Ctrl.
ALT = 1 << 3


class Session:
    def __init__(self, path=ADDR, timeout=25.0):
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
    env["XL_TABLE_REORDER_TEST"] = "1"
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
    return s.invoke("table-reorder.state")


def values(st):
    return st.get("values") or []


def row_rect(row):
    r = s.invoke("table-reorder.row-rect", row=row)
    if not r.get("ok"):
        return None
    return (r["x"] / 100.0, r["y"] / 100.0, r["w"] / 100.0, r["h"] / 100.0)


def scene_y(table_y):
    return TABLE_BOTTOM + table_y


def drag_row(from_row, to_scene_y, step=3.0):
    """Press the grip of `from_row` and walk the pointer to `to_scene_y`.

    Two things this has to get right, and both were found the hard way:

    - the STEP has to be small. The grip cell is 24x28, and until the swipe passes DragSource's
      threshold nothing has claimed the pointer, so a Move that lands outside the cell is simply
      not delivered - a 7px step leaves the cell before the gesture has become a drag;
    - the frames have to ADVANCE. A DropTarget registers itself by being visited, so a drag whose
      whole life happens inside one dispatch never meets a target at all.
    """
    rect = row_rect(from_row)
    if not rect:
        return False
    y = scene_y(rect[1] + rect[3] / 2.0)

    s.ok("input", native=True, events=[{"event": "Begin", "x": GRIP_X, "y": y,
        "button": "MouseLeft"}])
    s.ok("frame", count=1)

    cur = y
    guard = 0
    while abs(cur - to_scene_y) > step and guard < 400:
        cur += step if to_scene_y > cur else -step
        s.ok("input", native=True, events=[{"event": "Move", "x": GRIP_X, "y": cur,
            "button": "MouseLeft"}])
        s.ok("frame", count=1)
        guard += 1

    s.ok("input", native=True, events=[{"event": "End", "x": GRIP_X, "y": to_scene_y,
        "button": "MouseLeft"}])
    s.ok("frame", count=2)
    return True


try:
    s.ok("frame", count=4)

    st = state()
    check("the table carries its rows", st["rowCount"] == ROW_COUNT, st["rowCount"])
    check("reordering is on", st["reorderEnabled"] is True)
    check("and it starts in declared order",
            values(st)[:4] == ["field0", "field1", "field2", "field3"], values(st)[:4])

    # --- geometry -----------------------------------------------------------------------------
    r0 = row_rect(0)
    r1 = row_rect(1)
    check("a row has a rectangle", r0 is not None and r0[3] == ROW_HEIGHT, r0)
    check("the next row sits exactly one row lower",
            r1 is not None and abs((r0[1] - r1[1]) - ROW_HEIGHT) < 0.01, (r0, r1))
    check("a row spans the table's width", r0 is not None and r0[2] == TABLE_W, r0)

    # The cell rectangles are the CSS track list, and asking for them must not depend on the grip
    # column being there: the caller declared that column, so the numbering is the caller's.
    grip = s.invoke("table-reorder.cell-rect", row=0, column=0)
    name = s.invoke("table-reorder.cell-rect", row=0, column=1)
    kind = s.invoke("table-reorder.cell-rect", row=0, column=2)
    check("the grip column is the 24px track the sheet declared",
            grip.get("ok") and grip["x"] == 0 and grip["w"] == 2400, grip)
    check("the name column follows it and takes the 1fr",
            name.get("ok") and name["x"] == 2400 and name["w"] == 25600, name)
    check("and the last column is the 80px track",
            kind.get("ok") and kind["x"] == 28000 and kind["w"] == 8000, kind)

    inside = s.invoke("table-reorder.index-at", x=100, y=r0[1] + 4.0)
    check("a point inside a row names that row", inside["index"] == 0, inside)
    # Just below the visible box, but still inside the CONTENT: that names the row which would be
    # there, the same convention getRowRect follows for a row scrolled out of sight.
    below = s.invoke("table-reorder.index-at", x=100, y=-50.0)
    check("a point below the viewport still names a row of the content",
            below["index"] == 11, below)

    # Past the last row entirely: 40 rows of 28 is 1120 of content, and the box is 280 tall.
    past = s.invoke("table-reorder.index-at", x=100, y=TABLE_H - (ROW_COUNT * ROW_HEIGHT) - 20.0)
    check("a point past the last row names none", past["index"] == -1, past)
    above = s.invoke("table-reorder.index-at", x=100, y=TABLE_H + 20.0)
    check("and so does a point before the first", above["index"] == -1, above)

    # --- the insertion line sits on a boundary --------------------------------------------------
    #
    # Asked at a point just below row 1's top edge - the top half of row 1, so the insertion lands
    # ABOVE it, on the boundary row 1 shares with row 0.
    boundary = s.invoke("table-reorder.boundary-at", x=100, y=r1[1] + r1[3] - 2.0)
    check("the top half of a row snaps to the boundary above it",
            boundary["boundary"] == 1, boundary)
    line = boundary.get("rect")
    check("the line is thin", line and line["h"] == 200, line)
    check("the line is ON the row edge, not inside the row",
            line and abs(line["y"] / 100.0 + 1.0 - (r1[1] + r1[3])) < 0.01,
            (line, r1[1] + r1[3]))
    check("and it spans the table", line and line["w"] == int(TABLE_W * 100))

    lower = s.invoke("table-reorder.boundary-at", x=100, y=r1[1] + 2.0)
    check("the bottom half snaps to the boundary below", lower["boundary"] == 2, lower)

    # --- geometry survives scrolling out of view --------------------------------------------------
    s.invoke("table-reorder.scroll", delta=200)
    s.ok("frame", count=3)
    scrolled = row_rect(0)
    check("a row scrolled out of view still has a rectangle", scrolled is not None, scrolled)
    check("and it is where it would be, above the visible box",
            scrolled is not None and scrolled[1] > TABLE_H, scrolled)
    last = row_rect(ROW_COUNT - 1)
    check("so does the last row, below the visible box",
            last is not None and last[1] < 0.0, last)
    check("a row that does not exist has none", row_rect(ROW_COUNT) is None)

    s.invoke("table-reorder.scroll", delta=-200)
    s.ok("frame", count=3)

    # --- the named move -------------------------------------------------------------------------
    s.invoke("table-reorder.reset-counters")
    s.invoke("table-reorder.select", row=2)
    s.ok("frame", count=2)
    check("a move is accepted",
            s.invoke("table-reorder.reorder", **{"from": 2, "to": 5})["ok"] is True)
    s.ok("frame", count=3)
    st = state()
    check("`to` is the row's FINAL index",
            values(st)[:7] == ["field0", "field1", "field3", "field4", "field5", "field2",
                    "field6"], values(st)[:7])
    check("the selection followed the ROW, not the index", st["selected"] == 5, st["selected"])
    check("and exactly one move was reported", st["moves"] == 1, st["moves"])

    named_order = values(st)

    check("a move onto itself is not a move",
            s.invoke("table-reorder.reorder", **{"from": 3, "to": 3})["ok"] is False)
    check("and it was not reported", state()["moves"] == 1)

    # --- the same thing by dragging ---------------------------------------------------------------
    #
    # The table is reset by moving field2 back where it was, so that the drag starts from the order
    # the named move started from and the two answers can be compared directly.
    s.invoke("table-reorder.reorder", **{"from": 5, "to": 2})
    s.ok("frame", count=3)
    s.invoke("table-reorder.reset-counters")
    st = state()
    check("the table is back to its declared order",
            values(st)[:6] == ["field0", "field1", "field2", "field3", "field4", "field5"],
            values(st)[:6])

    s.invoke("table-reorder.select", row=2)
    s.ok("frame", count=2)

    # Row 5's LOWER half, so the insertion snaps below it - which is boundary 6, and a row coming
    # from above lands at index 5. The same move the named call made.
    r5 = row_rect(5)
    check("the drag ran", drag_row(2, scene_y(r5[1] + 2.0)) is True)
    time.sleep(0.4)
    s.ok("frame", count=3)
    st = state()
    check("dragging produced the SAME order as the named move",
            values(st) == named_order, values(st)[:7])
    check("the drag reported one move", st["moves"] == 1, st["moves"])
    check("and the selection followed the dragged row", st["selected"] == 5, st["selected"])
    check("no insertion line is left behind",
            "table-insertion-line" not in s.ok("scene")["text"])

    # --- a refusal changes nothing -----------------------------------------------------------------
    s.invoke("table-reorder.reorder", **{"from": 5, "to": 2})
    s.ok("frame", count=3)
    s.invoke("table-reorder.reset-counters")
    s.invoke("table-reorder.select", row=1)
    s.ok("frame", count=2)
    before = values(state())

    s.invoke("table-reorder.refuse", value=True)
    check("a refused move reports the refusal",
            s.invoke("table-reorder.reorder", **{"from": 1, "to": 4})["ok"] is False)
    s.ok("frame", count=3)
    st = state()
    check("the order did not move", values(st) == before, values(st)[:6])
    check("the selection did not move", st["selected"] == 1, st["selected"])
    check("nothing was counted as a move", st["moves"] == 0 and st["refusals"] == 1,
            (st["moves"], st["refusals"]))
    s.invoke("table-reorder.refuse", value=False)

    # --- the keyboard ------------------------------------------------------------------------------
    s.invoke("table-reorder.reset-counters")
    s.invoke("table-reorder.select", row=1)
    s.ok("frame", count=2)
    before = values(state())

    s.ok("input", native=True, events=key("DOWN", ALT))
    time.sleep(0.3)
    s.ok("frame", count=3)
    st = state()
    check("Alt+Down moves the selected row down",
            values(st)[1] == before[2] and values(st)[2] == before[1], values(st)[:4])
    check("and the selection went with it", st["selected"] == 2, st["selected"])

    s.ok("input", native=True, events=key("UP", ALT))
    time.sleep(0.3)
    s.ok("frame", count=3)
    st = state()
    check("Alt+Up puts it back", values(st) == before, values(st)[:4])
    check("with the selection back too", st["selected"] == 1, st["selected"])
    check("two moves, one each way", st["moves"] == 2, st["moves"])

    # The edges: nothing to do, and nothing done.
    s.invoke("table-reorder.reset-counters")
    s.invoke("table-reorder.select", row=ROW_COUNT - 1)
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("DOWN", ALT))
    time.sleep(0.3)
    s.ok("frame", count=2)
    check("Alt+Down on the last row does nothing", state()["moves"] == 0, state()["moves"])

    s.invoke("table-reorder.select", row=0)
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("UP", ALT))
    time.sleep(0.3)
    s.ok("frame", count=2)
    check("Alt+Up on the first row does nothing", state()["moves"] == 0, state()["moves"])

    # Nothing selected: the combination is not the table's to take, and the field beside it keeps
    # working - which is what declining rather than swallowing buys.
    s.invoke("table-reorder.select", row=-1)
    s.invoke("table-reorder.focus-neighbour")
    s.ok("frame", count=2)
    s.ok("input", native=True, events=key("DOWN", ALT))
    time.sleep(0.3)
    s.ok("frame", count=2)
    st = state()
    check("with nothing selected Alt+Down moves nothing", st["moves"] == 0, st["moves"])
    check("and the neighbouring field is still there and focused",
            st["neighbourText"] == "abcdef" and st["neighbourFocused"] is True,
            (st["neighbourText"], st["neighbourFocused"]))

    # --- turning it off ------------------------------------------------------------------------------
    s.invoke("table-reorder.select", row=1)
    s.invoke("table-reorder.set-reorder", value=False)
    s.ok("frame", count=3)
    check("reordering can be turned off", state()["reorderEnabled"] is False)
    check("and then a move is refused",
            s.invoke("table-reorder.reorder", **{"from": 1, "to": 2})["ok"] is False)
    s.ok("input", native=True, events=key("DOWN", ALT))
    time.sleep(0.3)
    s.ok("frame", count=2)
    check("as is the keyboard", state()["moves"] == 0, state()["moves"])
finally:
    s.close()
    proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
