#!/usr/bin/env python3
"""Drive the global hotkey stand (XL_HOTKEY_TEST) over the inspector socket and assert.

Everything here runs headless: no display, no compositor, no mouse.

Two facts shape the script. Hotkeys are delivered on the KEY PRESS, ahead of the ordinary key
route, so every check sends a press+release pair and reads the delivery log back. And a synthetic
key event must carry `keychar` with `native=True`: a keychar-less event never meets the text-input
processor, which is exactly the false positive that once hid the Ctrl-chord bug (see
text-input-check.py, section 20).

    tests/window/hotkey-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-hotkey.sock")


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


# mods: 1 = Shift, 4 = Ctrl (see InputModifier)
CTRL = 4
CTRL_LEFT = 1 << 16   # CtrlL
CTRL_RIGHT = 1 << 17  # CtrlR
ALT = 8


def key(code, char=None, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "modifiers": mods}
    if char is not None:
        ev["keychar"] = char
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def start_app(binary):
    env = dict(os.environ)
    env["XL_HOTKEY_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    return subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


DEFAULT_BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

FAIL = []
CHECKS = 0


def expect(cond, what, extra=""):
    global CHECKS
    CHECKS += 1
    if not cond:
        FAIL.append(f"{what} {extra}")
        print(f"  FAIL  {what} {extra}")
    else:
        print(f"  ok    {what}")


def connect():
    for _ in range(40):
        try:
            return Session()
        except OSError:
            time.sleep(0.5)
    raise SystemExit("app never came up")


APP = start_app(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY)
s = connect()


def step(n=1):
    s.ok("frame", count=n)
    time.sleep(0.15)


def clear():
    s.invoke("hotkey.clear", settle=0.0)


def log():
    return s.invoke("hotkey.log", settle=0.0)


def send(code, char=None, mods=0):
    s.ok("input", events=key(code, char, mods), native=True)
    step(2)


# `Session.invoke` takes the command name positionally, so a command whose own argument is called
# `name` has to go through the raw form
def rebind(name, combo):
    return s.ok("invoke", name="hotkey.rebind", args={"name": name, "combo": combo})


def order(entries):
    return [e["subscriber"] for e in entries]


def names(entries):
    return [e["hotkey"] for e in entries]


try:
    # A focus request is applied by the group on the NEXT commit, and with two candidates in the
    # ring the group's own fallback would otherwise decide which of them starts out focused
    step(3)
    s.invoke("hotkey.focus", subscriber="focused", settle=0.0)
    step(3)

    print("== 1. the focused subscriber is offered the key first ==")
    clear()
    send("K", "k", CTRL)
    st = log()
    # `declining` sits in the pre-scene band and would otherwise be first; the focused subscriber
    # pre-empts it, which is the whole point of pass A
    expect(order(st["log"])[:1] == ["focused"], "the focused subscriber was called first",
           repr(order(st["log"])))
    expect(len(st["log"]) == 1, "and it consumed the key, so the walk stopped",
           repr(order(st["log"])))
    expect(st["fallthrough"] == 0, "the key never reached the ordinary recognizer")

    print("== 2. a decline falls through to the next subscriber ==")
    clear()
    s.invoke("hotkey.set-consume", subscriber="focused", value=False, settle=0.0)
    send("K", "k", CTRL)
    st = log()
    expect(order(st["log"]) == ["focused", "declining", "global"],
           "everyone up to the first acceptor was called, in walk order", repr(order(st["log"])))
    expect(st["fallthrough"] == 0, "and `global` consumed it, so no fallthrough")

    print("== 3. the first acceptor stops the walk ==")
    clear()
    s.invoke("hotkey.set-consume", subscriber="declining", value=True, settle=0.0)
    send("K", "k", CTRL)
    st = log()
    expect(order(st["log"]) == ["focused", "declining"],
           "`global` was never called after `declining` accepted", repr(order(st["log"])))

    print("== 4. nobody handled it -> the ordinary key route still runs ==")
    clear()
    s.invoke("hotkey.set-consume", subscriber="declining", value=False, settle=0.0)
    s.invoke("hotkey.set-consume", subscriber="global", value=False, settle=0.0)
    send("K", "k", CTRL)
    st = log()
    expect(len(st["log"]) == 3, "all three were offered the key", repr(order(st["log"])))
    expect(st["fallthrough"] == 1, "and the key fell through to the recognizer",
           repr(st["fallthrough"]))

    print("== 5. a combination bound to nothing is untouched ==")
    clear()
    send("F8")
    st = log()
    expect(st["log"] == [], "no hotkey fired")
    expect(st["fallthrough"] == 1, "the key reached the recognizer unchanged")

    print("== 6. FocusedOnly is silent when the subscriber is not focused ==")
    # `sibling` shares the focus group with `focused`, so moving focus leaves `focused` in the walk
    # but no longer entitled to keys - which is what FocusedOnly is actually about
    s.invoke("hotkey.set-consume", subscriber="global", value=True, settle=0.0)
    s.invoke("hotkey.focus", subscriber="sibling", settle=0.0)
    step(3)
    clear()
    send("K", "k", CTRL)
    st = log()
    expect(order(st["log"]) == ["sibling"], "the newly focused subscriber took the key",
           repr(order(st["log"])))
    expect("focused" not in order(st["log"]),
           "and the unfocused one was never offered it", repr(order(st["log"])))

    s.invoke("hotkey.focus", subscriber="focused", settle=0.0)
    step(3)

    print("== 7. modifier normalization ==")
    clear()
    s.invoke("hotkey.set-consume", subscriber="focused", value=True, settle=0.0)
    # CtrlL alone must fold into Ctrl and match Ctrl+K
    send("K", "k", CTRL_LEFT)
    st = log()
    expect(len(st["log"]) == 1, "the left Ctrl matched the same combination as plain Ctrl",
           repr(order(st["log"])))

    clear()
    # NumLock (1<<4) and CapsLock (1<<1) must not stop a match
    send("K", "k", CTRL | (1 << 4) | (1 << 1))
    st = log()
    expect(len(st["log"]) == 1, "the lock states did not disturb the match",
           repr(order(st["log"])))

    clear()
    # ... and a missing modifier must NOT match
    send("K", "k", 0)
    st = log()
    expect(st["log"] == [], "plain K did not fire the Ctrl+K hotkey", repr(order(st["log"])))
    expect(st["fallthrough"] == 1, "it went to the recognizer instead")

    print("== 8. an exclusive focus group silences the subscribers outside it ==")
    clear()
    s.invoke("hotkey.set-exclusive", value=True, settle=0.0)
    step(3)
    send("F7")
    st = log()
    expect(order(st["log"]) == ["exclusive"],
           "only the subscriber inside the exclusive group was called", repr(order(st["log"])))

    print("== 9. BypassExclusive punches through ==")
    # BypassExclusive means "still offered the key", not "offered it first": the subscriber inside
    # the group is earlier in the walk, so it has to decline for the bypass to be observable
    clear()
    s.invoke("hotkey.set-consume", subscriber="exclusive", value=False, settle=0.0)
    send("Q", "q", CTRL)
    st = log()
    expect(order(st["log"]) == ["exclusive", "global-bypass"],
           "the bypassing subscription fired despite the exclusive group", repr(order(st["log"])))

    # ... while a subscription without the flag stays silenced
    clear()
    send("F7")
    st = log()
    expect(order(st["log"]) == ["exclusive"],
           "an ordinary outside subscription is still silenced", repr(order(st["log"])))
    s.invoke("hotkey.set-consume", subscriber="exclusive", value=True, settle=0.0)

    s.invoke("hotkey.set-exclusive", value=False, settle=0.0)
    step(3)

    print("== 10. a sided binding is matched only from that side ==")
    # CtrlL+K and Ctrl+K are two hotkeys on the same key. An event from the LEFT Ctrl matches both,
    # and the sided one is reported first; the right Ctrl matches only the base one.
    clear()
    send("K", "k", CTRL | CTRL_LEFT)
    st = log()
    expect(names(st["log"])[:1] == ["org.stappler.test.hotkey.sided"],
           "the left Ctrl fired the sided hotkey", repr(names(st["log"])))

    clear()
    send("K", "k", CTRL | CTRL_RIGHT)
    st = log()
    expect(names(st["log"])[:1] == ["org.stappler.test.hotkey.action"],
           "the right Ctrl fell through to the base hotkey", repr(names(st["log"])))

    clear()
    send("K", "k", CTRL)
    st = log()
    expect(names(st["log"])[:1] == ["org.stappler.test.hotkey.action"],
           "a Ctrl with no side reported still matches the base hotkey", repr(names(st["log"])))

    print("== 11. a reserved combination survives a focused text field ==")
    # Alt+F carries a keychar, so without TextInputProcessor::setReservedKeyFilter the text-input
    # processor would type "f" into the field and rewrite the key to KeyCanceled
    s.invoke("hotkey.focus-field", value=True, settle=0.0)
    step(4)
    field = s.invoke("hotkey.field-state", settle=0.0)
    expect(field["focused"], "the field holds the IME", repr(field))

    clear()
    send("F", "f", ALT)
    st = log()
    expect(names(st["log"])[:1] == ["org.stappler.test.hotkey.reserved"],
           "the reserved combination reached the scene", repr(names(st["log"])))
    field = s.invoke("hotkey.field-state", settle=0.0)
    expect(field["text"] == "", "and nothing was typed into the field", repr(field["text"]))

    # ... while an ordinary character still goes to the field
    send("G", "g")
    field = s.invoke("hotkey.field-state", settle=0.0)
    expect(field["text"] == "g", "an unreserved key is still text", repr(field["text"]))

    # Escape is a registered hotkey too, but it did NOT ask to be reserved - the IME keeps it,
    # which is what releases input. Reserving every registered combination would break exactly this
    s.ok("input", events=key("ESCAPE", "\x1b"), native=True)
    step(3)
    field = s.invoke("hotkey.field-state", settle=0.0)
    expect(not field["focused"], "an unreserved hotkey still reaches the IME", repr(field))


    print("== 12. the registry is enumerable and carries the engine's own hotkeys ==")
    listing = s.invoke("hotkey.list", settle=0.0)["hotkeys"]
    by_name = {it["name"]: it for it in listing}
    expect("org.stappler.test.hotkey.action" in by_name, "the stand's hotkey is listed")
    expect(by_name.get("org.stappler.test.hotkey.action", {}).get("combo") == "Ctrl+K",
           "with the combination it was registered under",
           repr(by_name.get("org.stappler.test.hotkey.action")))
    expect("org.stappler.xenolith.focus.next" in by_name,
           "and so are the engine's own, under their reverse-DNS names")
    expect(by_name.get("org.stappler.xenolith.form.reset", {}).get("combo") == "ESCAPE",
           "Escape is registered twice over, once per meaning",
           repr(by_name.get("org.stappler.xenolith.form.reset")))

    print("== 13. rebinding moves the hotkey and frees the old combination ==")
    clear()
    rebind("org.stappler.test.hotkey.action", "F8")
    send("K", "k", CTRL)
    st = log()
    expect(st["log"] == [], "the old combination no longer fires", repr(order(st["log"])))
    expect(st["fallthrough"] == 1, "and now falls through as a plain key")

    clear()
    send("F8")
    st = log()
    expect(len(st["log"]) == 1, "the new combination fires instead", repr(order(st["log"])))
    expect(names(st["log"])[:1] == ["org.stappler.test.hotkey.action"],
           "and it is the same hotkey, under the same name", repr(names(st["log"])))

    # Put it back, so a re-run from a live app starts from the same place
    rebind("org.stappler.test.hotkey.action", "Ctrl+K")

finally:
    print(f"\nSUMMARY: {CHECKS} checks, {len(FAIL)} failures")
    try:
        s.ok("quit")
    except Exception:
        APP.kill()
    APP.wait(timeout=10)

sys.exit(1 if FAIL else 0)
