#!/usr/bin/env python3
"""A label's damage box in both modes of basic2d::Label::setBoundsFromLayout, and what each costs.

On (the default) the box is the layout box padded by the tallest line, and the quads are not read;
off, it is read from the quads, each glyph placed through the font atlas - the way a server always
measures a remote client's labels. Two parts:

  * the label-fill stand (XL_LABEL_FILL_TEST): for grids of labels of several lengths,
    `label-fill.bench` writes every label's quads again - what a label pays on each change - and
    reads the box as damage tracking does, in both modes. Printed per label and per character: the
    fill, the box from the quads, the box from the layout, and how much larger the layout box is.
    Checked: the layout box holds every glyph the quads place, and every glyph is in the atlas when
    the numbers are taken. The times are printed, never asserted - they mean something only in a
    release build on a quiet machine (docs/agents/measuring-frames.md);
  * damage-check.py whole, with the layout mode off (XL_LABEL_LAYOUT_BOUNDS=0): no trail, and a
    label's colour change is a partial repaint of its own that leaves the picture a full redraw
    gives. damage-check.py alone covers the default.

    tests/window/label-bounds-check.py [--gapi vulkan|soft] [--iterations N] [path-to-testapp]

Times come from a release testapp; the damage part reads the backends' damage log, which only a
debug build writes, so it fails on a release one.

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, subprocess, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("damage_check",
        os.path.join(_here, "damage-check.py"))
dc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dc)

# (labels, characters each): short captions, paragraphs, a few long texts
GRIDS = [(24, 40), (24, 160), (8, 1200)]

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
    env["XL_LABEL_FILL_TEST"] = "1"
    env["XL_HIDE_FPS"] = "1"
    try:
        os.unlink(sock)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1280", "--height", "900",
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


def ready_bench(s, count, iterations, timeout=30.0):
    """Step frames until every label is shaped and every glyph is in the atlas, then bench."""
    deadline = time.monotonic() + timeout
    r = {}
    while time.monotonic() < deadline:
        s.ok("frame", count=2)
        time.sleep(0.2)
        r = s.ok("invoke", name="label-fill.bench", args={"iterations": 1}) or {}
        if r.get("labels") == count and r.get("unresolved") == 0 and r.get("chars", 0) > 0:
            return s.ok("invoke", name="label-fill.bench", args={"iterations": iterations}) or {}
    return r


def bench(binary, gapi, iterations):
    pid = os.getpid()
    sock = f"/tmp/xl-label-bounds-check-{gapi}-{pid}.sock"
    log = f"/tmp/xl-label-bounds-check-{gapi}-{pid}.log"
    print(f"--- label fill, gapi: {gapi}")
    proc, s = start_app(binary, sock, log, gapi)
    rows = []
    try:
        for count, length in GRIDS:
            s.ok("invoke", name="label-fill.fill", args={"count": count, "length": length})
            r = ready_bench(s, count, iterations)
            if "error" in r:
                check(f"{count} x {length}: bench", False, r["error"])
                continue
            what = f"{count} labels x {length} chars"
            check(f"{what}: every label is shaped and every glyph is in the atlas",
                    r.get("labels") == count and r.get("unresolved") == 0,
                    f"{r.get('labels')} labels, {r.get('unresolved')} with glyphs missing")
            check(f"{what}: the layout box holds every glyph", r.get("notContained") == 0,
                    f"{r.get('notContained')} labels spill out of it")
            rows.append((count, length, r))
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

    print()
    print("  per label (ns)              fill   box:quads  box:layout   quads/fill"
          "   ns/char quads   area layout/quads")
    for count, length, r in rows:
        n = r["labels"] * r["iterations"]
        chars = r["chars"] / r["labels"]
        fill_a = r["atlas"]["fillNs"] / n
        box_a = r["atlas"]["boundsNs"] / n
        fill_l = r["layout"]["fillNs"] / n
        box_l = r["layout"]["boundsNs"] / n
        fill = (fill_a + fill_l) / 2
        area = r["layoutArea"] / r["atlasArea"] if r["atlasArea"] else 0.0
        print(f"  {count:3} x {length:5} ({chars:6.0f} ch) {fill:10.0f} {box_a:10.0f} "
              f"{box_l:11.0f} {100.0 * box_a / fill:10.1f} % {box_a / chars:12.1f}"
              f" {area:13.2f}x")
    print("  (debug-build times are for scale only; take real ones from a release build)")
    print()

    text = open(log, errors="replace").read()
    check("no Vulkan validation error", "Validation Error" not in text, log)


def main():
    gapis = ["vulkan"]
    iterations = 20
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
        elif opt == "--iterations":
            iterations = int(value)
        else:
            raise SystemExit(f"unknown option: {opt}")

    binary = os.path.join(_here, "stappler-build", "x86_64-unknown-linux-gnu", "debug", "cc",
            "testapp")
    if argv:
        binary = os.path.abspath(argv[0])
    if not os.path.exists(binary):
        raise SystemExit(f"missing binary: {binary}\nbuild it: xenolith-cli build tests/window")

    global checks, failures
    for gapi in gapis:
        bench(binary, gapi, iterations)

        print(f"--- damage-check with the box from the quads, gapi: {gapi}")
        os.environ["XL_LABEL_LAYOUT_BOUNDS"] = "0"
        try:
            dc.run(binary, gapi)
        finally:
            del os.environ["XL_LABEL_LAYOUT_BOUNDS"]
    checks += dc.checks
    failures += dc.failures

    print(f"{checks} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
