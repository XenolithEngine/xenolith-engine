#!/usr/bin/env python3
"""Verify that the xcb backend reports left/right modifier sides.

The one check here that hotkey-check.py cannot make. That one injects modifiers as a raw bitmask
through the inspector, which proves the matching but never touches a backend; this drives a REAL
X11 window with XTEST, so the whole path is exercised: physical keycode -> XKB key name ->
InputKeyCode -> InputModifier side bit -> the sided bucket in HotkeyRegistry.

    tests/window/xcb-side-check.py

NOT part of the headless suite, and cannot be: it needs a live X11 server (DISPLAY :1 here) and
python-xlib, and it takes over the keyboard focus for a moment. Run it by hand after touching
XcbWindow's key handling or getKeySideModifier.

The Wayland twin of this would need a nested compositor to be safe - injecting through /dev/uinput
goes to whatever the real session has focused.
"""
import json, os, socket, struct, subprocess, sys, time

from Xlib import X, XK, display
from Xlib.ext import xtest

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-hotkey-x11.sock")
DISPLAY = os.environ.get("XL_TEST_DISPLAY", ":1")
BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")


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
env["XL_HOTKEY_TEST"] = "1"
env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
env["DISPLAY"] = DISPLAY
env.pop("WAYLAND_DISPLAY", None)   # force the xcb backend
env["SP_SESSION_TYPE"] = "x11"
try:
    os.unlink(ADDR)
except OSError:
    pass

app = subprocess.Popen([sys.argv[1] if len(sys.argv) > 1 else BINARY,
                        "--width", "800", "--height", "600"],
                       env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

s = None
for _ in range(60):
    try:
        s = Session()
        break
    except OSError:
        time.sleep(0.5)
if not s:
    app.kill()
    raise SystemExit("app never came up")

d = display.Display(DISPLAY)
root = d.screen().root

FAIL = []
CHECKS = 0


def expect(cond, what, extra=""):
    global CHECKS
    CHECKS += 1
    if not cond:
        FAIL.append(what)
        print(f"  FAIL  {what} {extra}")
    else:
        print(f"  ok    {what}")


def find_window():
    # the newest window whose WM_CLASS names the test app
    def walk(w, out):
        try:
            cls = w.get_wm_class()
        except Exception:
            cls = None
        if cls and any("testapp" in c for c in cls):
            out.append(w)
        try:
            for c in w.query_tree().children:
                walk(c, out)
        except Exception:
            pass
    out = []
    walk(root, out)
    return out[-1] if out else None


def kc(name):
    return d.keysym_to_keycode(XK.string_to_keysym(name))


def chord(modname, keyname):
    m, k = kc(modname), kc(keyname)
    xtest.fake_input(d, X.KeyPress, m)
    d.sync()
    time.sleep(0.05)
    xtest.fake_input(d, X.KeyPress, k)
    xtest.fake_input(d, X.KeyRelease, k)
    d.sync()
    time.sleep(0.05)
    xtest.fake_input(d, X.KeyRelease, m)
    d.sync()
    time.sleep(0.25)


def step(n=3):
    s.ok("frame", count=n)
    time.sleep(0.2)


try:
    time.sleep(2.0)
    step(5)

    win = find_window()
    expect(win is not None, "found the app window")

    # The WM may hand focus elsewhere; XTEST goes to whatever is focused, so this has to be
    # confirmed rather than assumed - an unfocused window makes the whole check vacuous
    def focused_on_app():
        f = d.get_input_focus().focus
        for _ in range(8):
            if not hasattr(f, "id"):
                return False
            if win and f.id == win.id:
                return True
            try:
                f = f.query_tree().parent
            except Exception:
                return False
        return False

    ok = False
    for _ in range(10):
        win.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        time.sleep(0.4)
        if focused_on_app():
            ok = True
            break
    expect(ok, "the app window holds the X input focus")
    step(3)

    for name in ("declining", "global", "focused", "sibling", "exclusive"):
        s.invoke("hotkey.set-consume", subscriber=name, value=True, settle=0.0)

    print("== left Ctrl ==")
    s.invoke("hotkey.clear", settle=0.0)
    chord("Control_L", "k")
    step(3)
    st = s.invoke("hotkey.log", settle=0.0)
    got = [e["hotkey"] for e in (st["log"] or [])]
    expect(got[:1] == ["org.stappler.test.hotkey.sided"],
           "a real left Ctrl reported CtrlL and fired the sided hotkey", repr(got))

    print("== right Ctrl ==")
    s.invoke("hotkey.clear", settle=0.0)
    chord("Control_R", "k")
    step(3)
    st = s.invoke("hotkey.log", settle=0.0)
    got = [e["hotkey"] for e in (st["log"] or [])]
    expect(got[:1] == ["org.stappler.test.hotkey.action"],
           "a real right Ctrl fell through to the base hotkey", repr(got))

finally:
    print(f"\nSUMMARY: {CHECKS} checks, {len(FAIL)} failures")
    try:
        s.ok("quit")
    except Exception:
        app.kill()
    try:
        app.wait(timeout=10)
    except Exception:
        app.kill()

sys.exit(1 if FAIL else 0)
