#!/usr/bin/env python3
"""Drive the hit-test stand (XL_HIT_TEST_TEST) over the inspector socket.

One registry answers "what is under this point" for every subsystem that needs to know - drag
targets, context menus, tooltips - and a node gets into it by being DRAWN, from inside its own
visit. Every rule that follows from that is invisible on screen, which is why it is checked here
rather than by a screenshot:

  * registration order is paint order, so the walk runs backwards and the TOP node answers;
  * an invisible node is not registered at all. Absence, not a flag on a record, is what makes it
    silent - and the same mechanism covers a detached or display:none subtree for free;
  * the box a record carries is only a fast reject: a square turned 45 degrees is a MISS in the
    corners of its own bounding box. The per-target rosters this registry replaces answered on the
    box and got that wrong;
  * a node drawn under a scissor cannot be hit where the scissor cut it away, which is a fact about
    the frame and not about the node;
  * the flag mask separates the tenants, so a query for one kind of target does not find another;
  * the padding belongs to the ASKER: the same record answers differently to two questions about
    the same point.

    tests/window/hit-test-check.py [path-to-testapp]

Prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

# HitTestLayout::FlagA / FlagB, application bits. Duplicated on purpose: a check that reads its
# expectations out of the thing it is checking cannot fail.
FLAG_A = 1 << 16
FLAG_B = 1 << 17


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

    def invoke(self, name, **args):
        return self.ok("invoke", name=name, args=args)

    def close(self):
        self.s.close()


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


def start_app(binary, addr):
    env = dict(os.environ)
    env["XL_HIT_TEST_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(addr):
            try:
                Session(addr).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


class Stand:
    def __init__(self, binary, addr):
        self.proc = start_app(binary, addr)
        self.s = Session(addr)
        self.step(3)

    def step(self, n=3):
        self.s.ok("frame", count=n)
        time.sleep(0.05)

    def registry(self):
        return self.s.invoke("hit-test.registry")

    def rects(self):
        return self.s.invoke("hit-test.rects")

    def query(self, x, y, flags=None, padding=0.0):
        args = {"x": x, "y": y, "padding": padding}
        if flags is not None:
            args["flags"] = flags
        return self.s.invoke("hit-test.query", **args)["hits"] or []

    def close(self):
        try:
            self.s.ok("quit")
        except Exception:
            pass
        self.s.close()
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()


def centre(r, fx=0.5, fy=0.5):
    return r["x"] + r["width"] * fx, r["y"] + r["height"] * fy


binary = sys.argv[1] if len(sys.argv) > 1 else \
        "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp"
addr = f"/tmp/xl-hit-test-{os.getpid()}.sock"

app = Stand(binary, addr)
try:
    reg = app.registry()
    names = [r["name"] for r in reg["records"]]
    rects = app.rects()

    print("what a frame registered")
    check("the base is in the registry", "region" in names, names)
    check("and every node that declared a flag", set(names) >=
            {"region", "under", "over", "rotated", "tagged", "pad", "clipped"}, names)
    check("an invisible node is simply absent", "hidden" not in names, names)
    check("and so is a node that declared nothing", "clip" not in names, names)
    check("the frame's mask carries both application flags",
            reg["mask"] & (FLAG_A | FLAG_B) == FLAG_A | FLAG_B, hex(reg["mask"]))
    check("the base was registered before its children - paint order, walked backwards",
            names.index("region") > names.index("under"), names)

    print("the topmost answers")
    x, y = centre(rects["over"])
    hits = app.query(x, y)
    check("a point on two stacked nodes is answered by the upper one",
            hits[:1] == ["over"], hits)
    check("and the one below is still a candidate under it", "under" in hits, hits)
    check("as is the base under both", "region" in hits, hits)

    x, y = centre(rects["under"], 0.15, 0.2)
    hits = app.query(x, y)
    check("a point on only the lower one answers with it", hits[:1] == ["under"], hits)
    check("and not with its neighbour", "over" not in hits, hits)

    print("a rotated node is not its bounding box")
    x, y = centre(rects["rotated"])
    check("the centre of a turned square is on it", app.query(x, y)[:1] == ["rotated"],
            app.query(x, y))
    r = rects["rotated"]
    check("its registered box grew by the rotation", r["width"] > 160.0, r["width"])
    corner = (r["x"] + 8.0, r["y"] + 8.0)
    hits = app.query(*corner)
    check("but the corner of that box is a miss", "rotated" not in hits, hits)
    check("and the point falls through to what is under it", "region" in hits, hits)

    print("what a scissor cut away cannot be hit")
    clip = rects["clip"]
    x, y = centre(clip)
    check("the child answers inside the clip", app.query(x, y)[:1] == ["clipped"],
            app.query(x, y))
    outside = (clip["x"] + clip["width"] + 60.0, clip["y"] + clip["height"] / 2.0)
    hits = app.query(*outside)
    check("the same child does not answer past the clip's edge", "clipped" not in hits, hits)
    check("though the point is inside the child's own box",
            outside[0] < rects["clipped"]["x"] + rects["clipped"]["width"], outside)
    check("and the base answers there instead", "region" in hits, hits)

    print("the mask separates the tenants")
    x, y = centre(rects["tagged"])
    check("a node registered under another flag is found by its own",
            app.query(x, y, flags=FLAG_B) == ["tagged"], app.query(x, y, flags=FLAG_B))
    hits = app.query(x, y, flags=FLAG_A)
    check("and not by a query for a different one", "tagged" not in hits, hits)
    check("which still finds what is under it", hits[:1] == ["region"], hits)

    print("the padding belongs to whoever asks")
    pad = rects["pad"]
    just_outside = (pad["x"] - 6.0, pad["y"] + pad["height"] / 2.0)
    check("a point beside a node is not on it", "pad" not in app.query(*just_outside),
            app.query(*just_outside))
    check("the same point, asked with a padding, is", 
            app.query(*just_outside, padding=12.0)[:1] == ["pad"],
            app.query(*just_outside, padding=12.0))
    check("and a padding does not reach past what it was given",
            "pad" not in app.query(pad["x"] - 20.0, pad["y"] + pad["height"] / 2.0, padding=12.0),
            app.query(pad["x"] - 20.0, pad["y"] + pad["height"] / 2.0, padding=12.0))

    print("a point on nothing")
    empty = (rects["region"]["x"] - 30.0, rects["region"]["y"] + 10.0)
    check("outside every registered node the answer is empty", app.query(*empty) == [],
            app.query(*empty))

    print("the registry is rebuilt every frame")
    gen = app.registry()["generation"]
    app.step(2)
    check("and says which frame it is", app.registry()["generation"] > gen, gen)
    check("with the same contents when nothing moved",
            [r["name"] for r in app.registry()["records"]] == names, names)
finally:
    app.close()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
