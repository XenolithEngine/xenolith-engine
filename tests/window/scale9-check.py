#!/usr/bin/env python3
"""Drive the basic2d::Scale9Sprite stand (XL_SCALE9_TEST) over the inspector socket.

A nine-slice sprite makes claims that are numbers, not pictures. "The corner did not stretch" is
the statement that one quad is 20 by 12 at every content size; "the middle repeats" is the
statement that nine texture rects tile the fragment with no gap and no overlap. A screenshot cannot
tell a corner stretched by 4% from one that was not, and comparing PNGs would be checking the
rasterizer rather than the slicing. So this reads the vertices the sprite actually wrote, through
Scale9Probe in the stand.

The three sprites and what each one is for:

  * `full`   the whole texture, sliced on all four sides - the reference;
  * `atlas`  the same four numbers over a SUB-RECT of that texture. The slice is measured in pixels
             of the FRAGMENT, so the view geometry must come out identical and the texture
             coordinates must not - the one difference that disappears if the code normalizes
             against the sub-rect instead of against the texture;
  * `zero`   one side left at zero: that column must not be emitted at all.

    tests/window/scale9-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-scale9-check.sock")

# What the stand declares (Scale9Layout.h). Duplicated on purpose: a check that reads its
# expectations out of the thing it is checking cannot fail.
TEX_W, TEX_H = 128.0, 96.0
SIZE_W, SIZE_H = 300.0, 200.0
TOP, RIGHT, BOTTOM, LEFT = 12.0, 16.0, 8.0, 20.0

# The sub-rect the `atlas` sprite looks into, normalized.
ATLAS_X, ATLAS_Y, ATLAS_W, ATLAS_H = 0.25, 0.125, 0.5, 0.5

# The stand reports view coordinates in hundredths and texture coordinates in millionths.
VIEW = 100.0
TEXTURE = 1_000_000.0


class Session:
    def __init__(self, path=ADDR, timeout=20.0):
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


def start_app(binary):
    env = dict(os.environ)
    env["XL_SCALE9_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(ADDR):
            try:
                s = Session()
                s.close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


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


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()


def step(n=2):
    # The headless window renders on demand, and a sprite rebuilds its vertices while it draws. So
    # everything asserted here is a frame away from the command that caused it.
    s.ok("frame", count=n)
    time.sleep(0.15)


def state():
    return s.invoke("scale9.state", settle=0.0)


def sprite(name):
    return state()[name]


def pieces(name):
    # Sorted top row first, left to right - the order a person would read them in, derived here
    # rather than taken on faith from the order the sprite emitted them.
    ps = sprite(name)["pieces"] or []
    return sorted(ps, key=lambda p: (-p["view"]["y"], p["view"]["x"]))


def wait_for_pieces(seconds=15.0):
    # The texture is compiled asynchronously; until it is loaded the sprite draws nothing at all.
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        step(3)
        st = state()
        if all(v["count"] > 0 for v in st.values()):
            return st
    raise SystemExit("the sprites never drew: the texture did not compile")


def v(x):
    return int(round(x * VIEW))


def t(x):
    return int(round(x * TEXTURE))


def rect_is(r, x, y, w, h, tol=1):
    return (abs(r["x"] - x) <= tol and abs(r["y"] - y) <= tol and abs(r["w"] - w) <= tol
            and abs(r["h"] - h) <= tol)


def tex_is(r, u, vv, uw, vh, tol=2):
    return (abs(r["u"] - u) <= tol and abs(r["v"] - vv) <= tol and abs(r["uw"] - uw) <= tol
            and abs(r["vh"] - vh) <= tol)


try:
    wait_for_pieces()

    print("\n-- 1. the stand is the one these numbers were written for")
    st = state()
    check("texture is the declared size",
            st["full"]["textureWidth"] == int(TEX_W) and st["full"]["textureHeight"] == int(TEX_H),
            f'{st["full"]["textureWidth"]}x{st["full"]["textureHeight"]}')
    check("slice is the declared one",
            st["full"]["slice"] == {"top": v(TOP), "right": v(RIGHT), "bottom": v(BOTTOM),
                "left": v(LEFT)}, str(st["full"]["slice"]))
    check("content is the declared size",
            rect_is(st["full"]["content"], 0, 0, v(SIZE_W), v(SIZE_H)),
            str(st["full"]["content"]))

    print("\n-- 2. nine pieces that tile the sprite, and corners that are not stretched")
    ps = pieces("full")
    check("nine pieces", len(ps) == 9, f"{len(ps)}")

    # The grid the pieces must land on, in the sprite's own coordinates: y grows UP, so the first
    # row read is the top one.
    xs = [0.0, LEFT, SIZE_W - RIGHT, SIZE_W]
    ys = [0.0, BOTTOM, SIZE_H - TOP, SIZE_H]
    # ...and in the texture, where v grows DOWN.
    us = [0.0, LEFT / TEX_W, 1.0 - RIGHT / TEX_W, 1.0]
    vs = [0.0, TOP / TEX_H, 1.0 - BOTTOM / TEX_H, 1.0]

    names = [["top-left", "top", "top-right"], ["left", "middle", "right"],
        ["bottom-left", "bottom", "bottom-right"]]

    def expected_view(row, col):
        return (v(xs[col]), v(ys[2 - row]), v(xs[col + 1] - xs[col]), v(ys[3 - row] - ys[2 - row]))

    def expected_tex(row, col):
        return (t(us[col]), t(vs[row]), t(us[col + 1] - us[col]), t(vs[row + 1] - vs[row]))

    if len(ps) == 9:
        for row in range(3):
            for col in range(3):
                p = ps[row * 3 + col]
                check(f"{names[row][col]}: view rect",
                        rect_is(p["view"], *expected_view(row, col)),
                        f'{p["view"]} != {expected_view(row, col)}')
                check(f"{names[row][col]}: texture rect",
                        tex_is(p["tex"], *expected_tex(row, col)),
                        f'{p["tex"]} != {expected_tex(row, col)}')

        # The pairing itself: the band drawn at the TOP of the sprite must carry the TOP of the
        # picture. Getting this backwards leaves every rect the right size and the frame upside
        # down, which is why it is asserted rather than assumed.
        check("the top view band carries the smallest v",
                ps[0]["view"]["y"] > ps[6]["view"]["y"] and ps[0]["tex"]["v"] < ps[6]["tex"]["v"],
                f'{ps[0]["tex"]["v"]} vs {ps[6]["tex"]["v"]}')

    print("\n-- 3. resizing moves the middle and nothing else")
    for (w, h) in ((500.0, 400.0), (90.0, 250.0)):
        s.invoke("scale9.set-size", target="full", width=w, height=h, settle=0.0)
        step()
        rs = pieces("full")
        check(f"{int(w)}x{int(h)}: still nine pieces", len(rs) == 9, f"{len(rs)}")
        if len(rs) != 9:
            continue

        corners = {0: (LEFT, TOP), 2: (RIGHT, TOP), 6: (LEFT, BOTTOM), 8: (RIGHT, BOTTOM)}
        for idx, (cw, ch) in corners.items():
            check(f"{int(w)}x{int(h)}: {names[idx // 3][idx % 3]} keeps its size",
                    abs(rs[idx]["view"]["w"] - v(cw)) <= 1
                    and abs(rs[idx]["view"]["h"] - v(ch)) <= 1,
                    f'{rs[idx]["view"]}')

        check(f"{int(w)}x{int(h)}: the top edge stretched along x only",
                abs(rs[1]["view"]["w"] - v(w - LEFT - RIGHT)) <= 1
                and abs(rs[1]["view"]["h"] - v(TOP)) <= 1, f'{rs[1]["view"]}')
        check(f"{int(w)}x{int(h)}: the left edge stretched along y only",
                abs(rs[3]["view"]["w"] - v(LEFT)) <= 1
                and abs(rs[3]["view"]["h"] - v(h - TOP - BOTTOM)) <= 1, f'{rs[3]["view"]}')
        check(f"{int(w)}x{int(h)}: the pieces still cover the sprite exactly",
                rs[0]["view"]["x"] == 0 and rs[6]["view"]["y"] == 0
                and rs[2]["view"]["x"] + rs[2]["view"]["w"] == v(w)
                and rs[0]["view"]["y"] + rs[0]["view"]["h"] == v(h),
                f'{rs[0]["view"]} {rs[2]["view"]} {rs[6]["view"]}')
        check(f"{int(w)}x{int(h)}: texture rects did not move",
                all(rs[i]["tex"] == ps[i]["tex"] for i in range(9)),
                "the slice is a property of the picture, not of the box it is drawn in")

    s.invoke("scale9.set-size", target="full", width=SIZE_W, height=SIZE_H, settle=0.0)
    step()

    print("\n-- 4. the slice is measured in pixels of the FRAGMENT")
    ats = pieces("atlas")
    check("atlas: nine pieces", len(ats) == 9, f"{len(ats)}")
    if len(ats) == 9:
        check("atlas: the same view geometry as the whole picture",
                all(ats[i]["view"] == ps[i]["view"] for i in range(9)),
                "the same four numbers over the same box must lay out the same way")

        aus = [ATLAS_X, ATLAS_X + LEFT / TEX_W, ATLAS_X + ATLAS_W - RIGHT / TEX_W,
            ATLAS_X + ATLAS_W]
        avs = [ATLAS_Y, ATLAS_Y + TOP / TEX_H, ATLAS_Y + ATLAS_H - BOTTOM / TEX_H,
            ATLAS_Y + ATLAS_H]
        for row in range(3):
            for col in range(3):
                p = ats[row * 3 + col]
                check(f"atlas: {names[row][col]} texture rect",
                        tex_is(p["tex"], t(aus[col]), t(avs[row]), t(aus[col + 1] - aus[col]),
                                t(avs[row + 1] - avs[row])), f'{p["tex"]}')

        # The claim behind all of that, stated on its own: a border of the same PIXEL width comes
        # out the same width in texture units whichever fragment it belongs to. Scaling the slice
        # with the sub-rect - the plausible wrong answer - would halve it here.
        check("atlas: the border band is as wide as the whole picture's, in texture units",
                ats[0]["tex"]["uw"] == ps[0]["tex"]["uw"]
                and ats[0]["tex"]["vh"] == ps[0]["tex"]["vh"],
                f'{ats[0]["tex"]} vs {ps[0]["tex"]}')
        check("atlas: the middle band is NOT the whole picture's",
                ats[4]["tex"]["uw"] != ps[4]["tex"]["uw"],
                "a smaller fragment has a smaller middle")

    print("\n-- 5. the middle can be left out, and nothing else changes")
    s.invoke("scale9.set-fill-center", target="full", value=False, settle=0.0)
    step()
    ns = pieces("full")
    check("eight pieces without the centre", len(ns) == 8, f"{len(ns)}")
    if len(ns) == 8:
        expected = [p for i, p in enumerate(ps) if i != 4]
        check("the other eight are unchanged, number for number",
                all(ns[i] == expected[i] for i in range(8)), "turning off the centre moved a piece")
    s.invoke("scale9.set-fill-center", target="full", value=True, settle=0.0)
    step()

    print("\n-- 6. a zero side is not a zero-area quad")
    zs = pieces("zero")
    check("six pieces, not nine", len(zs) == 6, f"{len(zs)}")
    check("no piece has zero area", all(p["view"]["w"] > 0 and p["view"]["h"] > 0 for p in zs),
            str([p["view"] for p in zs]))
    if len(zs) == 6:
        # Two columns: the middle (which starts at x = 0, the missing left side) and the right one.
        check("the left column is gone, the rest still tiles",
                zs[0]["view"]["x"] == 0
                and abs(zs[0]["view"]["w"] - v(SIZE_W - RIGHT)) <= 1
                and abs(zs[1]["view"]["w"] - v(RIGHT)) <= 1,
                f'{zs[0]["view"]} {zs[1]["view"]}')
        check("its texture rects tile the picture too",
                zs[0]["tex"]["u"] == 0
                and abs(zs[1]["tex"]["u"] - t(1.0 - RIGHT / TEX_W)) <= 2
                and abs(zs[1]["tex"]["u"] + zs[1]["tex"]["uw"] - t(1.0)) <= 2,
                f'{zs[0]["tex"]} {zs[1]["tex"]}')

    print("\n-- 7. a box smaller than its own corners is layout, not an error")
    s.invoke("scale9.set-size", target="full", width=24.0, height=10.0, settle=0.0)
    step()
    small = pieces("full")
    check("only the corners survive", len(small) == 4, f"{len(small)}")
    check("no negative and no zero size",
            all(p["view"]["w"] > 0 and p["view"]["h"] > 0 for p in small),
            str([p["view"] for p in small]))
    if len(small) == 4:
        k = 24.0 / (LEFT + RIGHT)
        ky = 10.0 / (TOP + BOTTOM)
        check("the corners shrank in proportion",
                abs(small[0]["view"]["w"] - v(LEFT * k)) <= 1
                and abs(small[1]["view"]["w"] - v(RIGHT * k)) <= 1
                and abs(small[0]["view"]["h"] - v(TOP * ky)) <= 1
                and abs(small[2]["view"]["h"] - v(BOTTOM * ky)) <= 1,
                str([p["view"] for p in small]))
        check("they still cover the box exactly",
                small[0]["view"]["w"] + small[1]["view"]["w"] == v(24.0)
                and small[0]["view"]["h"] + small[2]["view"]["h"] == v(10.0),
                str([p["view"] for p in small]))
        check("the texture rects did NOT shrink with them",
                small[0]["tex"] == ps[0]["tex"],
                "a squeezed corner shows the same pixels, compressed")

    s.invoke("scale9.set-size", target="full", width=SIZE_W, height=SIZE_H, settle=0.0)
    step()

    print("\n-- 8. a slice that leaves no middle is refused, and the refusal is not sticky")
    s.invoke("scale9.set-slice", target="full", top=TOP, right=60.0, bottom=BOTTOM, left=80.0,
            settle=0.0)
    step()
    bad = pieces("full")
    check("refused: one quad, not nine", len(bad) == 1, f"{len(bad)}")
    if len(bad) == 1:
        check("refused: it is the plain sprite",
                rect_is(bad[0]["view"], 0, 0, v(SIZE_W), v(SIZE_H))
                and tex_is(bad[0]["tex"], 0, 0, t(1.0), t(1.0)), f"{bad[0]}")

    s.invoke("scale9.set-slice", target="full", top=TOP, right=RIGHT, bottom=BOTTOM, left=LEFT,
            settle=0.0)
    step()
    back = pieces("full")
    check("a usable slice brings the nine pieces back", len(back) == 9, f"{len(back)}")
    if len(back) == 9:
        check("and they are the ones it started with",
                all(back[i] == ps[i] for i in range(9)), "the sprite did not return to its state")

    print("\n-- 9. autofit is refused rather than ignored")
    r = s.invoke("scale9.set-autofit", target="full", value="cover", settle=0.0)
    check("cover is not applied", r["autofit"] == "none", str(r))
    step()
    after = pieces("full")
    check("and the geometry did not move", len(after) == 9 and all(
            after[i] == ps[i] for i in range(9)), f"{len(after)}")

finally:
    try:
        s.ok("quit")
    except Exception:
        pass
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
