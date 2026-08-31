#!/usr/bin/env python3
"""Drive the ui::Panel styling stand (XL_PANEL_TEST) over the inspector socket.

The stand does its own asserting: it is the only side that can read a `PanelStyleComponent`, and
what it asserts is a sequence in time (style, restyle, un-style) rather than a state. So this script
is the part that could not live in C++ - it starts the app, holds the render loop open, pumps the
frames the stand's timeline needs, and turns the summary it logs into an exit status. Nothing here
re-checks a colour; everything here checks that the stand RAN and said zero failures.

What the stand covers, and why it is worth a script of its own:

  * the per-type appliers reach panel / checkbox / badge at all, and a plain Layer under the same
    sheet is the control that tells a broken applier from a sheet that matched nothing;
  * a class flip (`checkbox:checked`) restyles;
  * CmdReset undoes a rule that stopped matching - the button left with NO style component;
  * and the other half of the same command: a widget that painted ITSELF keeps that paint through
    the reset, and only loses it while a rule actually declares the attribute. Panel::setPathColor
    holds a layer under the stylesheet for exactly this reason; before it, every panel painted from
    code - scroll indicator, colour swatch, menu separator, table cell - turned white on the first
    resolver pass that touched it.

    tests/window/panel-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It prints "N checks, M
failures"; exit status is the result.
"""
import json, os, re, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-panel-check.sock")


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

    def close(self):
        self.s.close()


def start_app(binary):
    env = dict(os.environ)
    env["XL_PANEL_TEST"] = "1"
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

# The stand's timeline is three one-second delays, and an action only steps when a frame runs. In a
# headless process nothing produces frames on its own, so the loop below is what makes time pass at
# all - `render` makes each of those frames actually redraw, which is what the eye needs when this
# is run against a real window instead.
r = s.ok("render")
check("`render` holds the loop open", r.get("running") is True, r)

lines = []
deadline = time.time() + 30.0
summary = None
while time.time() < deadline:
    s.call("frame", count=4)
    time.sleep(0.05)
    lines = s.ok("logs").get("lines", [])
    summary = next((ln for ln in lines if "SUMMARY" in ln and "PanelTest" in ln), None)
    if summary:
        break

check("the stand reached its last phase", summary is not None,
        "\n".join(lines[-6:]) if lines else "no log at all")

if summary:
    m = re.search(r"SUMMARY: (\d+) checks, (\d+) failures", summary)
    check("the summary line is readable", m is not None, summary)
    if m:
        ran, failed = int(m.group(1)), int(m.group(2))
        # A number, not just "more than zero": the phases are fixed, so a stand that silently
        # stopped asserting would still print a clean summary.
        check("every assertion of the stand ran", ran == 16, f"{ran} checks")
        check("the stand reported no failure", failed == 0,
                "\n".join(ln for ln in lines if "PanelTest" in ln and "error" in ln.lower()))

# The stand logs each failure separately, so name the two that this script exists for: an assertion
# that stops running is as bad as one that fails, and the message is the only thing that says which.
errors = [ln for ln in lines if "PanelTest" in ln and ("reset:" in ln or "initial:" in ln)]
check("no phase reported a paint that a style pass took away", not errors, "\n".join(errors[:4]))

s.ok("render", stop=True)
s.close()
proc.kill()

print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
