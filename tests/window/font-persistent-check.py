#!/usr/bin/env python3
"""Persistent glyphs spread over many blocks of the Vulkan font queue (XL_FONT_PERSISTENT_TEST).

A label with persistent glyph data keeps its bitmaps in the queue's persistent buffer, and every
font input after it copies them from there into the new atlas. The buffer is made of blocks sized
from the device-local heap's budget; when the last one has no room for an input's new glyphs, the
queue starts another. The stand shows its labels one at a time, each of a size and run of
characters of its own, so each step is an input with new persistent glyphs:

  small    XL_FONT_PERSISTENT_BLOCK=16384: nearly every step overflows the last block and starts
           a new one, and each input after copies the old glyphs from all of them. Every step's
           glyphs reach the atlas, no allocation fails, and the blocks are many;
  budget   the block the budget gives: one block holds them all;
  copy     small blocks again, the glyphs drawn into process memory and copied into the staging
           buffer (XL_FONT_STAGING_COPY=1), as when the buffer is not mapped;
  picture  the last frame is the same in all three runs, pixel for pixel;
  refused  a block no heap can place (XL_FONT_PERSISTENT_BLOCK of 1 TiB): the persistent buffer is
           refused, the input with it, and the app goes on drawing frames; through seconds of
           frames the refused set is resent a few times, with growing delays, not every frame;
  recover  the first blocks refused (XL_FONT_PERSISTENT_REFUSE), as by a heap that frees later:
           a resend after the delay places them and the glyphs reach the atlas.

    tests/window/font-persistent-check.py [--steps N] [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, re, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("damage_check",
        os.path.join(_here, "damage-check.py"))
dc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dc)

SMALL_BLOCK = 16 * 1024
STEPS = 32  # FontPersistentLayout::MaxLabels
WIDTH, HEIGHT = 1280, 900

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


def start_app(binary, sock, log, block, copy=False, refuse=0):
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    env["XL_FONT_PERSISTENT_TEST"] = "1"
    env["XL_HIDE_FPS"] = "1"
    env.pop("XL_FONT_PERSISTENT_BLOCK", None)
    env["XL_FONT_STAGING_COPY"] = "1" if copy else "0"
    env["XL_FONT_PERSISTENT_REFUSE"] = str(refuse)
    if block:
        env["XL_FONT_PERSISTENT_BLOCK"] = str(block)
    try:
        os.unlink(sock)
    except OSError:
        pass
    proc = dc.subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height",
            str(HEIGHT), "--gapi", "vulkan"], env=env,
            cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(log, "w"), stderr=dc.subprocess.STDOUT)
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


def invoke(s, name, **args):
    return s.ok("invoke", name=name, args=args) or {}


def settle(s, count, timeout=30.0):
    """Frames until the first `count` labels are laid out and the atlas holds all their glyphs."""
    deadline = time.monotonic() + timeout
    st = {}
    while time.monotonic() < deadline:
        s.ok("frame", count=2)
        st = invoke(s, "font-persistent.state")
        if st.get("laidOut") == count and st.get("chars", 0) > 0 and st.get("missing") == 0:
            return True, st
        time.sleep(0.05)
    return False, st


def run(binary, tag, block, steps, copy=False, wait=True, hold=0.0, refuse=0):
    sock = f"/tmp/xl-font-persistent-{tag}-{os.getpid()}.sock"
    log = f"/tmp/xl-font-persistent-{tag}-{os.getpid()}.log"
    proc, s = start_app(binary, sock, log, block, copy, refuse)
    ret = {"log": log, "settled": 0, "failed": [], "frames": 0}
    try:
        for k in range(1, steps + 1):
            invoke(s, "font-persistent.show", count=k)
            if not wait:
                s.ok("frame", count=8)
                continue
            ok, st = settle(s, k)
            if ok:
                ret["settled"] += 1
            else:
                ret["failed"].append((k, st))
        # Frames with nothing new to lay out: what a refused set costs while it stays refused.
        deadline = time.monotonic() + hold
        while time.monotonic() < deadline:
            s.ok("frame", count=2)
            ret["frames"] += 2
            time.sleep(0.02)
        ret["state"] = invoke(s, "font-persistent.state")
        s.ok("frame", count=4)
        time.sleep(0.2)
        # A refused input fails its frames, so then there may be no picture to take.
        if wait:
            ret["shot"] = dc.shot_bytes(s)
        ret["alive"] = proc.poll() is None
    except OSError as e:
        # The app stopped answering: the checks below judge what its log shows.
        ret["alive"] = False
        print(f"       the app stopped answering: {e}")
    finally:
        s.close()
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except dc.subprocess.TimeoutExpired:
            proc.kill()
        try:
            os.unlink(sock)
        except OSError:
            pass
    text = open(log, errors="replace").read()
    ret["blocks"] = [int(m.group(1)) for m in re.finditer(r"persistent glyph block (\d+): (\d+) bytes",
            text)]
    ret["sizes"] = [int(m.group(2)) for m in re.finditer(r"persistent glyph block (\d+): (\d+) bytes",
            text)]
    ret["errors"] = [l for l in text.splitlines() if "FontAttachmentHandle" in l
            and ("failed" in l or "Not enough" in l or "measured" in l or "exceeds" in l)]
    ret["refusals"] = text.count("Font persistent allocation failed")
    ret["delays"] = [int(m.group(1)) for m in re.finditer(r"atlas batch failed \(\d+ in a row\), "
            r"resent in (\d+) ms", text)]
    return ret


def main():
    args = sys.argv[1:]
    steps = STEPS
    if "--steps" in args:
        i = args.index("--steps")
        steps = int(args[i + 1])
        del args[i:i + 2]
    binary = args[0] if args else os.path.join(_here,
            "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

    print(f"\n-- small blocks ({SMALL_BLOCK} bytes), {steps} steps")
    small = run(binary, "small", SMALL_BLOCK, steps)
    check("every step's glyphs reach the atlas", small["settled"] == steps,
            f"{small['failed'][:3]}, see {small['log']}")
    check("no glyph allocation or placement fails", not small["errors"],
            "\n".join(small["errors"][:5]))
    check("the persistent glyphs spread over many blocks", len(small["blocks"]) >= steps // 2,
            f"{len(small['blocks'])} blocks, see {small['log']}")
    check("each block is the forced size or the input that did not fit it",
            all(sz >= SMALL_BLOCK for sz in small["sizes"]), str(small["sizes"][:8]))
    print(f"       {len(small['blocks'])} blocks, {small.get('state', {}).get('chars')} glyphs shown")

    print(f"\n-- blocks of the budget, {steps} steps")
    budget = run(binary, "budget", 0, steps)
    check("every step's glyphs reach the atlas", budget["settled"] == steps,
            f"{budget['failed'][:3]}, see {budget['log']}")
    check("no glyph allocation or placement fails", not budget["errors"],
            "\n".join(budget["errors"][:5]))
    check("one block of the budget holds them all", len(budget["blocks"]) == 1,
            f"{budget['sizes']}, see {budget['log']}")
    check("the budget's block is within 1 and 32 MiB",
            all(1 << 20 <= sz <= 32 << 20 for sz in budget["sizes"]), str(budget["sizes"]))

    print(f"\n-- small blocks, the staging buffer written by copy, {steps} steps")
    copied = run(binary, "copy", SMALL_BLOCK, steps, True)
    check("every step's glyphs reach the atlas", copied["settled"] == steps,
            f"{copied['failed'][:3]}, see {copied['log']}")
    check("no glyph allocation or placement fails", not copied["errors"],
            "\n".join(copied["errors"][:5]))

    print("\n-- the picture")
    a, b = small.get("shot"), budget.get("shot")
    same = a is not None and a == b
    diff = -1
    if a is not None and b is not None and len(a) == len(b):
        diff = sum(1 for i in range(0, len(a), 3) if a[i:i + 3] != b[i:i + 3])
    ink = sum(1 for i in range(0, len(b or b""), 3) if b[i] < 128 and b[i + 1] < 128 and b[i + 2] < 128)
    check("the picture shows the glyphs", ink > 20000, f"{ink} dark pixels")
    check("many blocks draw what one block draws", same, f"{diff} pixels differ")
    check("the copied staging buffer draws the same", copied.get("shot") == b)

    print("\n-- a persistent block no heap can place")
    refused = run(binary, "refused", 1 << 40, 4, wait=False, hold=4.0)
    text = open(refused["log"], errors="replace").read()
    check("the persistent buffer is refused", refused["refusals"] > 0, f"see {refused['log']}")
    check("and the app goes on running frames", refused.get("alive")
            and refused.get("state", {}).get("shown") == 4, str(refused.get("state")))
    check("without a fault", not re.search(r"Segmentation|SIGSEGV|SIGABRT|Assertion", text),
            f"see {refused['log']}")
    # Four inputs with new glyphs, then resends at 0.25, 0.5, 1, 2, 4 s: a dozen at most, against
    # one a frame (hundreds) when a refusal is resent on the next update.
    check("the refused set is not resent every frame", refused["refusals"] <= 12,
            f"{refused['refusals']} refusals over {refused['frames']} frames")
    check("each resend waits longer than the one before",
            len(refused["delays"]) >= 3 and refused["delays"][-1] > refused["delays"][0],
            str(refused["delays"]))
    print(f"       {refused['refusals']} refusals over {refused['frames']} frames, delays "
            f"{refused['delays']} ms")

    print("\n-- the first three blocks refused, then placed")
    recover = run(binary, "recover", 0, 4, refuse=3)
    check("every step's glyphs reach the atlas", recover["settled"] == 4,
            f"{recover['failed'][:3]}, see {recover['log']}")
    check("the three refusals were resent after a delay", recover["refusals"] == 3
            and len(recover["delays"]) == 3, f"{recover['refusals']} refusals, delays "
            f"{recover['delays']}, see {recover['log']}")

    print(f"\n{checks} checks, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
