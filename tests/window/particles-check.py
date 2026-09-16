#!/usr/bin/env python3
"""Check the basic2d GPU particles against their CPU reference, in examples/window/particles.

The particle update shader and the CPU reference compile the same text, XL2dGlslParticleSim.h, so
with a fixed seed the particles on the GPU after S steps are the reference's particles after S
steps. The example computes the reference itself (particles.snapshot {reference: true}): it has the
system, the texture and the node transform the renderer took. What is compared:

  * integers - the generator state and both lifetimes - for EQUALITY: a different emission step, a
    different phase, one random number too many all show here first;
  * floats relative to max(1, |value|): the 95th percentile within 1e-3 and the worst within 1e-2.
    GPU and CPU float arithmetic are not bit-identical (contraction into fused multiply-add, the
    trigonometry), and the difference grows with the steps; measured, the worst is ~3e-3 after 200
    steps and the 95th percentile below 1e-5. An error in the model moves every particle.

This check found a runtime bug on its first run: sprt::dtoa dropped the zeros right after the
decimal point (1.0278 printed as 1.278), so a few particles' fields went through JSON wrong and
agreed between GPU and CPU while disagreeing with physics. tests/stappler json-git pins it now.

The step count comes from the snapshot's own report, cycle * framesInGen + cycleFrame, never from
the frames asked for: the headless clock is real time and a late frame drops steps.

The image is checked by features only: particles are visible over every preset, the flipbook shows
many frames of its sheet (each frame is a solid color), fire turns redder with age.

A second, short run sets XL_PARTICLE_FEEDBACK=1 and checks the counters of the feedback pipeline.

    tests/window/particles-check.py [path-to-particles]

With no argument it expects the debug x86_64-linux build of examples/window/particles. It starts its
own app instances, runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import os, subprocess, sys, time, zlib, struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests/parity"))
import xlclient  # noqa: E402  (the inspector protocol, shared with the parity harness)

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-particles-check.sock")
WIDTH, HEIGHT = 1440, 900
SEED = 4242
PRESETS = ["fire", "smoke", "fountain", "snow", "explosion", "vortex", "flipbook"]

# The scene area, clear of the panel (right) and of the frame statistics overlay (bottom left)
SCENE = (0, 40, 960, 780)

# ParticleTextures.cpp, s_frameSheetColors
FRAME_SHEET = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255),
        (255, 128, 0), (128, 0, 255), (0, 128, 255), (255, 0, 128), (128, 255, 0), (0, 255, 128),
        (128, 128, 255), (255, 128, 128), (128, 255, 128), (255, 255, 255)]

# Channel offsets of the raw screenshot formats (sprt::window ImageFormat): RGBA, BGRA, else PNG
RAW_ORDER = {37: (0, 1, 2), 43: (0, 1, 2), 44: (2, 1, 0), 50: (2, 1, 0)}

EXACT = ("rng", "currentLifetime", "fullLifetime", "index")

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


class App:
    def __init__(self, binary, env_extra=None):
        env = dict(os.environ)
        env.update(env_extra or {})
        env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
        try:
            os.unlink(ADDR)
        except OSError:
            pass
        self.proc = subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height",
                str(HEIGHT)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.session = None
        for _ in range(600):
            if self.proc.poll() is not None:
                break
            if os.path.exists(ADDR):
                try:
                    self.session = xlclient.Session("unix:" + ADDR, 60)
                    break
                except OSError:
                    pass
            time.sleep(0.05)
        if not self.session:
            self.proc.kill()
            raise SystemExit("app did not come up")

    def invoke(self, command, **args):
        return self.session.call("invoke", name="particles." + command, args=args)

    def feedback(self):
        return self.invoke("stats")["emitters"][0]["feedback"]

    def screenshot(self):
        """(width, height, rows of (r, g, b))"""
        shot = self.session.call("screenshot", format="raw")
        data = shot["data"]
        if isinstance(data, str):
            data = xlclient.decode_bytes(data)
        order = RAW_ORDER.get(shot.get("pixelFormat"))
        if order is None:
            return decode_png(xlclient.decode_bytes(self.session.call("screenshot")["data"]))
        width, height = shot["width"], shot["height"]
        r, g, b = order
        rows = []
        for y in range(height):
            line = data[y * width * 4:(y + 1) * width * 4]
            rows.append(list(zip(line[r::4], line[g::4], line[b::4])))
        return width, height, rows

    def close(self):
        try:
            self.session.call("quit", graceful=True)
            self.session.close()
            self.proc.wait(timeout=15)
        except (OSError, subprocess.TimeoutExpired):
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def decode_png(data):
    """(width, height, rows of (r, g, b)) of an 8-bit RGB or RGBA PNG"""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("screenshot is not a PNG")
    pos, idat, width, height, bpp = 8, b"", 0, 0, 4
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, ctype = struct.unpack(">IIBB", body[:10])
            bpp = {2: 3, 6: 4}[ctype]
        elif tag == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * bpp
    prev = bytearray(stride)
    rows = []
    i = 0
    for _ in range(height):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x:x + 3]) for x in range(0, stride, bpp)])
        prev = line
    return width, height, rows


def particle_pixels(image, box=SCENE):
    """Pixels of the box that differ from its most common color (the background)"""
    _, _, rows = image
    x0, y0, x1, y1 = box
    counts = {}
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            counts[rows[y][x]] = counts.get(rows[y][x], 0) + 1
    bg = max(counts, key=counts.get)
    ret = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            c = rows[y][x]
            if abs(c[0] - bg[0]) + abs(c[1] - bg[1]) + abs(c[2] - bg[2]) > 60:
                ret.append((x, y, c))
    return ret


def steps_of(feedback):
    return feedback["cycle"] * feedback["framesInGen"] + feedback["cycleFrame"]


def restart_and_wait(app, min_steps):
    """Restart with the fixed seed and wait for a report of the new restart past `min_steps`"""
    app.invoke("seed", n=SEED)
    stats = app.invoke("restart")
    generation = stats["emitters"][0]["restartGeneration"]
    deadline = time.time() + 30
    while time.time() < deadline:
        f = app.feedback()
        if f["restartGeneration"] == generation and f["sequence"] > 0 and steps_of(f) >= min_steps:
            return f
        time.sleep(0.05)
    raise SystemExit(f"no report of restart {generation} past {min_steps} steps")


def flat(v):
    return v if isinstance(v, list) else [v]


def compare(name, snapshot):
    if "reference" not in snapshot:
        check(f"{name}: the reference is computed", False, snapshot.get("referenceError", ""))
        return
    gpu, cpu = snapshot["particles"], snapshot["reference"]
    exact_bad, errors, worst = [], [], (0.0, None)
    for p, q in zip(gpu, cpu):
        for key in p:
            if key in EXACT:
                if p[key] != q[key]:
                    exact_bad.append((p["index"], key, p[key], q[key]))
                continue
            for a, b in zip(flat(p[key]), flat(q[key])):
                d = abs(a - b) / max(1.0, abs(b))
                errors.append(d)
                if d > worst[0]:
                    worst = (d, (p["index"], key, a, b))
    errors.sort()
    p95 = errors[int(len(errors) * 0.95)] if errors else 0.0
    steps = snapshot["steps"]
    check(f"{name}: {len(gpu)} particles after {steps} steps, integers equal the reference",
            len(gpu) == len(cpu) and not exact_bad, f"{len(exact_bad)} differ, first {exact_bad[:3]}")
    check(f"{name}: floats within the tolerance (p95 {p95:.1e}, worst {worst[0]:.1e})",
            p95 <= 1e-3 and worst[0] <= 1e-2, f"worst {worst[1]}")


def check_reference(app, name, min_steps):
    restart_and_wait(app, min_steps)
    count = app.invoke("stats")["emitters"][0]["system"]["count"]
    snapshot = app.invoke("snapshot", n=count, reference=True)
    check(f"{name}: snapshot taken", snapshot.get("ok"), snapshot.get("reason", ""))
    if snapshot.get("ok"):
        compare(name, snapshot)
    return snapshot


def reference_run(binary):
    app = App(binary)
    try:
        app.session.call("frame", count=5)
        app.invoke("emitters", n=1)
        app.invoke("locale", name="en-us")

        for preset in PRESETS:
            app.invoke("preset", name=preset)
            app.invoke("seed", n=SEED)
            framesInGen = app.feedback()["framesInGen"] or 60
            # past the first cycle where it is short enough, so a second cycle's phases are used
            snapshot = check_reference(app, preset, min(max(framesInGen, 60) + 20, 200))

            pixels = particle_pixels(app.screenshot())
            check(f"{preset}: particles are visible", len(pixels) > 500, f"{len(pixels)} pixels")

            if preset == "flipbook":
                image = app.screenshot()
                seen = [0] * len(FRAME_SHEET)
                for _, _, c in particle_pixels(image):
                    for k, f in enumerate(FRAME_SHEET):
                        if abs(c[0] - f[0]) <= 3 and abs(c[1] - f[1]) <= 3 and abs(c[2] - f[2]) <= 3:
                            seen[k] += 1
                            break
                frames = sum(1 for n in seen if n > 20)
                check(f"flipbook: many frames of the sheet are drawn ({frames} of 16)", frames >= 8,
                        str(seen))

            if preset == "fire":
                check_fire_colors(app)

            if preset == "vortex":
                check_everything(app)

        # The same seed, the same particles at the same step of two restarts
        app.invoke("preset", name="fountain")
        runs = []
        for _ in range(2):
            restart_and_wait(app, 1)
            got = {}
            deadline = time.time() + 1.5
            while time.time() < deadline:
                s = app.invoke("snapshot", n=32)
                if s.get("ok"):
                    got.setdefault(steps_of(s["feedback"]), s["particles"])
            runs.append(got)
        common = sorted(set(runs[0]) & set(runs[1]))
        check(f"a restart with the same seed repeats the particles ({len(common)} common steps)",
                len(common) > 0 and all(runs[0][k] == runs[1][k] for k in common))
    finally:
        app.close()


def check_fire_colors(app):
    """Fire's color curve goes from yellow to red: young particles near the base are yellower"""
    app.invoke("move", x=480, y=240)
    restart_and_wait(app, 120)
    pixels = particle_pixels(app.screenshot())
    base_y = HEIGHT - 240  # the node in layout coordinates, the scene is the whole height

    def ratio(lo, hi):
        sel = [c for x, y, c in pixels if base_y - hi <= y < base_y - lo]
        r = sum(c[0] for c in sel)
        g = sum(c[1] for c in sel)
        return (g / r if r else 0.0), len(sel)

    young, young_n = ratio(-10, 40)
    old, old_n = ratio(110, 220)
    check(f"fire: yellower at the base than at the top (G/R {young:.2f} vs {old:.2f})",
            young_n > 200 and old_n > 50 and young > old + 0.1, f"pixels {young_n}, {old_n}")


