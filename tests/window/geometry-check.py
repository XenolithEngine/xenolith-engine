#!/usr/bin/env python3
"""Drive the window-geometry protocol (XL_GEOMETRY_TEST) over the inspector socket and assert.

Two things are checked, and they are the two halves of the protocol.

READING: what a scene sees through RenderServerChannel::getWindowGeometry() - a content rect in the
LOGICAL units WindowInfo::rect uses, the surface extent in pixels, and `hasPosition`, which is what
tells "the window is at the origin" apart from "this platform does not say where windows are".

RESTORING: a second Root window is opened at a requested position and read back through the same
protocol. That round trip - a rect handed to WindowInfo::rect and returned by getWindowGeometry() -
is what lets an application reopen its window where the user left it.

Headless owns its own geometry outright, so the round trip is exact here. Under a real window
manager the requested position is a hint; run this without --headless and expect drift.

    tests/window/geometry-check.py [path-to-testapp]
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-geometry-check.sock")

WIDTH, HEIGHT = 1024, 768
SECOND_X, SECOND_Y = 137, 89
SECOND_W, SECOND_H = 480, 320


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


def start_app(binary):
    env = dict(os.environ)
    env["XL_GEOMETRY_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen(
            [binary, "--headless", "--width", str(WIDTH), "--height", str(HEIGHT)],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(300):
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


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()
try:
    s.ok("frame", count=3)

    # --- reading -------------------------------------------------------------------------------
    state = s.invoke("geometry.state")
    g = state["geometry"]

    check("the platform reports a window position", state.get("positionCapability") is True)
    check("geometry says so too", g.get("hasPosition") is True)
    check("the logical size is the size the window was started at",
            (g["width"], g["height"]) == (WIDTH, HEIGHT), g)

    # The same numbers must come out of the window command and the window list - three readers, one
    # snapshot.
    wg = s.ok("window", op="geometry")
    check("`window geometry` agrees with the scene",
            (wg["x"], wg["y"], wg["width"], wg["height"])
                    == (g["x"], g["y"], g["width"], g["height"]), wg)

    root = [w for w in s.ok("windows")["windows"] if w.get("default")][0]
    check("`list_windows` agrees with the scene",
            (root.get("x"), root.get("y")) == (g["x"], g["y"]), root)

    # --- the notification ----------------------------------------------------------------------
    s.invoke("geometry.reset-counter")
    check("the counter starts clean", s.invoke("geometry.state")["changes"] == 0)

    s.ok("window", op="resize", width=900, height=700)
    time.sleep(0.8)
    s.ok("frame", count=3)

    state = s.invoke("geometry.state")
    check("a resize fires the geometry hook", state["changes"] >= 1, state["changes"])
    check("exactly once - an unchanged snapshot must not wake the scene",
            state["changes"] == 1, state["changes"])
    check("the new size reached the scene",
            (state["geometry"]["width"], state["geometry"]["height"]) == (900, 700),
            state["geometry"])
    check("what was notified is what is readable",
            state["lastNotified"] == state["geometry"])
    check("a resize does not move the window",
            (state["geometry"]["x"], state["geometry"]["y"]) == (g["x"], g["y"]))

    # --- the restore round trip ------------------------------------------------------------------
    opened = s.invoke("geometry.open-second", x=SECOND_X, y=SECOND_Y, width=SECOND_W,
            height=SECOND_H)
    check("the second window was requested", opened.get("ok") is True)

    time.sleep(1.5)
    s.ok("frame", count=3)

    second = s.invoke("geometry.state")["second"]
    check("the second window exists", second.get("open") is True)
    sg = second.get("geometry")
    check("it reports a geometry of its own", sg is not None)
    if sg:
        check("the requested POSITION came back",
                (sg["x"], sg["y"]) == (SECOND_X, SECOND_Y), sg)
        check("the requested SIZE came back",
                (sg["width"], sg["height"]) == (SECOND_W, SECOND_H), sg)
        check("and it says the position is real", sg.get("hasPosition") is True)

    listed = [w for w in s.ok("windows")["windows"] if w["id"] == "geometry-second"]
    check("the window list carries the second window's position",
            listed and (listed[0].get("x"), listed[0].get("y")) == (SECOND_X, SECOND_Y), listed)

    s.invoke("geometry.close-second")

finally:
    try:
        s.call("quit")
    except OSError:
        pass
    s.close()
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
