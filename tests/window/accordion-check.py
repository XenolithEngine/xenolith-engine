#!/usr/bin/env python3
"""Drive ui::AccordionView beside a ui::DockSystem over one ui::PanelRegistry, through real input.

The stand (XL_ACCORDION_TEST) checks the API side itself and signs off with

    [W][AccordionTest] SUMMARY: 39 checks, 0 failures

so the first thing this does is give it frames until that line appears - in headless there is no
time except the frames a driver asks for. What the stand cannot check is everything that only
exists once a pointer is involved, and that is the rest of this script:

  * a swipe from the GRIP pulls the panel out; a swipe from the header BODY does not, and a tap
    there toggles the section instead. That split is the whole reason PanelHandle has a
    canBeginDragAt seam, and it is invisible to an API-level test - both paths call the same
    methods in the end;

  * a panel dragged from the accordion into the dock and back arrives as the SAME NODE. The build
    counter is the only thing that says so: a rebuilt panel looks identical on screen and in a
    layout dump, and has silently lost its scroll position, its selection and its half-typed text.

The stand ends phase 6 by standing a FRESH dock up over the same registry, so both containers are
live by the time this starts driving - which is also, in passing, a check that a registry can take
a second host after its first one is gone.

    tests/window/accordion-check.py [path-to-testapp]

Exits non-zero on any failure.
"""
import json, os, socket, struct, subprocess, sys, time

ENV_NAME = "XL_ACCORDION_TEST"
TAG = "AccordionTest"
TIMEOUT = 30.0

# The dock occupies the left 420pt of the stand's root and the accordion the 260pt beside it. These
# are the two places a drop has to land; they are read off the stand's own geometry rather than
# guessed, except for the dock body, which has no probe of its own here.
DOCK_BODY = (250.0, 400.0)