def check_everything(app):
    """Every parameter the presets leave at zero, and every flag, over vortex"""
    patch = {
        "hue": {"min": -0.2, "max": 0.3},
        "radialAcceleration": {"min": 10.0, "max": 30.0},
        "tangentialAcceleration": {"min": -20.0, "max": 40.0},
        "acceleration": {"min": -15.0, "max": 5.0},
        "angle": {"min": 0.0, "max": 3.0},
        "angularVelocity": {"min": -2.0, "max": 2.0},
        "velocity": {"min": 10.0, "max": 40.0},
        "linearVelocity": {"min": [5.0, -5.0], "max": [10.0, 5.0]},
        "linearAcceleration": {"min": [0.0, -30.0], "max": [5.0, -10.0]},
        "lifetime": {"min": 1.5, "max": 2.5},
        "explosiveness": 0.3,
        "origin": [30.0, -20.0],
        "flags": ["useLifetimeMax", "orderByLifetime", "alignWithVelocity"],
    }
    app.invoke("set", **patch)
    check_reference(app, "everything, scene coordinates", 200)

    app.invoke("move", x=300, y=420)
    app.invoke("set", flags=["localCoords", "useLifetimeMax", "orderByLifetime"])
    check_reference(app, "everything, local coordinates, moved node", 200)


