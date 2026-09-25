#!/usr/bin/env python3
"""Partial redraw on the headless pseudo-swapchain, checked by the picture it leaves.

The damage stand (XL_DAMAGE_TEST) moves a red square in discrete jumps beside a static grey one.
With partial redraw only the damaged area of an image is drawn again - on top of whatever the image
already held from the frame that last used it. Get the image's history wrong (the per-slot damage
snapshot keyed by the wrong index, or the image not in the layout a LOAD expects) and the old
position survives as a second red square: a trail. So the check is simply that every screenshot
holds exactly ONE square's worth of red.

The flat queue is the one with partial redraw, and the frames must really take that path: the
backend's damage log has to report partial redraws, or the picture proves nothing.

    tests/window/damage-check.py [--gapi vulkan|soft] [path-to-testapp]

Without --gapi both backends run (testapp must be built with SOFT=1 for soft).

Prints "N checks, M failures"; exit status is the result.
"""
import base64, io, json, os, socket, struct, subprocess, sys, time

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))

WIDTH, HEIGHT = 1024, 768
SQUARE = 200  # DamageLayout: both squares are 200 x 200 points
STEP_DELAY = 0.5  # DamageLayout::StepDelay
STEPS = 3  # DamageLayout::MoveSteps

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

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass


def start_app(binary, sock, log, gapi):
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    env["XL_DAMAGE_TEST"] = "1"
    env["XL_FLAT_QUEUE"] = "1"
    env["XL_HIDE_FPS"] = "1"
    env["XL_VK_DAMAGE_LOG"] = "1"
    env["XL_SOFT_DAMAGE_LOG"] = "1"
    try:
        os.unlink(sock)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height", str(HEIGHT),
            "--gapi", gapi], env=env, cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(log, "w"), stderr=subprocess.STDOUT)
    for _ in range(600):
        if proc.poll() is not None:
            raise SystemExit(f"testapp exited early with {proc.returncode}, see {log}")
        if os.path.exists(sock):
            try:
                return proc, Session(sock)
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("testapp did not come up")


def shot_bytes(s, window=None):
    """The current picture as raw RGB bytes."""
    shot = (s.ok("screenshot", window=window) if window else s.ok("screenshot")) or {}
    data = shot.get("data") or ""
    if isinstance(data, str) and data.startswith("BASE64:"):
        raw = data[len("BASE64:"):]
        data = base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4))
    return Image.open(io.BytesIO(data)).convert("RGB").tobytes()


def red_area(s, window=None):
    """Pixels of the moving square's red in the current screenshot."""
    shot = (s.ok("screenshot", window=window) if window else s.ok("screenshot")) or {}
    data = shot.get("data") or ""
    if isinstance(data, str) and data.startswith("BASE64:"):
        raw = data[len("BASE64:"):]
        data = base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4))
    img = Image.open(io.BytesIO(data)).convert("RGB")
    # Material Red 500 is (244, 67, 54); the grey square, the caption and the white ground are not
    # anywhere near it.
    px = img.tobytes()
    return sum(1 for i in range(0, len(px), 3)
            if px[i] > 200 and px[i + 1] < 110 and px[i + 2] < 110)


