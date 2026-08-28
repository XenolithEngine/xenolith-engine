#!/usr/bin/env python3
"""Drive the xenolith::ClipboardSession stand (XL_CLIPBOARD_TEST) over the inspector socket.

Everything here runs headless: no display, no compositor, no mouse. That is not a convenience -
it is the only place a clipboard round trip is deterministic. On a real window system taking the
selection needs an input serial from a focused window, so a background test window silently fails
to become the owner and a read comes back with whatever another application put there.

The claims, in the order they are checked:

  * one payload carries SEVERAL representations, and the caller's preference list - not the offer's
    order - decides which one comes back;
  * matching is by PREFIX, so "text/plain" also accepts "text/plain;charset=utf-8";
  * a preference list matching nothing is still answered EXACTLY ONCE. This is the whole reason the
    seam exists: wayland answers a type it did not offer with silence, and the base controller
    answers twice. Both are counted here rather than described;
  * a cancelled read is not answered at all, and neither is one whose field lost focus - which
    before E7 was a comment claiming something no code did;
  * an offer with no representations is REFUSED, because on Android an empty type list means
    "clear the clipboard";
  * what a ui::TextInput copies, a ui::TextView pastes: copy/cut/paste live in the base now. But a
    masked field still refuses to copy, and a text view still never masks.

    tests/window/clipboard-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-clipboard-check.sock")

# What the stand and this script agree on, spelled here on purpose: a check that reads its
# expectations out of the thing it is checking cannot fail.
OWN = "application/x-xenolith-test+json"
OWN_TEXT = '{"kind":"test"}'
PLAIN = "text/plain"
PLAIN_TEXT = "hello"


class Session:
    def __init__(self, path=ADDR, timeout=10.0):
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


def start_app(binary):
    env = dict(os.environ)
    env["XL_CLIPBOARD_TEST"] = "1"
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


DEFAULT_BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY)
s = Session()


def step(n=2):
    s.ok("frame", count=n)
    time.sleep(0.15)


def state():
    return s.invoke("clipboard.state", settle=0.0)


def write_both():
    return s.invoke("clipboard.write", settle=0.0, label="test payload",
                    reps=[{"type": OWN, "text": OWN_TEXT}, {"type": PLAIN, "text": PLAIN_TEXT}])


def read(prefer):
    """Start a read and return the answer once it has landed.

    No retry loop, and that is itself a claim: the write and the read each cross to the context
    thread and back, so a command that starts a read cannot see its own answer. Stepping frames is
    what lets the answer arrive. Where form-check.py has to retry the PASTE (nothing re-reads a
    clipboard), this stand reads back a stored answer, so the retry disappears.
    """
    before = state()["deliveries"]
    r = s.invoke("clipboard.read", settle=0.0, prefer=prefer)
    for _ in range(40):
        st = state()
        if st["deliveries"] > before:
            return r, st["lastRead"]
        step(1)
    return r, state()["lastRead"]


try:
    print("== the transport says what it is ==")
    st = state()
    check("headless reports a clipboard", st["available"] is True, st)
    check("nothing is in flight to begin with", st["pending"] is False, st)

    print("== one payload, two representations ==")
    w = write_both()
    check("the write was accepted", w["ok"] is True, w)

    s.invoke("clipboard.reset-counters", settle=0.0)
    r, last = read([OWN])
    check("a read starts", r["ok"] is True, r)
    check("the specific type comes back", last["type"] == OWN, last)
    check("and carries ITS bytes", last["text"] == OWN_TEXT, last)
    check("answered exactly once", state()["deliveries"] == 1, state())

    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([PLAIN])
    check("the text type comes back", last["type"] == PLAIN, last)
    # Different bytes for a different type is what proves the encoder is asked PER TYPE rather than
    # serving one blob under several names
    check("and carries DIFFERENT bytes", last["text"] == PLAIN_TEXT, last)
    check("answered exactly once", state()["deliveries"] == 1, state())

    print("== the caller's preference decides, not the offer's order ==")
    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([OWN, PLAIN])
    check("the first preference wins", last["type"] == OWN, last)

    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([PLAIN, OWN])
    check("reversing the list reverses the answer", last["type"] == PLAIN, last)

    print("== matching is by prefix ==")
    s.invoke("clipboard.write", settle=0.0, label="charset",
             reps=[{"type": "text/plain;charset=utf-8", "text": "wide"}])
    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([PLAIN])
    check("a bare text/plain accepts a charset suffix", last["type"] == "text/plain;charset=utf-8",
          last)
    check("and brings its bytes", last["text"] == "wide", last)

    print("== a list that matches nothing is still ANSWERED ==")
    write_both()
    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read(["image/png"])
    # The point of the whole seam: silence here is what wayland does natively, and two answers is
    # what the base controller does
    check("exactly one answer, not zero and not two", state()["deliveries"] == 1, state())
    check("and it is a refusal", last["ok"] is False, last)
    check("with no type taken", last["type"] == "", last)
    check("and it names what WAS there", OWN in last["available"], last)

    print("== an empty offer is refused, not sent ==")
    r = s.invoke("clipboard.write-empty", settle=0.0)
    check("writing nothing is refused", r["ok"] is False, r)
    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([OWN])
    check("and the previous payload is still there", last["text"] == OWN_TEXT, last)

    print("== a cancelled read is never answered ==")
    write_both()
    s.invoke("clipboard.reset-counters", settle=0.0)
    r = s.invoke("clipboard.read-then-cancel", settle=0.0, prefer=[OWN])
    step(6)
    # Asserted rather than assumed: a serial of 0 would mean the read never started, and then
    # "the callback never ran" would be true for the wrong reason
    check("the read really started", r["serial"] != 0, r)
    check("nothing is left in flight", r["pending"] is False, r)
    check("and the callback never ran", state()["deliveries"] == 0, state())

    print("== what one widget copies, the other pastes ==")
    s.invoke("clipboard.set-text", settle=0.0, widget="field", value="from the field")
    s.invoke("clipboard.set-text", settle=0.0, widget="view", value="")
    step()
    r = s.invoke("clipboard.widget-copy", settle=0.0, widget="field")
    check("a field copies", r["ok"] is True, r)
    step(2)
    s.invoke("clipboard.widget-paste", settle=0.0, widget="view")
    step(6)
    check("and a text view pastes it",
          state()["widgets"]["view"]["text"] == "from the field",
          state()["widgets"]["view"])

    s.invoke("clipboard.set-text", settle=0.0, widget="field", value="")
    step()
    r = s.invoke("clipboard.widget-copy", settle=0.0, widget="view")
    check("a text view copies", r["ok"] is True, r)
    step(2)
    s.invoke("clipboard.widget-paste", settle=0.0, widget="field")
    step(6)
    check("and a field pastes it",
          state()["widgets"]["field"]["text"] == "from the field",
          state()["widgets"]["field"])

    print("== a paste whose field lost focus is not applied ==")
    s.invoke("clipboard.set-text", settle=0.0, widget="field", value="before")
    s.invoke("clipboard.write", settle=0.0, label="after",
             reps=[{"type": PLAIN, "text": "after"}])
    step(2)
    r = s.invoke("clipboard.widget-paste-then-blur", settle=0.0, widget="field")
    # Same guard: a paste that never started would make the next check pass vacuously
    check("the paste really started", r["ok"] is True, r)
    step(8)
    check("the field kept what it had", state()["widgets"]["field"]["text"] == "before",
          state()["widgets"]["field"])
    # Not a bare inequality: the same paste WITHOUT the blur has to work, or the check above would
    # pass on a paste that is simply broken
    s.invoke("clipboard.widget-paste", settle=0.0, widget="field")
    step(8)
    check("while the same paste without a blur lands",
          state()["widgets"]["field"]["text"].endswith("after"), state()["widgets"]["field"])

    print("== policy stayed with the widget ==")
    s.invoke("clipboard.write", settle=0.0, label="sentinel",
             reps=[{"type": PLAIN, "text": "sentinel"}])
    s.invoke("clipboard.set-text", settle=0.0, widget="secret", value="s3cret")
    s.invoke("clipboard.set-password", settle=0.0, widget="secret", value=True)
    step(2)
    r = s.invoke("clipboard.widget-copy", settle=0.0, widget="secret")
    check("a masked field refuses to copy", r["ok"] is False, r)
    s.invoke("clipboard.reset-counters", settle=0.0)
    _, last = read([PLAIN])
    check("and the clipboard still holds what it held", last["text"] == "sentinel", last)

    # The asymmetry that survived the merge, asserted so a later tidy-up cannot quietly unify them
    s.invoke("clipboard.set-password", settle=0.0, widget="view", value=True)
    s.invoke("clipboard.set-text", settle=0.0, widget="view", value="not masked")
    step(2)
    r = s.invoke("clipboard.widget-copy", settle=0.0, widget="view")
    check("a text view copies even in password mode - it never masks", r["ok"] is True, r)

    print("== probe ==")
    write_both()
    s.invoke("clipboard.reset-counters", settle=0.0)
    s.invoke("clipboard.probe", settle=0.0)
    for _ in range(40):
        if state()["deliveries"] > 0:
            break
        step(1)
    last = state()["lastRead"]
    check("the probe answered exactly once", state()["deliveries"] == 1, state())
    check("and lists both representations",
          OWN in last["available"] and PLAIN in last["available"], last)

finally:
    s.close()
    proc.kill()

print()
print(f"{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