def feedback_run(binary):
    app = App(binary, {"XL_PARTICLE_FEEDBACK": "1"})
    try:
        app.session.call("frame", count=5)
        app.invoke("emitters", n=1)
        app.invoke("preset", name="explosion")
        restart_and_wait(app, 20)

        count = app.invoke("stats")["emitters"][0]["system"]["count"]
        agree, total = 0, 0
        for _ in range(5):
            s = app.invoke("snapshot", n=count, reference=True)
            if not s.get("ok") or "reference" not in s:
                continue
            total += 1
            alive = sum(1 for p in s["particles"] if p["currentLifetime"] > 0)
            reference = sum(1 for p in s["reference"] if p["currentLifetime"] > 0)
            if s["feedback"]["counters"] and s["feedback"]["alive"] == alive == reference:
                agree += 1
        check(f"feedback: alive equals the snapshot and the reference ({agree}/{total})",
                total > 0 and agree == total)

        previous = app.feedback()["totalBirths"]
        whole, grew = True, False
        for _ in range(60):
            time.sleep(0.05)
            current = app.feedback()["totalBirths"]
            whole = whole and (current - previous) % count == 0
            grew = grew or current > previous
            previous = current
        check("feedback: explosion births arrive in whole bursts of count", whole and grew)
    finally:
        app.close()


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT,
        "examples/window/particles/stappler-build/x86_64-unknown-linux-gnu/debug/cc/particles")
if not os.path.exists(binary):
    raise SystemExit(f"{binary} is not built: xenolith-cli build examples/window/particles")

started = time.time()
reference_run(binary)
feedback_run(binary)
print(f"\n{checks} checks, {failures} failures ({time.time() - started:.0f} s)")
sys.exit(1 if failures else 0)
