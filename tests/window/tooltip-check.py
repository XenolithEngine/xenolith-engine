#!/usr/bin/env python3
"""Drive the tooltip stand (XL_TOOLTIP_TEST) over the inspector socket.

A hint is data on a node now, and ONE listener on the scene resolves which node the pointer rests
on, out of the frame's hit-test registry. Everything that used to fall out of having a listener per
node has to be checked deliberately:

  * the hints here are declared in init(), before the layout is in a scene, and nothing acquires a
    TooltipSystem. A component cannot notice its own arrival in a scene the way a listener could, so
    the coordinator existing at all is the first check - and the one that would silently break every
    hint an application declares while building a widget;
  * the registry is walked backwards, so the upper of two overlapping nodes is the one hovered;
  * the hover padding belongs to the NODE, so the same point resolves differently for a node that
    declared one;
  * a disabled hint is invisible to the resolution while its node stays perfectly ordinary;
  * a node slid out from under a pointer that never moved loses the hint. That case used to come
    from each target's own geometry updates and is now the per-frame re-resolution - no amount of
    synthetic pointer movement would catch it.

    tests/window/tooltip-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

# TooltipLayout::PadHover
PAD_HOVER = 16.0


class Session:
    def __init__(self, path, timeout=25.0):
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


def start_app(binary, addr):
    env = dict(os.environ)
    env["XL_TOOLTIP_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(addr):
            try:
                Session(addr).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


class Stand:
    def __init__(self, binary, addr):
        self.proc = start_app(binary, addr)
        self.s = Session(addr)
        self.step(3)

    def step(self, n=2):
        self.s.ok("frame", count=n)
        time.sleep(0.03)

    def state(self):
        return self.s.invoke("tooltip.state")

    def move(self, x, y):
        """Put the pointer somewhere and let the scene settle."""
        self.s.ok("input", native=True, events=[{
            "event": "MouseMove", "x": x, "y": y, "button": "None",
        }])
        self.step(2)

    def wait(self, seconds, predicate=None):
        """Keep frames coming for `seconds` - the dwell is an action, so it needs them."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            self.step(1)
            time.sleep(0.04)
            if predicate and predicate():
                return True
        return False

    def close(self):
        try:
            self.s.ok("quit")
        except Exception:
            pass
        self.s.close()
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()


def centre(r, fx=0.5, fy=0.5):
    return r["x"] + r["width"] * fx, r["y"] + r["height"] * fy


binary = sys.argv[1] if len(sys.argv) > 1 else \
        "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp"
addr = f"/tmp/xl-tooltip-{os.getpid()}.sock"