def run(binary, gapi):
    pid = os.getpid()
    sock = f"/tmp/xl-damage-check-{gapi}-{pid}.sock"
    log = f"/tmp/xl-damage-check-{gapi}-{pid}.log"
    print(f"--- gapi: {gapi}")
    proc, s = start_app(binary, sock, log, gapi)
    mark = 0
    try:
        density = 1.0
        for w in (s.ok("windows") or {}).get("windows", []):
            if w.get("default"):
                density = float(w.get("density") or 1.0)
        square = (SQUARE * density) ** 2

        # Sample through the whole walk and past its end: every frame the square moved in was a
        # partial redraw of an image that last held an older position.
        areas = []
        deadline = time.monotonic() + STEP_DELAY * (STEPS + 2)
        while time.monotonic() < deadline:
            s.ok("frame", count=2)
            time.sleep(0.12)
            areas.append(red_area(s))

        bad = [a for a in areas if not (0.9 * square <= a <= 1.1 * square)]
        check("every frame shows exactly one red square (no trail)", not bad and len(areas) >= 5,
                f"expected ~{int(square)} red pixels, got {areas}")

        # --- a label changing colour --------------------------------------------------------------
        #
        # A label's glyphs are zero-size points the shader moves by their atlas entries, so its box
        # is found through the atlas. Scanned as points, a line of text on a baseline has no
        # height: the label drops out of the damage and keeps its old colour. Every change must be a
        # partial repaint of its own, and the picture after an even number of them - the first
        # colour again - the picture a full redraw gives.
        word = "damage: partial redraw" if gapi == "vulkan" else "damage: repainting"
        decisions = []
        for _ in range(4):
            mark = len(open(log, errors="replace").read())
            s.ok("invoke", name="damage.label-color", args={})
            s.ok("frame", count=4)
            time.sleep(0.2)
            text = open(log, errors="replace").read()[mark:]
            decisions.append((text.count(word), text.count("damage: full")))
        s.ok("frame", count=4)
        time.sleep(0.3)
        partial_shot = shot_bytes(s)
        check("every colour change of a label is a partial repaint of its own",
                all(p > 0 and f == 0 for p, f in decisions),
                f"(partial, full) decisions per change: {decisions}")
        s.ok("window", op="resize", width=WIDTH + 40, height=HEIGHT)
        s.ok("frame", count=4)
        time.sleep(0.4)
        s.ok("window", op="resize", width=WIDTH, height=HEIGHT)
        s.ok("frame", count=6)
        time.sleep(0.5)
        full_shot = shot_bytes(s)
        check("a label's colour changes repaint the whole label", partial_shot == full_shot,
                "the partially redrawn picture differs from a full redraw")

        # --- the same walk in a virtual window, read as a plane ------------------------------------
        #
        # A compositor holds published frames, so their slots are skipped and the next frame lands
        # in an image that held an OLDER frame than the one before it. That history is exactly what
        # the per-slot damage snapshot has to get right - and on Vulkan the image went through
        # ShaderReadOnly and back before it is LOADed again.
        mark = len(open(log, errors="replace").read())
        s.ok("invoke", name="open-virtual", args={"layout": "damage", "width": WIDTH,
                "height": HEIGHT})
        vid = None
        deadline = time.monotonic() + 20.0
        while vid is None and time.monotonic() < deadline:
            s.ok("frame", count=1)
            for w in (s.ok("windows") or {}).get("windows", []):
                if w.get("virtual"):
                    vid = w.get("id")
            time.sleep(0.1)
        if vid is None:
            raise SystemExit("the virtual window never appeared")
        deadline = time.monotonic() + 20.0
        while not (s.ok("window", window=vid, op="plane") or {}).get("serial") \
                and time.monotonic() < deadline:
            s.ok("frame", window=vid, count=1)
            time.sleep(0.1)

        areas, most_pinned = [], 0
        deadline = time.monotonic() + STEP_DELAY * (STEPS + 2)
        while time.monotonic() < deadline:
            s.ok("window", window=vid, op="plane-hold", ms=400)
            s.ok("frame", window=vid, count=2)
            time.sleep(0.12)
            most_pinned = max(most_pinned,
                    len((s.ok("window", window=vid, op="plane") or {}).get("pinned", [])))
            areas.append(red_area(s, window=vid))

        bad = [a for a in areas if not (0.9 * square <= a <= 1.1 * square)]
        check("a virtual window with held frames shows exactly one red square",
                not bad and len(areas) >= 5, f"expected ~{int(square)} red pixels, got {areas}")
        check("and its frames really were held", most_pinned >= 2, f"{most_pinned} pinned at most")
    finally:
        try:
            s.ok("quit")
        except SystemExit:
            pass
        s.close()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
        try:
            os.unlink(sock)
        except OSError:
            pass

    text = open(log, errors="replace").read()
    # Each backend's damage log (XL_VK_DAMAGE_LOG, XL_SOFT_DAMAGE_LOG) says it in its own words
    word = "damage: partial redraw" if gapi == "vulkan" else "damage: repainting"
    check("the frames took the partial-redraw path", text[:mark].count(word) > 0,
            f"no partial redraw in the {gapi} damage log ({log})")
    check("so did the virtual window's", text[mark:].count(word) > 0,
            f"no partial redraw after the virtual window opened ({log})")
    check("no Vulkan validation error", "Validation Error" not in text, log)


def main():
    gapis = ["vulkan", "soft"]
    argv = sys.argv[1:]
    while argv and argv[0].startswith("--"):
        opt, argv = argv[0], argv[1:]
        if "=" in opt:
            opt, value = opt.split("=", 1)
        else:
            if not argv:
                raise SystemExit(f"{opt} needs a value")
            value, argv = argv[0], argv[1:]
        if opt == "--gapi":
            gapis = [value]
        else:
            raise SystemExit(f"unknown option: {opt}")

    binary = os.path.join(_here, "stappler-build", "x86_64-unknown-linux-gnu", "debug", "cc",
            "testapp")
    if argv:
        binary = argv[0]
    if not os.path.exists(binary):
        raise SystemExit(f"missing binary: {binary}\nbuild it: xenolith-cli build tests/window")

    for gapi in gapis:
        run(binary, gapi)

    print(f"{checks} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
