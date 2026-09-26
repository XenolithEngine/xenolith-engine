#!/usr/bin/env python3
"""The window manager's compositor end to end: examples/os/server (wmserver) headless, with two
tests/window clients (clientapp) launched by the server itself (XL_WM_APPS).

Stage `basic` (M8, M9), soft or Vulkan backend:

1. launch: two planes, both published; the top one enabled and focused, the other paused and
   minimized;
2. identity: with the clients still, the host frame equals the top plane's published frame;
3. pacing: while the top client animates, its frames never outrun the host's (published <=
   host frames + 1) nor the vblank rate, and the paused plane draws nothing; screenshots of the
   animated plane all arrive while the host keeps composing (on Vulkan a screenshot takes the
   plane's frame alone, so the compositor waits for it);
4. caps: a plane scaled on commit is refused and nothing changes; a plane at alpha 0.5 blends
   over the background;
5. pause and resume: a paused plane is minimized, draws nothing and shows the background;
   resumed, it draws again and the host shows it;
6. kill: a client killed with SIGKILL takes its plane with it, the other one is shown, and the
   server keeps running;
7. quit: the server exits with 0; on Vulkan the log has no validation error.

Without `--gapi` both backends run, Vulkan first.

`--soak SECONDS` runs a long mixed load instead - animation, screenshots, plane alpha, pause and
resume in turn - and checks that the host never stops composing and the server exits cleanly.

    tests/window/wm-compositor-check.py [--stage basic] [--gapi soft|vulkan] [--soak SECONDS]
            [path-to-wmserver] [path-to-clientapp]

Without paths the debug builds of this checkout are used; clientapp is looked for next to the
build of wmserver (same target and build type). Prints "N checks, M failures"; exit status is
the result.
"""
import base64, importlib.util, io, os, signal, subprocess, sys, time

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check

ROOT = os.path.abspath(os.path.join(_here, "..", ".."))
FPS = 60
WIDTH, HEIGHT = 800, 600
SHOTS = 20
SERVER_LOG = f"/tmp/wm-compositor-check-server-{os.getpid()}.log"


def state(s):
    return s.invoke("wm-state") or {}


def plane_by_app(st, label):
    for p in st.get("planes", []):
        if p.get("app") == label:
            return p
    return {}


def wait_state(s, predicate, timeout=20.0):
    deadline = time.monotonic() + timeout
    st = state(s)
    while time.monotonic() < deadline:
        if predicate(st):
            return st
        time.sleep(0.1)
        st = state(s)
    return st


def decode(shot):
    data = (shot or {}).get("data") or ""
    if not (isinstance(data, str) and data.startswith("BASE64:")):
        return None
    raw = data[len("BASE64:"):]
    return Image.open(io.BytesIO(base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4)))) \
        .convert("RGBA")


def max_delta(a, b):
    """Largest difference of one channel between two images of one size; None if sizes differ."""
    if a is None or b is None or a.size != b.size:
        return None
    pa, pb = a.tobytes(), b.tobytes()
    return max((abs(x - y) for x, y in zip(pa, pb)), default=0)


def host_frame(s):
    """One new host frame: the compositor composes what the planes hold now."""
    before = state(s).get("hostFrames", 0)
    s.ok("frame", count=1)
    wait_state(s, lambda st: st.get("hostFrames", 0) > before, timeout=5.0)
    time.sleep(0.1)


def host_shot(s):
    return decode(s.ok("screenshot"))


def plane_shot(s, window):
    return decode(s.ok("screenshot", window=window))


def settle_plane(s, label, quiet=0.8, timeout=10.0):
    """Wait until the plane has published nothing new for `quiet` seconds."""
    last = plane_by_app(state(s), label).get("published", 0)
    since = time.monotonic()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(0.1)
        now = plane_by_app(state(s), label).get("published", 0)
        if now != last:
            last, since = now, time.monotonic()
        elif time.monotonic() - since >= quiet:
            return True
    return False


def client_session(inspector):
    path = inspector[len("unix:"):] if inspector.startswith("unix:") else inspector
    deadline = time.monotonic() + 20.0
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return rc.Session(path)
            except OSError:
                pass
        time.sleep(0.1)
    return None


def pid_of(inspector):
    """The process launched with this inspector address: the server does not report pids."""
    needle = ("XENOLITH_INSPECTOR_ADDRESS=" + inspector).encode()
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open(f"/proc/{entry}/environ", "rb") as f:
                if needle in f.read().split(b"\0"):
                    return int(entry)
        except OSError:
            continue
    return None


