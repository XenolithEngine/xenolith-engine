#!/usr/bin/env python3
"""Play the source side of XDND against the drag-external stand on a live X11 server.

The stand's own run covers everything above the native window with a synthetic offer. This covers
the X11 backend under it: python-xlib is the other application, and the only thing it shares with
testapp is the X server, as a file manager would.

NOT part of the headless suite, and cannot be: it needs an X server (DISPLAY :1 here, Xwayland
will do). It takes no keyboard focus and moves no pointer - XDND is client messages and a
selection - so unlike xcb-side-check.py it does not care what else is on screen.

The claims, in order:

  * the window announces XDND version 5 (XdndAware);
  * a position over the file target is answered with XdndStatus accepting a copy, and a position
    over nothing with a refusal;
  * the drop converts XdndSelection to text/uri-list, the paths arrive decoded, and XdndFinished
    reports the copy;
  * a drag that leaves is never finished and drops nothing;
  * text offered as UTF8_STRING reaches the text field as text/plain.

    XL_TEST_DISPLAY=:1 tests/window/xdnd-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, select, socket, struct, subprocess, sys, time

from Xlib import X, display
from Xlib.protocol import event

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-xdnd-check.sock")
DISPLAY = os.environ.get("XL_TEST_DISPLAY", ":1")
BINARY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

PATHS = ["/tmp/xdnd check.txt", "/tmp/файл.png"]


def file_uri(path):
    out = "file://"
    for b in path.encode():
        c = chr(b)
        if c.isascii() and (c.isalnum() or c in "/-._~"):
            out += c
        else:
            out += "%%%02X" % b
    return out


URI_LIST = "".join(file_uri(p) + "\r\n" for p in PATHS).encode()

checks = 0
failures = 0


def check(name, ok, detail=""):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok or not detail else "  (" + str(detail) + ")"))


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


env = dict(os.environ)
env["XL_DRAG_EXTERNAL_TEST"] = "1"
env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
env["DISPLAY"] = DISPLAY
env.pop("WAYLAND_DISPLAY", None)   # force the xcb backend
env["SP_SESSION_TYPE"] = "x11"
try:
    os.unlink(ADDR)
except OSError:
    pass

proc = subprocess.Popen([BINARY, "--width", "800", "--height", "600"], env=env,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
s = None
try:
    for _ in range(600):
        if os.path.exists(ADDR):
            try:
                s = Session()
                break
            except OSError:
                pass
        time.sleep(0.05)
    if not s:
        raise SystemExit("app did not come up")

    # The stand's own synthetic run comes first; a real drag has to wait for the drag system
    state = None
    for _ in range(300):
        s.call("frame", count=1)
        state = s.invoke("drag-external.state")
        if state.get("summary"):
            break
        time.sleep(0.05)
    check("the stand finished its own run", bool(state and state.get("summary")), state)

    d = display.Display(DISPLAY)
    root = d.screen().root
    atom = d.intern_atom

    # testapp's window: the XdndAware one owned by our process
    target = None
    for _ in range(100):
        stack = [root]
        while stack and not target:
            w = stack.pop()
            try:
                pid = w.get_full_property(atom("_NET_WM_PID"), X.AnyPropertyType)
                aware = w.get_full_property(atom("XdndAware"), X.AnyPropertyType)
                if pid and pid.value[0] == proc.pid and aware:
                    target = w
                    break
                stack.extend(w.query_tree().children)
            except Exception:
                pass
        if target:
            break
        time.sleep(0.05)
    if not target:
        raise SystemExit("no XdndAware window of testapp")

    aware = target.get_full_property(atom("XdndAware"), X.AnyPropertyType)
    check("XdndAware announces version 5", aware.value[0] == 5, aware.value[0])

    # World points are pixels from the bottom-left of the content, which sits inside the frame
    # extents a client-side decoration draws into
    points = s.invoke("drag-external.points")
    geom = target.get_geometry()
    extents = target.get_full_property(atom("_GTK_FRAME_EXTENTS"), X.AnyPropertyType)
    left, right, top, bottom = (list(extents.value) if extents else [0, 0, 0, 0])
    origin = root.translate_coords(target, 0, 0)
    content_height = geom.height - top - bottom

    def root_point(p):
        return (int(origin.x + left + p["x"]), int(origin.y + top + (content_height - p["y"])))

    source = root.create_window(-10, -10, 1, 1, 0, 0, window_class=X.InputOnly,
                                visual=X.CopyFromParent)
    d.flush()

    payload = {}

    def send(kind, *data):
        values = [source.id] + list(data)
        values += [0] * (5 - len(values))
        ev = event.ClientMessage(window=target, client_type=atom(kind), data=(32, values))
        target.send_event(ev, event_mask=0)
        d.flush()

    def serve(e):
        # SelectionRequest for XdndSelection: answer from `payload`
        name = d.get_atom_name(e.target)
        data = payload.get(name)
        prop = e.property
        if data is None:
            prop = X.NONE
        else:
            e.requestor.change_property(e.property, e.target, 8, data)
        reply = event.SelectionNotify(time=e.time, requestor=e.requestor, selection=e.selection,
                                      target=e.target, property=prop)
        e.requestor.send_event(reply, event_mask=0)
        d.flush()

    def wait(kind, timeout=5.0):
        end = time.time() + timeout
        while time.time() < end:
            while d.pending_events():
                e = d.next_event()
                if e.type == X.SelectionRequest:
                    serve(e)
                elif e.type == X.ClientMessage and e.client_type == atom(kind):
                    return e.data[1]
            s.call("frame", count=1)
            select.select([d.fileno()], [], [], 0.05)
        return None

    def position(point, action="XdndActionCopy"):
        rx, ry = root_point(point)
        send("XdndPosition", 0, (rx << 16) | ry, X.CurrentTime, atom(action))
        return wait("XdndStatus")

    source.set_selection_owner(atom("XdndSelection"), X.CurrentTime)
    d.flush()

    # 1. files onto the file target
    payload = {"text/uri-list": URI_LIST}
    before = s.invoke("drag-external.state")
    send("XdndEnter", 5 << 24, atom("text/uri-list"))
    status = position(points["files"])
    check("a position over the file target is answered", status is not None)
    if status:
        check("the target accepts", status[1] & 1 == 1, status)
        check("the target says copy", status[4] == atom("XdndActionCopy"), status)
    send("XdndDrop", 0, X.CurrentTime)
    finished = wait("XdndFinished")
    check("the drop is finished", finished is not None)
    if finished:
        check("the drop is reported as accepted", finished[1] & 1 == 1, finished)
        check("the drop is reported as a copy", finished[2] == atom("XdndActionCopy"), finished)
    after = None
    for _ in range(40):
        s.call("frame", count=1)
        after = s.invoke("drag-external.state")
        if after["drops"] > before["drops"] and len(after["paths"]) >= 2:
            break
        time.sleep(0.05)
    check("the drop reached the target once", after["drops"] == before["drops"] + 1, after)
    check("the paths arrive decoded", after["paths"][-2:] == PATHS, after["paths"])

    # 2. over nothing, then out of the window
    before = after
    send("XdndEnter", 5 << 24, atom("text/uri-list"))
    empty = {"x": points["files"]["x"] + 400, "y": points["files"]["y"] + 150}
    status = position(empty)
    check("a position over nothing is answered", status is not None)
    if status:
        check("nothing accepts it", status[1] & 1 == 0, status)
    status = position(points["files"])
    check("moving onto the target is accepted again", bool(status and status[1] & 1), status)
    send("XdndLeave")
    stray = wait("XdndFinished", timeout=1.0)
    check("a drag that left is not finished", stray is None, stray)
    for _ in range(10):
        s.call("frame", count=1)
        time.sleep(0.02)
    after = s.invoke("drag-external.state")
    check("a drag that left dropped nothing", after["drops"] == before["drops"], after)
    check("a drag that left leaves no session", not after["dragging"], after)

    # 3. text into the field, offered the X11 way
    payload = {"UTF8_STRING": b"xdnd"}
    before = after
    send("XdndEnter", 5 << 24, atom("UTF8_STRING"))
    status = position(points["field"])
    check("the field accepts UTF8_STRING as text", bool(status and status[1] & 1), status)
    send("XdndDrop", 0, X.CurrentTime)
    finished = wait("XdndFinished")
    check("the text drop is finished as accepted", bool(finished and finished[1] & 1), finished)
    for _ in range(40):
        s.call("frame", count=1)
        after = s.invoke("drag-external.state")
        if after["field"].endswith("xdnd"):
            break
        time.sleep(0.05)
    check("the text reached the field", after["field"] == before["field"] + "xdnd", after["field"])

    source.destroy()
    d.flush()
finally:
    if s:
        try:
            s.call("quit")
        except Exception:
            pass
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