class Session:
    def __init__(self, path, timeout=25.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
        # the greeting is a LINE, before any frame - read it to the newline or the first length
        # word is consumed as part of it
        line = b""
        while not line.endswith(b"\n"):
            line += self.s.recv(1)
        assert line.startswith(b"# xenolith/1 ok"), line
        self.serial = 0

    def call(self, cmd, **kw):
        self.serial += 1
        req = {"serial": self.serial, "cmd": cmd}
        req.update(kw)
        payload = json.dumps(req).encode()
        self.s.sendall(struct.pack("<I", len(payload)) + payload)
        size = struct.unpack("<I", self._read(4))[0]
        return json.loads(self._read(size).decode())

    def _read(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.s.recv(n - len(buf))
            if not chunk:
                raise EOFError
            buf += chunk
        return buf

    # --- the stand's own vocabulary ---------------------------------------

    def invoke(self, name, **args):
        return self.call("invoke", name=name, args=args)["result"]

    def frames(self, count=2):
        self.call("frame", count=count)

    def sections(self):
        return [(s["id"], s["expanded"]) for s in self.invoke("accordion.sections")]

    def host(self, panel):
        return self.invoke("accordion.host", panel=panel).get("host")

    def builds(self):
        return self.invoke("accordion.builds")

    def drag(self, start, end, steps=12):
        """A press, enough travel to pass the 8pt threshold, and a release.

        Step by step rather than in one jump: the threshold, the pointer capture and the drop
        resolution each happen on a different event, and a single Move would skip past the first
        two - which is exactly the part an API-level test cannot reach."""
        x0, y0 = start
        x1, y1 = end
        self.call("input", events=[_ev("Begin", x0, y0)])
        self.frames(1)
        for i in range(1, steps + 1):
            self.call("input", events=[_ev("Move", x0 + (x1 - x0) * i / steps,
                    y0 + (y1 - y0) * i / steps)])
            self.frames(1)
        self.call("input", events=[_ev("End", x1, y1)])
        self.frames(3)

    def tap(self, point):
        self.call("input", events=[_ev("Begin", point[0], point[1])])
        self.frames(1)
        self.call("input", events=[_ev("End", point[0], point[1])])
        self.frames(2)

    def close(self):
        self.s.close()


def _ev(name, x, y):
    return {"event": name, "x": x, "y": y, "button": "MouseLeft", "id": 1}


def logs(path):
    """The one-shot text protocol: send a word, read until EOF."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(path)
    s.sendall(b"logs\n")
    out = b""
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        out += chunk
    s.close()
    return out.decode("utf-8", "replace")


def start_app(binary, addr):
    env = dict(os.environ)
    env[ENV_NAME] = "1"
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


class Checks:
    def __init__(self):
        self.failures = []
        self.count = 0

    def expect(self, cond, what):
        self.count += 1
        if not cond:
            self.failures.append(what)


def wait_for_summary(s, addr):
    """Frames until the stand signs off. Waiting a fixed number instead would bake this machine's
    frame pacing into the test."""
    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        s.frames(2)
        time.sleep(0.05)
        for line in logs(addr).splitlines():
            if "SUMMARY" in line and TAG in line:
                return line.strip()
    return None


def drive(s, c):
    # --- the grip/body split ------------------------------------------------
    #
    # A swipe on the header BODY must do nothing at all: it is not a tap (so it does not toggle)
    # and it does not start on the grip (so it does not drag). Both halves matter - a widget that
    # dragged from anywhere on the header would come loose under every slightly imprecise click.
    target = s.sections()[1][0]
    probe = s.invoke("accordion.probe", panel=target)
    before = dict(s.sections())
    s.drag((probe["headerX"], probe["headerY"]), (probe["headerX"] - 180.0, probe["headerY"]))
    c.expect(s.host(target) == "accordion", "a swipe on the header body moved the panel")
    c.expect(dict(s.sections()).get(target) == before.get(target),
            "a swipe on the header body toggled the section")

    # A TAP there does toggle.
    s.tap((probe["headerX"], probe["headerY"]))
    c.expect(dict(s.sections()).get(target) != before.get(target),
            "a tap on the header body did not toggle the section")

    # --- a reorder by the grip ---------------------------------------------
    order = [x[0] for x in s.sections()]
    last, first = order[-1], order[0]
    grip = s.invoke("accordion.probe", panel=last)
    head = s.invoke("accordion.probe", panel=first)
    s.drag((grip["gripX"], grip["gripY"]), (head["headerX"], head["headerY"] + 10.0))
    c.expect([x[0] for x in s.sections()][0] == last,
            "a drag from the grip did not carry the section to the top")

    # --- across to the dock and back ---------------------------------------
    builds_before = s.builds()
    moving = [x[0] for x in s.sections()][0]

    grip = s.invoke("accordion.probe", panel=moving)
    s.drag((grip["gripX"], grip["gripY"]), DOCK_BODY)
    c.expect(s.host(moving) == "dock", "the panel did not reach the dock")
    c.expect(moving not in [x[0] for x in s.sections()],
            "the accordion kept a section for a panel the dock took")

    # Back again. The dock's tab for it is in the strip along the top of its frame; the stand's
    # single frame spans the left half of the root, so the tabs start just inside its left edge.
    anchor = s.invoke("accordion.probe", panel=[x[0] for x in s.sections()][0])
    s.drag((123.0, 651.0), (anchor["headerX"], anchor["headerY"] - 40.0))
    c.expect(s.host(moving) == "accordion", "the panel did not come back to the accordion")

    # THE POINT OF ALL OF IT: not one rebuild anywhere in the round trip.
    after = s.builds()
    c.expect(after == builds_before,
            "a panel was rebuilt by the round trip: %s -> %s" % (builds_before, after))


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

addr = "/tmp/xl-accordion-check.sock"
proc = start_app(binary, addr)
s = Session(addr)
c = Checks()
summary = None
try:
    summary = wait_for_summary(s, addr)
    if summary is not None:
        drive(s, c)
finally:
    s.close()
    proc.kill()
    try:
        os.unlink(addr)
    except OSError:
        pass

if summary is None:
    print("  FAIL accordion     never reached its summary")
    sys.exit(1)

stand_ok = " 0 failures" in summary
print("  %s accordion     stand: %s" % ("ok  " if stand_ok else "FAIL",
        summary.split("SUMMARY: ")[-1]))
print("  %s accordion     input: %d checks, %d failures" % (
        "ok  " if not c.failures else "FAIL", c.count, len(c.failures)))
for f in c.failures:
    print("       " + f)

sys.exit(0 if stand_ok and not c.failures else 1)
