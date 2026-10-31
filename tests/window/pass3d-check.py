#!/usr/bin/env python3
"""A depth-tested 3d pass under the basic2d UI, in tests/pass3d, checked by the picture it leaves.

tests/pass3d adds its own graphics pass to the stock basic2d queue through the Scene2d::init builder
callback: the pass clears Output, draws a ground slab and two boxes that cut through each other with
its own depth attachment, and the basic2d pass composites the UI over it. With PASS3D_STILL=1 the
picture is fixed, so the check reads known pixels of a headless screenshot:

  * the ground and both boxes are there - the 2d pass LOADS what the 3d pass wrote. If the queue
    compiler sees the 2d pass as the first writer of Output, it clears it and only the UI is left;
    that is what both the load/store bug in Queue_buildLoadStore and a 2d pass without ReadColor do;
  * the boxes are in front of the ground, which is drawn last: the depth test works;
  * the tall box hides the cube in one place and the cube hides the box in another: they intersect
    by depth, not by draw order;
  * the semi-transparent 2d veil shows the ground through it: the UI composites over the 3d pass.

A second run with PASS3D_NO3D=1 is the baseline: the same pixels then show the background, so the
first run's numbers are not something the stock queue draws anyway.

    tests/window/pass3d-check.py [path-to-pass3d]

With no argument it expects the debug x86_64-linux build of tests/pass3d. Prints "N checks,
M failures"; exit status is the result.
"""
import io, os, subprocess, sys, time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests/parity"))
import xlclient  # noqa: E402  (the inspector protocol, shared with the parity harness)

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-pass3d-check.sock")
WIDTH, HEIGHT = 960, 640
TOLERANCE = 12

# Pixels of the PASS3D_STILL picture at 960x640, density 1, and their colours (8-bit UNORM)
BACKGROUND = ((10, 10), (31, 31, 36))  # Scene2d background, cleared by the 3d pass
GROUND = ((200, 320), (97, 128, 77))  # the slab's lit top
BOX_TOP = ((462, 152), (70, 116, 197))  # the tall box's top, above the ground drawn after it
BOX_OVER_CUBE = ((420, 260), (27, 45, 76))  # the tall box's face in front of the cube
CUBE_OVER_BOX = ((495, 300), (172, 85, 61))  # the cube's face in front of the tall box
VEIL = ((480, 510), (176, 191, 166))  # white at 0.5 over the ground

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


def near(a, b):
    return all(abs(x - y) <= TOLERANCE for x, y in zip(a, b))


def run(binary, env_extra, log_path):
    """The screenshot of a still frame and the app's log"""
    env = dict(os.environ)
    env.update(env_extra)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    log = open(log_path, "w")
    proc = subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height", str(HEIGHT)],
            env=env, stdout=log, stderr=subprocess.STDOUT)
    session = None
    for _ in range(600):
        if proc.poll() is not None:
            raise SystemExit(f"pass3d exited early with {proc.returncode}, see {log_path}")
        if os.path.exists(ADDR):
            try:
                session = xlclient.Session("unix:" + ADDR, 60)
                break
            except OSError:
                pass
        time.sleep(0.05)
    if not session:
        proc.kill()
        raise SystemExit("pass3d did not come up")

    # The still scene changes nothing by itself: redraw every frame, then step a few
    session.call("render")
    for _ in range(4):
        session.call("frame")
    shot = session.call("screenshot")
    image = Image.open(io.BytesIO(xlclient.decode_bytes(shot["data"]))).convert("RGB")

    try:
        session.call("quit", graceful=True)
        session.close()
        proc.wait(timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        proc.kill()
    log.close()
    with open(log_path, errors="replace") as f:
        return image, f.read()


def main():
    binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT,
            "tests/pass3d/stappler-build/x86_64-unknown-linux-gnu/debug/cc/pass3d"))
    if not os.path.exists(binary):
        raise SystemExit(f"no pass3d at {binary}: build tests/pass3d first")

    log_path = f"/tmp/xl-pass3d-check-{os.getpid()}.log"
    image, text = run(binary, {"PASS3D_STILL": "1"}, log_path)

    def at(sample):
        return image.getpixel(sample[0])

    print("--- 3d pass under the 2d UI")
    check("the picture is the expected size", image.size == (WIDTH, HEIGHT), str(image.size))
    check("the 3d pass cleared to the scene background", near(at(BACKGROUND), BACKGROUND[1]),
            str(at(BACKGROUND)))
    check("the ground survives the 2d pass", near(at(GROUND), GROUND[1]), str(at(GROUND)))
    check("the box stands over the ground drawn after it", near(at(BOX_TOP), BOX_TOP[1]),
            str(at(BOX_TOP)))
    check("the box hides the cube here", near(at(BOX_OVER_CUBE), BOX_OVER_CUBE[1]),
            str(at(BOX_OVER_CUBE)))
    check("and the cube hides the box there", near(at(CUBE_OVER_BOX), CUBE_OVER_BOX[1]),
            str(at(CUBE_OVER_BOX)))
    check("the 2d veil blends over the ground", near(at(VEIL), VEIL[1]), str(at(VEIL)))
    check("no Vulkan validation error", "Validation Error" not in text, log_path)

    base_path = f"/tmp/xl-pass3d-check-base-{os.getpid()}.log"
    base, _ = run(binary, {"PASS3D_STILL": "1", "PASS3D_NO3D": "1"}, base_path)

    print("--- baseline without the 3d pass")
    check("the ground pixel is background without it",
            near(base.getpixel(GROUND[0]), BACKGROUND[1]), str(base.getpixel(GROUND[0])))
    check("and the veil blends over the background",
            not near(base.getpixel(VEIL[0]), VEIL[1]), str(base.getpixel(VEIL[0])))

    print(f"\n{checks} checks, {failures} failures")
    if failures == 0:
        for p in (log_path, base_path):
            try:
                os.unlink(p)
            except OSError:
                pass
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
