#!/usr/bin/env python3
"""A window asks for its frames itself, and every change has to end in a finite number of asks.

A node that changes marks the scene changed (Node::markSceneChanged), and the director asks its
window for a frame (Director::handleSceneChanged -> requestFrame). What must never happen is a
chain: a frame whose drawing changes the scene again, so that it asks for the next one, and so on
for ever. The frame-request stand (XL_FRAME_REQUEST_TEST) makes one kind of change per command from
outside a frame, and nothing on it moves by itself. For each change this check reads the director's
own count of asks (`frame` with count 0 reports `requested`; it steps nothing) and requires:

  * the change was drawn - the window presented a frame after it;
  * the asks stopped - nothing new for QUIET seconds, within SETTLE_TIMEOUT;
  * and there were few of them.

Nothing here steps the window: a `frame` call with a count would draw frames the director did not
ask for and hide exactly the failure this is about.

    tests/window/frame-request-check.py [--gapi vulkan|soft] [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, subprocess, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("damage_check",
        os.path.join(_here, "damage-check.py"))
dc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dc)

QUIET = 1.5  # seconds with no new ask and no new frame: settled
SETTLE_TIMEOUT = 15.0
IDLE = 3.0  # seconds an idle scene is watched for

# Asks one change may cost. A change asks once, and a wakeup landing beside it may add one. A task
# the visit posts is a change after the frame and asks once more. The frame that draws a label's new
# glyphs is gated on the atlas, and the upload and the materials each wake the app thread.
MAX_ASKS = 2
MAX_ASKS_ECHO = 3
MAX_ASKS_GLYPHS = 6

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


def start_app(binary, sock, log, gapi):
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    env["XL_FRAME_REQUEST_TEST"] = "1"
    env["XL_HIDE_FPS"] = "1"
    try:
        os.unlink(sock)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768",
            "--gapi", gapi], env=env, cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(log, "w"), stderr=subprocess.STDOUT)
    for _ in range(600):
        if proc.poll() is not None:
            raise SystemExit(f"testapp exited early with {proc.returncode}, see {log}")
        if os.path.exists(sock):
            try:
                return proc, dc.Session(sock)
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("testapp did not come up")


def counters(s):
    """(asks, presented frames, scene changes), read without asking for anything."""
    r = s.ok("frame", count=0) or {}
    return int(r.get("requested", -1)), int(r.get("presented", 0)), int(r.get("changes", 0))


def settle(s, quiet=QUIET, timeout=SETTLE_TIMEOUT):
    """Poll until neither the asks nor the frames moved for `quiet` seconds. Returns
    (settled, counters)."""
    last = counters(s)
    since = time.monotonic()
    deadline = since + timeout
    while time.monotonic() < deadline:
        time.sleep(0.1)
        now = counters(s)
        if now[:2] != last[:2]:
            last, since = now, time.monotonic()
        elif time.monotonic() - since >= quiet:
            return True, now
    return False, last


def invoke(s, name, args=None):
    """A stand command. They answer at once: the reply of an ordinary test command waits for frames
    it asks for itself (TestLayout::addCommand), and those would be counted here."""
    s.ok("invoke", name=f"frame-request.{name}", args=args or {})


def change(s, name, args=None, limit=MAX_ASKS, drawn=True):
    before = counters(s)
    invoke(s, name, args)
    settled, after = settle(s)
    asks, frames = after[0] - before[0], after[1] - before[1]
    what = f"{name}{'' if not args else ' ' + str(args)}"
    print(f"       {what}: {asks} asks, {frames} frames, {after[2] - before[2]} changes")
    check(f"{what}: the asks stop", settled, f"still asking after {SETTLE_TIMEOUT} s: "
            f"{asks} asks, {frames} frames")
    if drawn:
        check(f"{what}: the change is drawn", frames >= 1, f"{frames} frames")
    else:
        check(f"{what}: nothing changed, nothing is drawn", frames == 0 and asks == 0,
                f"{asks} asks, {frames} frames")
    check(f"{what}: at most {limit} asks", asks <= limit, f"{asks} asks")
    return settled


def run(binary, gapi):
    pid = os.getpid()
    sock = f"/tmp/xl-frame-request-check-{gapi}-{pid}.sock"
    log = f"/tmp/xl-frame-request-check-{gapi}-{pid}.log"
    print(f"--- gapi: {gapi}")
    proc, s = start_app(binary, sock, log, gapi)
    try:
        deadline = time.monotonic() + 30.0
        while counters(s)[1] == 0 and time.monotonic() < deadline:
            time.sleep(0.1)
        check("the window draws its first frame without a kick", counters(s)[1] > 0, log)

        settled, _ = settle(s)
        check("the scene settles after it opens", settled, "it kept asking for frames")

        before = counters(s)
        time.sleep(IDLE)
        after = counters(s)
        check("an idle scene asks for nothing and draws nothing", after[:2] == before[:2],
                f"asks {before[0]} -> {after[0]}, frames {before[1]} -> {after[1]}")

        change(s, "same", drawn=False)
        change(s, "move")
        change(s, "move")
        change(s, "resize")
        change(s, "resize")
        change(s, "echo", limit=MAX_ASKS_ECHO)
        change(s, "echo", limit=MAX_ASKS_ECHO)
        # Characters the stand has not shown yet: the frame waits for the atlas
        change(s, "text", {"text": "Жёлтый щавель Ω 42"}, limit=MAX_ASKS_GLYPHS)
        change(s, "text", {"text": "Quizzical ψυχή ЮЯ ÿ"}, limit=MAX_ASKS_GLYPHS)
        change(s, "text", {"text": "Frame requests"})

        # Many changes in a row, faster than frames: they share frames rather than queue them
        before = counters(s)
        for _ in range(20):
            invoke(s, "move")
        settled, after = settle(s)
        asks = after[0] - before[0]
        print(f"       20 moves: {asks} asks, {after[1] - before[1]} frames")
        check("twenty quick moves: the asks stop", settled, f"{asks} asks")
        check("and they cost no more asks than moves", asks <= 20 + MAX_ASKS, f"{asks} asks")

        # A finite animation asks on every frame while it runs, and not after it
        before = counters(s)
        invoke(s, "animate", {"seconds": 0.8})
        settled, after = settle(s)
        frames = after[1] - before[1]
        print(f"       animate 0.8 s: {after[0] - before[0]} asks, {frames} frames")
        check("an animation is drawn without a kick", frames >= 5, f"{frames} frames")
        check("and the asks stop when it ends", settled, f"{after[0] - before[0]} asks")

        before = counters(s)
        time.sleep(IDLE)
        after = counters(s)
        check("and afterwards the scene is idle again", after[:2] == before[:2],
                f"asks {before[0]} -> {after[0]}, frames {before[1]} -> {after[1]}")
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
    check("no Vulkan validation error", "Validation Error" not in text, log)


def main():
    gapis = ["vulkan"]
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
        binary = os.path.abspath(argv[0])
    if not os.path.exists(binary):
        raise SystemExit(f"missing binary: {binary}\nbuild it: xenolith-cli build tests/window")

    for gapi in gapis:
        run(binary, gapi)

    print(f"{checks} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
