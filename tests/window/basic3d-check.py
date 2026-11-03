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
  * two boxes are drawn by the example's own pipeline (Scene3d's app pipelines): one in red and blue
    stripes, the colours of its material's params, the other green alone, as its flag 1 asks;
  * a third box takes its second colour from the material's second texture: the coarse checker's
    orange shows where its blue would be; and its first from the third texture: magenta where
    its red would be;
  * a fourth takes it from a layered texture (array layers, the pass's sampler2DArray slots), the
    layer its params pick: the third layer's cyan, none of the other layers' red or white;
  * a point light makes a pool of light on the ground under it, gone with BASIC3D_NO_LIGHT=1,
    and the ground past its radius is as without it;
  * every mesh is in one pack in GPU memory (MeshPack3d): once it is loaded the frames copy no
    vertex into themselves and draw everything from the pack; with BASIC3D_NOPACK=1 they copy
    every mesh, and the picture is the same; removing the draws releases the pack, which leaves
    the resource cache; with the example letting go of the meshes and the pack too, the frames in
    flight hold their last references and release them on the app thread;
  * a material picking a pipeline the scene does not have (BASIC3D_UNKNOWN_PIPELINE=1, in the second
    run) draws as a mesh, with one warning; its layered texture is none the mesh pipeline can
    sample, so it is white, with a warning of its own;
  * the 2d panel composites over the 3d picture;
  * a translucent pipeline (BASIC3D_GLASS=1, a run of its own) draws in the second subpass, over
    the opaque draws: a tilted glass blends its colour over the ground and the green box behind
    it, more where the depth it reads from the opaque draws is further behind it; outside it the
    picture is as without it; the pass counts it a translucent draw;
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
GROUND = [(700, 450, 740, 490), (60, 120, 100, 160)]  # clear of the torus, the boxes and the 2d
PANEL = (240, 78)  # inside the translucent panel, clear of its label
STRIPES = (760, 200, 950, 400)  # the box in red and blue stripes
SOLID = (10, 200, 200, 400)  # the box in green alone
UNKNOWN = (395, 5, 565, 85)  # the box of an unknown pipeline, in the second run
TEXTURED = (600, 15, 830, 170)  # the box whose second colour is the material's second texture
LAYERED = (740, 450, 950, 600)  # the box whose second colour is a layer of a layered texture
POOL = (370, 500, 410, 530)  # the ground under the point light
GLASS_COLOUR = (25.5, 76.5, 229.5)  # the glass's params[1], as 8 bits
GLASS_THICK = (160, 370, 200, 390)  # the glass where the ground is two units behind it
GLASS_THIN = (250, 480, 290, 500)  # where it is under a unit

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


def run(binary, env_extra, log_path, probe=None):
    """The screenshot of a still frame and the app's log; `probe(session)` runs after the shot,
    and what it returns is the third value"""
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
    probed = probe(session) if probe else None

    try:
        session.call("quit", graceful=True)
        session.close()
        proc.wait(timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        proc.kill()
    log.close()
    with open(log_path, errors="replace") as f:
        if probe:
            return image, f.read(), probed
        return image, f.read()


def main():
    binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT,
            "examples/window/basic3d/stappler-build/x86_64-unknown-linux-gnu/debug/cc/basic3d"))
    if not os.path.exists(binary):
        raise SystemExit(f"no basic3d at {binary}: build examples/window/basic3d first")

    log_path = f"/tmp/xl-basic3d-check-{os.getpid()}.log"

    def probe(session):
        stats = session.call("invoke", name="basic3d.stats", args={})
        dropped = session.call("invoke", name="basic3d.drop", args={})
        gone = None
        for i in range(120):
            session.call("frame")
            time.sleep(0.02)
            after = session.call("invoke", name="basic3d.stats", args={})
            if not after["packInCache"]:
                gone = i + 1
                break
        # the frames in flight held the last references to the meshes, and through them the pack
        for _ in range(10):
            session.call("frame")
        after = session.call("invoke", name="basic3d.stats", args={})
        return stats, dropped, after, gone

    image, text, (packed, dropped, released, gone) = run(binary, {"BASIC3D_STILL": "1"}, log_path, probe)

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

    print("--- an app pipeline")
    stripes = pixels(image, STRIPES)
    red = sum(1 for p in stripes if p[0] > 120 and p[1] < 40 and p[2] < 40)
    blue = sum(1 for p in stripes if p[2] > 120 and p[0] < 40 and p[1] < 40)
    check("it draws a box in the two colours of its params", red > 1500 and blue > 1500,
            f"red {red}, blue {blue}")
    solid = pixels(image, SOLID)
    green = sum(1 for p in solid if p[1] > 120 and p[0] < 40 and p[2] < 40)
    yellow = sum(1 for p in solid if p[0] > 120 and p[1] > 120 and p[2] < 40)
    check("and the other in the first colour alone, by its flag", green > 5000 and yellow == 0,
            f"green {green}, yellow {yellow}")
    textured = pixels(image, TEXTURED)
    orange = sum(1 for p in textured if p[0] > 150 and 60 < p[1] < 160 and p[2] < 90)
    blue = sum(1 for p in textured if p[2] > 120 and p[0] < 40 and p[1] < 40)
    check("a second texture takes the place of the second colour", orange > 800 and blue == 0,
            f"orange {orange}, blue {blue}")
    magenta = sum(1 for p in textured if p[0] > 120 and p[2] > 120 and p[1] < 40)
    red = sum(1 for p in textured if p[0] > 120 and p[1] < 40 and p[2] < 40)
    check("a third texture takes the place of the first colour", magenta > 800 and red == 0,
            f"magenta {magenta}, red {red}")
    layered = pixels(image, LAYERED)
    cyan = sum(1 for p in layered if p[1] > 120 and p[2] > 120 and p[0] < 40)
    white = sum(1 for p in layered if min(p) > 150)
    blue = sum(1 for p in layered if p[2] > 120 and p[0] < 40 and p[1] < 40)
    check("a layered texture's layer the params pick takes its place", cyan > 3000
            and white == 0 and blue == 0, f"cyan {cyan}, white {white}, blue {blue}")
    check("the unknown pipeline's box is absent without its variable",
            sum(1 for p in pixels(image, UNKNOWN) if min(p) > 150) < 50)

    print("--- the 2d over it")
    panel = image.getpixel(PANEL)
    check("the translucent panel composites over the ground",
            lum(ground) + 50 < lum(panel) < 230, f"{panel} over {ground}")

    print("--- mip levels")
    spreads = [statistics.pstdev([lum(p) for p in pixels(image, box)]) for box in GROUND]
    check("the fine checker averages to an even colour through the mips",
            all(s < 4 for s in spreads), str([round(s, 1) for s in spreads]))

    nomips_path = f"/tmp/xl-basic3d-check-nomips-{os.getpid()}.log"
    nomips, nomipsText = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_NOMIPS": "1",
            "BASIC3D_UNKNOWN_PIPELINE": "1"}, nomips_path)
    raw = [statistics.pstdev([lum(p) for p in pixels(nomips, box)]) for box in GROUND]
    check("and aliases when level 0 alone is sampled", all(s > 20 for s in raw),
            str([round(s, 1) for s in raw]))

    print("--- the mesh pack")
    check("the frame copies no vertex: every draw is from the pack",
            packed["frameVertexBytes"] == 0 and packed["frameDraws"] == 0
            and packed["packDraws"] >= 5 and packed["packLoaded"], str(packed))
    flat_path = f"/tmp/xl-basic3d-check-nopack-{os.getpid()}.log"
    flat, _, unpacked = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_NOPACK": "1"}, flat_path,
            lambda session: session.call("invoke", name="basic3d.stats", args={}))
    check("with BASIC3D_NOPACK=1 the frame copies every mesh",
            unpacked["frameVertexBytes"] > 50000 and unpacked["packDraws"] == 0, str(unpacked))
    # the ground's 4 vertexes and the torus's 65 x 25 of Vertex3d, 64 bytes each; the four boxes of
    # the stripes, 24 vertexes each of their own format, Compact3d, 16 bytes each
    check("each mesh in the bytes of its vertex format",
            unpacked["frameVertexBytes"] == (4 + 65 * 25) * 64 + 4 * 24 * 16, str(unpacked))
    # the frame statistics at the bottom left change between runs
    worst = max(max(abs(a - b) for a, b in zip(image.getpixel((x, y)), flat.getpixel((x, y))))
            for x in range(0, WIDTH, 2) for y in range(0, 540, 2))
    check("and the picture is the same", worst <= 1, f"{worst} levels at most")
    check("removing the draws releases the pack: it leaves the cache",
            dropped["packUsers"] == 0 and gone is not None, f"after {gone} frames")
    check("the last references the frames held are released on the app thread",
            released["packsReleasedOffThread"] == 0, str(released))

    print("--- a point light")
    dark_path = f"/tmp/xl-basic3d-check-nolight-{os.getpid()}.log"
    dark, _ = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_NO_LIGHT": "1"}, dark_path)
    pool = statistics.mean(lum(p) for p in pixels(image, POOL))
    unlit = statistics.mean(lum(p) for p in pixels(dark, POOL))
    check("it makes a pool of light on the ground", pool - unlit > 40,
            f"{pool:.1f} with it, {unlit:.1f} without")
    far = [statistics.mean(lum(p) for p in pixels(im, GROUND[1])) for im in (image, dark)]
    check("past its radius the ground is as without it", abs(far[0] - far[1]) < 1.0,
            f"{far[0]:.1f} with it, {far[1]:.1f} without")

    print("--- a translucent pipeline")
    check("the pass has no translucent draw without the glass", packed["translucentDraws"] == 0,
            str(packed))
    glass_path = f"/tmp/xl-basic3d-check-glass-{os.getpid()}.log"
    glass, glassText, glassStats = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_GLASS": "1"},
            glass_path, lambda session: session.call("invoke", name="basic3d.stats", args={}))
    check("and one with it", glassStats["translucentDraws"] == 1, str(glassStats))
    check("its material of a schema is written once, 32 bytes",
            glassStats["materials"] == 1 and glassStats["materialBytes"] == 32, str(glassStats))
    check("and none without it", packed["materials"] == 0 and packed["materialBytes"] == 0,
            str(packed))
    check("the view's block is written once, 4 bytes, with the glass and without it",
            glassStats["blocks"] == 1 and glassStats["blockBytes"] == 4
            and packed["blocks"] == 1 and packed["blockBytes"] == 4, f"{glassStats} {packed}")

    def alpha(box):
        # how far each pixel went from the scene toward the glass's colour, the mean over the box
        ret = []
        for x in range(box[0], box[2]):
            for y in range(box[1], box[3]):
                on, off = glass.getpixel((x, y)), image.getpixel((x, y))
                d = [GLASS_COLOUR[i] - off[i] for i in range(3)]
                ret.append(sum((on[i] - off[i]) * d[i] for i in range(3)) / sum(v * v for v in d))
        return statistics.mean(ret)

    thick, thin = alpha(GLASS_THICK), alpha(GLASS_THIN)
    check("it blends over the opaque draws: neither its colour nor theirs",
            0.15 < thin and thick < 0.97, f"{thin:.2f} and {thick:.2f} of the way to its colour")
    check("more where the depth it reads is further behind it", thick - thin > 0.2,
            f"{thick:.2f} vs {thin:.2f}")
    worst = max(max(abs(a - b) for a, b in zip(image.getpixel((x, y)), glass.getpixel((x, y))))
            for box in [TORUS, STRIPES, TEXTURED, LAYERED] + GROUND
            for x in range(box[0], box[2], 2) for y in range(box[1], box[3], 2))
    check("outside it the picture is as without it", worst <= 1, f"{worst} levels at most")
    check("no Vulkan validation error with it", "Validation Error" not in glassText, glass_path)
    check("its render pass ends in its last subpass", "ended in subpass" not in glassText,
            glass_path)

    print("--- a pipeline's material missing")
    nodata_path = f"/tmp/xl-basic3d-check-nodata-{os.getpid()}.log"
    nodata, nodataText, nodataStats = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_GLASS": "1",
            "BASIC3D_GLASS_NODATA": "1"}, nodata_path,
            lambda session: session.call("invoke", name="basic3d.stats", args={}))
    check("the glass without its material is no translucent draw",
            nodataStats["translucentDraws"] == 0 and nodataStats["materials"] == 0,
            str(nodataStats))
    mismatch = nodataText.count("reads a material of a schema")
    check("the mesh pipeline draws it, with one warning", mismatch == 1, f"{mismatch} warnings")
    check("no Vulkan validation error without it", "Validation Error" not in nodataText,
            nodata_path)

    print("--- a pipeline's view block missing")
    noblock_path = f"/tmp/xl-basic3d-check-noblock-{os.getpid()}.log"
    noblock, noblockText, noblockStats = run(binary, {"BASIC3D_STILL": "1", "BASIC3D_GLASS": "1",
            "BASIC3D_NOBLOCK": "1"}, noblock_path,
            lambda session: session.call("invoke", name="basic3d.stats", args={}))
    check("the glass in a view without the block is no translucent draw",
            noblockStats["translucentDraws"] == 0 and noblockStats["blocks"] == 0,
            str(noblockStats))
    blockWarnings = noblockText.count("reads a view's block of a schema")
    check("the mesh pipeline draws it, with one warning", blockWarnings == 1,
            f"{blockWarnings} warnings")
    check("no Vulkan validation error without it", "Validation Error" not in noblockText,
            noblock_path)

    print("--- a pipeline the scene does not have")
    white = sum(1 for p in pixels(nomips, UNKNOWN) if min(p) > 150)
    check("its box draws as a mesh", white > 3000, f"{white} pixels")
    warnings = nomipsText.count("which the pass does not have")
    check("with one warning", warnings == 1, f"{warnings} warnings")
    layeredWarnings = nomipsText.count("samples no layered texture")
    check("its layered texture is dropped, with one warning", layeredWarnings == 1,
            f"{layeredWarnings} warnings")

    print("--- the log")
    check("no Vulkan validation error", "Validation Error" not in text, log_path)
    check("the render passes end in their last subpass", "ended in subpass" not in text, log_path)
    check("no basic3d error", "basic3d::" not in text and "FrameContext3d" not in text, log_path)

    print(f"\n{checks} checks, {failures} failures")
    if failures == 0:
        for p in (log_path, nomips_path, dark_path, flat_path, glass_path, nodata_path,
                  noblock_path):
            try:
                os.unlink(p)
            except OSError:
                pass
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