app = Stand(binary, addr)
try:
    st = app.state()
    rects = st["rects"]

    print("a coordinator nobody asked for")
    check("a hint declared before the scene still got a coordinator", st["hasSystem"] is True)
    check("with its one hover listener", st.get("hasListener") is True)
    check("and that listener is running", st.get("listenerRunning") is True,
            "a listener added from a descendant's handleEnter never enters")
    check("nothing is hovered before the pointer arrives", st.get("hovered") == "", st.get("hovered"))

    # Short enough to keep the run quick, long enough that "before the dwell" is a real state
    app.s.invoke("tooltip.set-delay", ms=200)

    print("resting on a node")
    x, y = centre(rects["plain"])
    app.move(x, y)
    st = app.state()
    check("the pointer is resolved to the node under it", st["hovered"] == "plain", st["hovered"])
    check("and the dwell is armed for it", st["pending"] == "plain", st["pending"])
    check("with no hint up yet", st["visible"] is False)

    app.wait(2.0, lambda: app.state()["visible"])
    st = app.state()
    check("the hint appears once the pointer has rested", st["visible"] is True)
    check("and it names the node it describes", st["shown"] == "plain", st["shown"])
    check("and says what that node declared", st.get("shownText") == "plain hint",
            st.get("shownText"))

    # WHICH PASS IT IS DRAWN IN, and nothing else in the dump answers it. An in-scene hint is a
    # child of the scene content like any other, so a high z-order only decides where it comes in
    # the ORDINARY passes - it would still be depth-tested and painted against the scene it is
    # supposed to sit on top of. RenderingLevel::Overlay is a pass of its own, drawn last, and
    # SubWindow::openOverlay marks the whole layout with it. The mark is inherited and cannot be
    # escaped from inside, so the tip's root is the one line that carries it.
    tip = [l for l in app.s.call("scene")["result"]["text"].splitlines() if "#aux-tip" in l]
    check("the hint is in the scene as an overlay, not merely at a high z-order",
            len(tip) == 1 and "overlay" in tip[0], tip)

    print("the topmost node wins")
    x, y = centre(rects["over"])
    app.move(x, y)
    st = app.state()
    check("a point on two stacked nodes resolves to the upper one", st["hovered"] == "over",
            st["hovered"])
    x, y = centre(rects["under"], 0.1, 0.15)
    app.move(x, y)
    check("a point on only the lower one resolves to it", app.state()["hovered"] == "under",
            app.state()["hovered"])

    print("the padding belongs to the node")
    pad = rects["padded"]
    app.move(pad["x"] - PAD_HOVER / 2.0, pad["y"] + pad["height"] / 2.0)
    check("a point just outside a padded node still rests on it",
            app.state()["hovered"] == "padded", app.state()["hovered"])
    app.move(pad["x"] - PAD_HOVER * 2.0, pad["y"] + pad["height"] / 2.0)
    check("but not past the padding it declared", app.state()["hovered"] != "padded",
            app.state()["hovered"])

    print("a hint that is turned off")
    x, y = centre(rects["disabled"])
    app.move(x, y)
    check("a disabled hint is not resolved to", app.state()["hovered"] == "",
            app.state()["hovered"])
    app.s.invoke("tooltip.set-enabled", node="disabled", value=True)
    app.move(x + 1.0, y)
    check("and is again the moment it is turned back on", app.state()["hovered"] == "disabled",
            app.state()["hovered"])

    print("leaving")
    x, y = centre(rects["plain"])
    app.move(x, y)
    app.wait(2.0, lambda: app.state()["visible"])
    check("a hint is up to be left", app.state()["visible"] is True)
    app.move(rects["region"]["x"] - 40.0, rects["region"]["y"] + 10.0)
    st = app.state()
    check("stepping off resolves to nothing", st["hovered"] == "", st["hovered"])
    check("and takes the hint down with it", st["visible"] is False)

    print("the node moves, the pointer does not")
    x, y = centre(rects["plain"])
    app.move(x, y)
    app.wait(2.0, lambda: app.state()["visible"])
    check("a hint is up again", app.state()["visible"] is True)
    app.s.invoke("tooltip.move", node="plain", dx=0.0, dy=-260.0)
    app.step(3)
    st = app.state()
    check("a node slid out from under a still pointer stops being hovered", st["hovered"] == "",
            st["hovered"])
    check("and its hint goes down without a single pointer event", st["visible"] is False)
    app.s.invoke("tooltip.move", node="plain", dx=0.0, dy=260.0)
    app.step(2)
    check("sliding it back puts it under the pointer again",
            app.state()["hovered"] == "plain", app.state()["hovered"])

    print("editing and removing a hint")
    app.wait(2.0, lambda: app.state()["visible"])
    app.s.invoke("tooltip.set-text", node="plain", text="edited hint")
    app.step(2)
    check("changing the text of a hint that is up rebuilds it",
            app.state().get("shownText") == "edited hint", app.state().get("shownText"))
    app.s.invoke("tooltip.remove", node="plain")
    app.step(3)
    st = app.state()
    check("removing the hint takes it down", st["visible"] is False)
    check("and the node stops being resolved to", st["hovered"] == "", st["hovered"])
finally:
    app.close()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