def background_rgb(img):
    """The colour at the corner, where no plane is when every plane is paused."""
    return img.getpixel((0, 0))[:3]


def start_server(server_bin, client_bin, gapi):
    sock = f"/tmp/wm-compositor-check-{os.getpid()}.sock"
    try:
        os.unlink(sock)
    except OSError:
        pass
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    env["XL_WM_APPS"] = f"{client_bin};{client_bin}"
    env["XL_WM_FPS"] = str(FPS)
    env["XL_HIDE_FPS"] = "1"
    proc = subprocess.Popen([server_bin, "--headless", "--gapi", gapi, "--width", str(WIDTH),
            "--height", str(HEIGHT)], env=env, cwd=os.path.dirname(server_bin),
            stdout=open(SERVER_LOG, "w"), stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 30.0
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise SystemExit(f"wmserver exited early with {proc.returncode}, see {SERVER_LOG}")
        if os.path.exists(sock):
            try:
                return proc, rc.Session(sock)
            except OSError:
                pass
        time.sleep(0.1)
    proc.kill()
    raise SystemExit("wmserver did not come up")


def run_basic(server_bin, client_bin, gapi):
    print(f"== basic ({gapi})")
    proc, s = start_server(server_bin, client_bin, gapi)
    clients = {}
    try:
        # 1. launch
        st = wait_state(s, lambda st: len(st.get("planes", [])) == 2
                and all(p.get("published", 0) > 0 for p in st["planes"]), timeout=30.0)
        check("launch: the compositor owns the host", st.get("attached") is True, st.get("attached"))
        check("launch: two planes, both published", len(st.get("planes", [])) == 2
                and all(p.get("published", 0) > 0 for p in st.get("planes", [])),
                [(p.get("app"), p.get("published")) for p in st.get("planes", [])])
        st = wait_state(s, lambda st: plane_by_app(st, "app1").get("minimized") is True
                and plane_by_app(st, "app2").get("focused") is True)
        top, low = plane_by_app(st, "app2"), plane_by_app(st, "app1")
        check("launch: the last launched plane is on top, enabled and focused",
                top.get("enabled") is True and top.get("focused") is True
                and top.get("z", -1) > low.get("z", -1), top)
        check("launch: the plane below is paused and minimized",
                low.get("enabled") is False and low.get("minimized") is True, low)
        if len(st.get("planes", [])) != 2:
            return

        for p in st["planes"]:
            clients[p["app"]] = client_session(p["inspector"])
        check("launch: both clients answer on their inspectors", all(clients.values()))
        if not all(clients.values()):
            return

        # 2. identity
        for c in clients.values():
            c.invoke("client-animation", op="stop")
        settle_plane(s, "app2")
        host_frame(s)
        host, planeImg = host_shot(s), plane_shot(s, top["window"])
        delta = max_delta(host, planeImg)
        check("identity: the host frame is the top plane's frame", delta is not None and delta <= 1,
                f"max channel delta {delta}")

        # 3. pacing
        clients["app2"].invoke("client-animation", op="start")
        time.sleep(0.5)
        a = state(s)
        time.sleep(2.0)
        b = state(s)
        dPub = plane_by_app(b, "app2")["published"] - plane_by_app(a, "app2")["published"]
        dHost = b["hostFrames"] - a["hostFrames"]
        dLow = plane_by_app(b, "app1")["published"] - plane_by_app(a, "app1")["published"]
        check("pacing: the animated plane draws", dPub > 10, f"{dPub} frames in 2 s")
        check("pacing: plane frames never outrun host frames", dPub <= dHost + 1,
                f"published {dPub}, host {dHost}")
        check("pacing: plane frames follow the vblank rate", dPub <= 2 * FPS * 1.25 + 2,
                f"published {dPub} in 2 s at {FPS} Hz")
        check("pacing: the paused plane draws nothing", dLow == 0, f"{dLow} frames")

        a = state(s)
        shots = [plane_shot(s, top["window"]) for _ in range(SHOTS)]
        b = state(s)
        good = sum(1 for img in shots if img is not None and img.size == (WIDTH, HEIGHT))
        check("screenshots: every plane screenshot of the animated plane arrives", good == SHOTS,
                f"{good} of {SHOTS}")
        check("screenshots: the host keeps composing meanwhile",
                b["hostFrames"] > a["hostFrames"], f"{b['hostFrames'] - a['hostFrames']} frames")
        a = state(s)
        time.sleep(1.0)
        b = state(s)
        check("screenshots: the pace comes back after them",
                b["hostFrames"] - a["hostFrames"] >= FPS // 2,
                f"{b['hostFrames'] - a['hostFrames']} host frames in 1 s")
        clients["app2"].invoke("client-animation", op="stop")
        settle_plane(s, "app2")

        # 4. caps
        r = s.invoke("wm-plane", plane=top["id"], dst=[0, 0, 400, 300])
        after = plane_by_app(state(s), "app2")
        check("caps: a scaled plane is refused", r.get("ok") is False, r)
        check("caps: a refused commit changes nothing", after.get("dst") == [0, 0, 800, 600],
                after.get("dst"))

        host_frame(s)
        opaque = host_shot(s)
        r = s.invoke("wm-plane", plane=top["id"], alpha=0.5)
        check("caps: plane alpha is accepted", r.get("ok") is True, r)
        host_frame(s)
        half = host_shot(s)
        s.invoke("wm-plane", plane=top["id"], alpha=1.0)
        r = s.invoke("wm-pause", plane=top["id"])
        host_frame(s)
        bg = background_rgb(host_shot(s))
        s.invoke("wm-resume", plane=top["id"])
        worst = 0
        if opaque is not None and half is not None:
            po, ph = opaque.tobytes(), half.tobytes()
            for i in range(0, len(po), 4 * 97):  # a sample of the pixels
                for k in range(3):
                    want = (po[i + k] * 0.5 + bg[k] * 0.5)
                    worst = max(worst, abs(ph[i + k] - want))
        check("caps: a plane at alpha 0.5 blends over the background",
                opaque is not None and half is not None and worst <= 2, f"worst {worst:.1f}")

        # 5. pause and resume
        r = s.invoke("wm-pause", plane=top["id"])
        check("pause: accepted", r.get("ok") is True, r)
        st = wait_state(s, lambda st: plane_by_app(st, "app2").get("minimized") is True)
        check("pause: the plane is minimized", plane_by_app(st, "app2").get("minimized") is True)
        clients["app2"].invoke("client-animation", op="start")
        time.sleep(0.3)
        a = plane_by_app(state(s), "app2")["published"]
        time.sleep(1.0)
        b = plane_by_app(state(s), "app2")["published"]
        check("pause: a paused plane draws nothing", b == a, f"{b - a} frames")
        host_frame(s)
        paused = host_shot(s)
        colours = paused.getcolors(4) if paused is not None else None
        check("pause: the host shows only the background", colours is not None and len(colours) == 1,
                colours)

        r = s.invoke("wm-resume", plane=top["id"])
        check("resume: accepted", r.get("ok") is True, r)
        st = wait_state(s, lambda st: plane_by_app(st, "app2").get("minimized") is False
                and plane_by_app(st, "app2").get("published", 0) > b)
        check("resume: the plane is not minimized and draws again",
                plane_by_app(st, "app2").get("minimized") is False
                and plane_by_app(st, "app2").get("published", 0) > b)
        clients["app2"].invoke("client-animation", op="stop")
        settle_plane(s, "app2")
        host_frame(s)
        delta = max_delta(host_shot(s), plane_shot(s, top["window"]))
        check("resume: the host shows the plane again", delta is not None and delta <= 1,
                f"max channel delta {delta}")

        # 6. kill
        pid = pid_of(top["inspector"])
        check("kill: the top client's process is found", pid is not None)
        if pid:
            os.kill(pid, signal.SIGKILL)
        clients["app2"].close()
        clients.pop("app2")
        st = wait_state(s, lambda st: len(st.get("planes", [])) == 1
                and st["planes"][0].get("enabled") is True
                and st["planes"][0].get("minimized") is False, timeout=10.0)
        check("kill: the plane goes with the process", len(st.get("planes", [])) == 1,
                len(st.get("planes", [])))
        survivor = st.get("planes", [{}])[0]
        check("kill: the other plane is shown", survivor.get("app") == "app1"
                and survivor.get("enabled") is True and survivor.get("minimized") is False,
                survivor)
        app = [a for a in st.get("apps", []) if a.get("label") == "app2"]
        check("kill: the application is reported gone", bool(app) and app[0].get("running") is False,
                app)
        before = st.get("hostFrames", 0)
        s.ok("frame", count=1)
        st = wait_state(s, lambda st: st.get("hostFrames", 0) > before, timeout=5.0)
        check("kill: the server keeps composing", st.get("hostFrames", 0) > before)
        check("kill: the inspector still answers", bool(s.ok("windows")))
        host_frame(s)
        delta = max_delta(host_shot(s), plane_shot(s, survivor.get("window")))
        check("kill: the host shows the remaining plane", delta is not None and delta <= 1,
                f"max channel delta {delta}")
    finally:
        for c in clients.values():
            if c:
                c.close()
        # 7. quit
        try:
            s.call("quit")
        except (SystemExit, OSError):
            pass
        s.close()
        try:
            code = proc.wait(20)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = None
        check("quit: the server exits with 0", code == 0, code)

    check_log(gapi)


def check_log(gapi):
    text = open(SERVER_LOG, errors="replace").read()
    errors = [l.rstrip() for l in text.splitlines() if l.startswith("[E]")]
    check("the server log has no errors", not errors, "\n    ".join(errors[:5]))
    if gapi == "vulkan":
        check("no Vulkan validation error", "Validation Error" not in text, SERVER_LOG)


def run_soak(server_bin, client_bin, gapi, seconds):
    """Mixed load for `seconds`: every step the host must have composed since the last one."""
    print(f"== soak ({gapi}, {seconds} s)")
    proc, s = start_server(server_bin, client_bin, gapi)
    clients = {}
    stalls = []
    steps = 0
    try:
        st = wait_state(s, lambda st: len(st.get("planes", [])) == 2
                and all(p.get("published", 0) > 0 for p in st["planes"]), timeout=30.0)
        check("soak: two planes published", len(st.get("planes", [])) == 2)
        if len(st.get("planes", [])) != 2:
            return
        for p in st["planes"]:
            clients[p["app"]] = client_session(p["inspector"])
        top = plane_by_app(st, "app2")
        c = clients["app2"]
        c.invoke("client-animation", op="start")

        deadline = time.monotonic() + seconds
        last = state(s).get("hostFrames", 0)
        while time.monotonic() < deadline:
            op = steps % 6
            if op == 0:
                plane_shot(s, top["window"])
            elif op == 1:
                s.invoke("wm-plane", plane=top["id"], alpha=0.5)
            elif op == 2:
                s.invoke("wm-plane", plane=top["id"], alpha=1.0)
                host_shot(s)
            elif op == 3:
                s.invoke("wm-pause", plane=top["id"])
            elif op == 4:
                s.invoke("wm-resume", plane=top["id"])
            else:
                for _ in range(3):
                    plane_shot(s, top["window"])
            time.sleep(0.5)
            now = state(s).get("hostFrames", 0)
            # A paused top plane leaves nothing to compose; every other step must move the host.
            if op != 3 and now <= last:
                stalls.append(steps)
            last = now
            steps += 1
        check("soak: the host composed through every step", not stalls,
                f"stalled at steps {stalls[:10]} of {steps}")
    finally:
        for c in clients.values():
            if c:
                c.close()
        try:
            s.call("quit")
        except (SystemExit, OSError):
            pass
        s.close()
        try:
            code = proc.wait(20)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = None
        check("soak: the server exits with 0", code == 0, code)

    check_log(gapi)


def default_binary(project, name, near=None):
    if near:
        # .../stappler-build/<target>/<build>/cc/<exe>: the same target and build type
        parts = near.split(os.sep)
        if "stappler-build" in parts:
            i = parts.index("stappler-build")
            return os.path.join(ROOT, project, *parts[i:i + 4], name)
    return os.path.join(ROOT, project, "stappler-build/x86_64-unknown-linux-gnu/debug/cc", name)


def main():
    args = sys.argv[1:]
    stage, gapis, soak, paths = "basic", ["vulkan", "soft"], 0, []
    while args:
        a = args.pop(0)
        if a == "--stage":
            stage = args.pop(0)
        elif a == "--gapi":
            gapis = [args.pop(0)]
        elif a == "--soak":
            soak = int(args.pop(0))
        else:
            paths.append(os.path.abspath(a))

    server_bin = paths[0] if paths else default_binary("examples/os/server", "wmserver")
    client_bin = paths[1] if len(paths) > 1 else default_binary("tests/window/client", "clientapp",
            server_bin)
    for b in (server_bin, client_bin):
        if not os.path.exists(b):
            raise SystemExit(f"not built: {b}")

    if stage != "basic":
        raise SystemExit(f"unknown stage: {stage}")
    for gapi in gapis:
        if soak > 0:
            run_soak(server_bin, client_bin, gapi, soak)
        else:
            run_basic(server_bin, client_bin, gapi)

    print(f"{rc.checks} checks, {rc.failures} failures")
    if rc.failures:
        print(f"server log: {SERVER_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
