#!/usr/bin/env python3
"""The basic3d module in examples/window/basic3d, checked by the picture it leaves.

The example is a basic3d::Scene3d with a View3d over the whole window: a torus with a coarse checker
texture over a ground plane with a fine one, lit by an ambient light and a sun, under a translucent
basic2d panel and a label. With BASIC3D_STILL=1 the picture is fixed, so the check reads regions of a
headless screenshot:

  * the torus is drawn, textured (both colours of its checker show) and lit by the sun (its sunny
    side is brighter than its shaded one);
  * the ground's fine checker shrinks four to eight times on screen: through the mip chain the GPU
    generated, it averages to an even colour. A second run with BASIC3D_NOMIPS=1 samples level 0
    alone and the same ground aliases into noise - that the two differ is what proves the chain is
    sampled;
  * the 2d panel composites over the 3d picture;
  * no validation error, no render pass ending early, no basic3d complaint in the log.

    tests/window/basic3d-check.py [path-to-basic3d]

With no argument it expects the debug x86_64-linux build of examples/window/basic3d. Prints "N checks,
M failures"; exit status is the result.
"""
import io, os, statistics, subprocess, sys, time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests/parity"))
import xlclient  # noqa: E402  (the inspector protocol, shared with the parity harness)

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-basic3d-check.sock")
WIDTH, HEIGHT = 960, 640

# Regions of the BASIC3D_STILL picture at 960x640, density 1: (x0, y0, x1, y1), y down
TORUS = (325, 110, 640, 355)
TORUS_SHADED = (325, 110, 430, 355)  # the -x side, away from the sun
TORUS_SUNNY = (530, 110, 640, 355)  # the +y side, toward it
GROUND = [(700, 450, 740, 490), (800, 100, 840, 140)]  # clear of the torus and the 2d
PANEL = (240, 78)  # inside the translucent panel, clear of its label

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


def lum(p):
    return 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]


def pixels(image, box):
    x0, y0, x1, y1 = box
    return [image.getpixel((x, y)) for x in range(x0, x1) for y in range(y0, y1)]


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
            raise SystemExit(f"basic3d exited early with {proc.returncode}, see {log_path}")
        if os.path.exists(ADDR):
            try:
                session = xlclient.Session("unix:" + ADDR, 60)
                break
            except OSError:
                pass
        time.sleep(0.05)
    if not session:
        proc.kill()
        raise SystemExit("basic3d did not come up")

    # The textures load after the first frames: redraw every frame, step a few, then a pause
    session.call("render")
    for _ in range(6):
        session.call("frame")
    time.sleep(0.5)
    for _ in range(2):
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
            "examples/window/basic3d/stappler-build/x86_64-unknown-linux-gnu/debug/cc/basic3d"))
    if not os.path.exists(binary):
        raise SystemExit(f"no basic3d at {binary}: build examples/window/basic3d first")

    log_path = f"/tmp/xl-basic3d-check-{os.getpid()}.log"
    image, text = run(binary, {"BASIC3D_STILL": "1"}, log_path)

    ground = image.getpixel((GROUND[0][0], GROUND[0][1]))

    def isTorus(p):
        return abs(p[0] - ground[0]) + abs(p[1] - ground[1]) + abs(p[2] - ground[2]) > 40

    print("--- the 3d scene")
    check("the picture is the expected size", image.size == (WIDTH, HEIGHT), str(image.size))
    torus = [p for p in pixels(image, TORUS) if isTorus(p)]
    check("the torus is drawn", len(torus) > 15000, f"{len(torus)} pixels")
    orange = sum(1 for p in torus if p[0] > 150 and p[2] < 110 and p[0] - p[2] > 80)
    white = sum(1 for p in torus if min(p) > 170)
    check("its texture shows both colours of the checker", orange > 1000 and white > 1000,
            f"orange {orange}, white {white}")
    shaded = [lum(p) for p in pixels(image, TORUS_SHADED) if isTorus(p)]
    sunny = [lum(p) for p in pixels(image, TORUS_SUNNY) if isTorus(p)]
    shadedMean = statistics.mean(shaded) if shaded else 0
    sunnyMean = statistics.mean(sunny) if sunny else 0
    check("its sunny side is brighter than its shaded one", sunnyMean - shadedMean > 40,
            f"{sunnyMean:.1f} vs {shadedMean:.1f}")

    print("--- the 2d over it")
    panel = image.getpixel(PANEL)
    check("the translucent panel composites over the ground",
            lum(ground) + 50 < lum(panel) < 230, f"{panel} over {ground}")

    print("--- mip levels")
    spreads = [statistics.pstdev([lum(p) for p in pixels(image, box)]) for box in GROUND]
    check("the fine checker averages to an even colour through the mips",
            all(s < 4 for s in spreads), str([round(s, 1) for s in spreads]))

    nomips_path = f"/tmp/xl-basic3d-check-nomips-{os.getpid()}.log"
    nomips, _ = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_NOMIPS": "1"}, nomips_path)
    raw = [statistics.pstdev([lum(p) for p in pixels(nomips, box)]) for box in GROUND]
    check("and aliases when level 0 alone is sampled", all(s > 20 for s in raw),
            str([round(s, 1) for s in raw]))

    print("--- the log")
    check("no Vulkan validation error", "Validation Error" not in text, log_path)
    check("the render passes end in their last subpass", "ended in subpass" not in text, log_path)
    check("no basic3d error", "basic3d::" not in text and "FrameContext3d" not in text, log_path)

    print(f"\n{checks} checks, {failures} failures")
    if failures == 0:
        for p in (log_path, nomips_path):
            try:
                os.unlink(p)
            except OSError:
                pass
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
