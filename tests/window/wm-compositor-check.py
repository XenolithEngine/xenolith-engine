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

Stage `input` (M10): two planes side by side, input sent to the HOST window through its inspector:

1. taps: each half's client gets its tap in its own pixels (the right plane shows its window's
   left half, so host x 600 is x 200 there), the other client nothing;
2. z and input regions: the plane above takes only the points of its input region, the rest go
   to the plane below; an empty region is the whole destination again;
3. capture: a press that starts in one plane stays there across the border until it ends;
4. Pointer: the plane under the mouse has it, the other one not;
5. keys and focus: keys go to the plane the WM focused (`wm-focus`); moving the focus while a key
   is held cancels it there; typed text lands in the focused plane's field only;
6. a paused plane's press is cancelled, and the plane below takes the next tap;
7. at density 2 the client's scene gets the tap at half the host pixels.

Without `--stage` both stages run.

`--soak SECONDS` runs a long mixed load instead - animation, screenshots, plane alpha, pause and
resume in turn - and checks that the host never stops composing and the server exits cleanly.

    tests/window/wm-compositor-check.py [--stage basic|input] [--gapi soft|vulkan] [--soak SECONDS]
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


def start_server(server_bin, client_bin, gapi, density=None):
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
    cmd = [server_bin, "--headless", "--gapi", gapi, "--width", str(WIDTH), "--height", str(HEIGHT)]
    if density:
        cmd += ["--density", str(density)]
    proc = subprocess.Popen(cmd, env=env, cwd=os.path.dirname(server_bin),
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


def quit_server(proc, s):
    """Ask the server to quit; its exit code, or None when it had to be killed."""
    try:
        s.call("quit")
    except (SystemExit, OSError):
        pass
    s.close()
    try:
        return proc.wait(20)
    except subprocess.TimeoutExpired:
        proc.kill()
        return None


def tap(s, x, y, pid=0):
    s.ok("input", native=True, events=[
        {"event": "Begin", "id": pid, "button": "MouseLeft", "x": x, "y": y},
        {"event": "End", "id": pid, "button": "MouseLeft", "x": x, "y": y}])


def key(s, name, event, char=None, at=(600, 300)):
    # a key carries the pointer's position, as the platforms report it
    ev = {"event": event, "keycode": name, "x": at[0], "y": at[1]}
    if char is not None:
        ev["keychar"] = ord(char)
    s.ok("input", native=True, events=[ev])


def client_events(c, clear=False):
    return (c.invoke("client-input", clear=clear) or {}).get("events", [])


def wait_events(c, predicate, timeout=5.0):
    """The client's recorded input once `predicate` holds for it (or the last seen)."""
    deadline = time.monotonic() + timeout
    events = client_events(c)
    while time.monotonic() < deadline and not predicate(events):
        time.sleep(0.1)
        events = client_events(c)
    return events


def touches(events, name=None):
    return [e for e in events if e.get("kind") == "touch" and (name is None or e["event"] == name)]


def keys(events, name=None):
    return [e for e in events if e.get("kind") == "key" and (name is None or e["event"] == name)]


def near(e, x, y, tol=1.0):
    return e is not None and abs(e.get("x", -1e9) - x) <= tol and abs(e.get("y", -1e9) - y) <= tol


def client_flag(c, name, value, timeout=5.0):
    """Wait for the client's window to report `name` (focused, pointer) as `value`."""
    deadline = time.monotonic() + timeout
    st = c.invoke("client-state") or {}
    while time.monotonic() < deadline and st.get(name) is not value:
        time.sleep(0.1)
        st = c.invoke("client-state") or {}
    return st.get(name)


def layout(s, p, dst, z, src=None, **kw):
    r = s.invoke("wm-plane", plane=p["id"], src=src or [0, 0, dst[2], dst[3]], dst=dst, z=z,
            enabled=True, **kw)
    return r.get("ok") is True


def run_input(server_bin, client_bin, gapi):
    print(f"== input ({gapi})")
    proc, s = start_server(server_bin, client_bin, gapi)
    clients = {}
    try:
        st = wait_state(s, lambda st: len(st.get("planes", [])) == 2
                and all(p.get("published", 0) > 0 for p in st["planes"]), timeout=30.0)
        if len(st.get("planes", [])) != 2:
            check("input: two planes published", False, st)
            return
        p1, p2 = plane_by_app(st, "app1"), plane_by_app(st, "app2")
        c1, c2 = client_session(p1["inspector"]), client_session(p2["inspector"])
        clients = {"app1": c1, "app2": c2}
        check("input: both clients answer on their inspectors", c1 is not None and c2 is not None)
        if not (c1 and c2):
            return
        for c in (c1, c2):
            c.invoke("client-animation", op="stop")

        halves = layout(s, p1, [0, 0, 400, 600], 0) and layout(s, p2, [400, 0, 400, 600], 1)
        check("input: two planes side by side", halves)

        def reset():
            for c in (c1, c2):
                client_events(c, clear=True)
            s.invoke("wm-routes", clear=True)

        # 1. taps
        reset()
        tap(s, 200, 300)
        tap(s, 600, 300)
        e1 = wait_events(c1, lambda ev: touches(ev, "End"))
        e2 = wait_events(c2, lambda ev: touches(ev, "End"))
        b1, b2 = touches(e1, "Begin"), touches(e2, "Begin")
        check("taps: the left plane gets its tap in its pixels",
                len(b1) == 1 and near(b1[0], 200, 300), e1)
        check("taps: the right plane gets its tap shifted by its destination",
                len(b2) == 1 and near(b2[0], 200, 300), e2)
        routes = (s.invoke("wm-routes") or {}).get("routes", [])
        check("taps: the routes say hit, then capture",
                [r["reason"] for r in routes] == ["hit", "capture", "hit", "capture"]
                and [r["plane"] for r in routes] == [p1["id"], p1["id"], p2["id"], p2["id"]],
                routes)

        # 2. z and input regions
        full1 = layout(s, p1, [0, 0, 800, 600], 0)
        full2 = layout(s, p2, [0, 0, 800, 600], 1, input=[[400, 0, 400, 600]])
        check("regions: both planes on the whole screen, the top one with a region", full1 and full2)
        reset()
        tap(s, 600, 300)
        tap(s, 200, 300)
        e1 = wait_events(c1, lambda ev: touches(ev, "End"))
        e2 = wait_events(c2, lambda ev: touches(ev, "End"))
        check("regions: a point in the top plane's region goes to it",
                len(touches(e2, "Begin")) == 1 and near(touches(e2, "Begin")[0], 600, 300), e2)
        check("regions: a point outside it goes to the plane below",
                len(touches(e1, "Begin")) == 1 and near(touches(e1, "Begin")[0], 200, 300), e1)
        layout(s, p2, [0, 0, 800, 600], 1, input=[])
        reset()
        tap(s, 200, 300)
        e2 = wait_events(c2, lambda ev: touches(ev, "End"))
        time.sleep(0.3)
        check("regions: an empty region is the whole destination again",
                len(touches(e2, "Begin")) == 1 and not touches(client_events(c1)), e2)

        # 3. capture across the border
        layout(s, p1, [0, 0, 400, 600], 0)
        layout(s, p2, [400, 0, 400, 600], 1)
        reset()
        s.ok("input", native=True, events=[
            {"event": "Begin", "id": 0, "button": "MouseLeft", "x": 200, "y": 300},
            {"event": "Move", "id": 0, "button": "MouseLeft", "x": 600, "y": 310},
            {"event": "End", "id": 0, "button": "MouseLeft", "x": 600, "y": 310}])
        e1 = wait_events(c1, lambda ev: touches(ev, "End"))
        time.sleep(0.3)
        ends = touches(e1, "End")
        check("capture: the press stays with the plane it began in",
                len(touches(e1, "Begin")) == 1 and len(ends) == 1 and near(ends[0], 600, 310), e1)
        check("capture: the plane across the border sees nothing", not touches(client_events(c2)))

        # 4. Pointer
        s.ok("input", native=True, events=[
            {"event": "MouseMove", "button": "None", "x": 200, "y": 300}])
        check("pointer: the plane under the mouse has it", client_flag(c1, "pointer", True) is True)
        check("pointer: the other plane has not", client_flag(c2, "pointer", False) is False)
        s.ok("input", native=True, events=[
            {"event": "MouseMove", "button": "None", "x": 600, "y": 300}])
        check("pointer: it follows the mouse across", client_flag(c2, "pointer", True) is True
                and client_flag(c1, "pointer", False) is False)

        # 5. keys and focus; the fields let go of the keyboard first, so the keys reach the scenes
        for c in (c1, c2):
            c.invoke("client-field", op="blur")
        r = s.invoke("wm-focus", plane=p1["id"])
        check("focus: the WM focuses a plane", r.get("ok") is True, r)
        check("focus: its window is focused", client_flag(c1, "focused", True) is True
                and client_flag(c2, "focused", False) is False)
        reset()
        key(s, "F5", "KeyPressed")
        e1 = wait_events(c1, lambda ev: keys(ev, "KeyPressed"))
        check("keys: a key goes to the focused plane",
                [k.get("key") for k in keys(e1, "KeyPressed")] == ["F5"], e1)
        s.invoke("wm-focus", plane=p2["id"])
        e1 = wait_events(c1, lambda ev: keys(ev, "KeyCanceled"))
        check("keys: moving the focus cancels the held key where it was pressed",
                [k.get("key") for k in keys(e1, "KeyCanceled")] == ["F5"], e1)
        key(s, "F5", "KeyReleased")
        key(s, "F6", "KeyPressed")
        key(s, "F6", "KeyReleased")
        e2 = wait_events(c2, lambda ev: keys(ev, "KeyReleased"))
        time.sleep(0.3)
        check("keys: the next key goes to the newly focused plane",
                [k.get("key") for k in keys(e2, "KeyPressed")] == ["F6"], e2)
        check("keys: the release of the cancelled key reaches nobody",
                not keys(client_events(c1), "KeyReleased")
                and "F5" not in [k.get("key") for k in keys(client_events(c2))])
        c2.invoke("client-field", op="focus")
        time.sleep(0.3)
        before1 = (c1.invoke("client-text") or {}).get("text", "")
        for ch in "ab":
            key(s, ch.upper(), "KeyPressed", ch)
            key(s, ch.upper(), "KeyReleased", ch)
        deadline = time.monotonic() + 5.0
        text2 = ""
        while time.monotonic() < deadline:
            text2 = (c2.invoke("client-text") or {}).get("text", "")
            if text2.endswith("ab"):
                break
            time.sleep(0.1)
        check("keys: typed text lands in the focused plane's field", text2.endswith("ab"), text2)
        check("keys: and not in the other one",
                (c1.invoke("client-text") or {}).get("text", "") == before1)

        # 6. a paused plane
        layout(s, p1, [0, 0, 800, 600], 0)
        layout(s, p2, [400, 0, 400, 600], 1)
        reset()
        s.ok("input", native=True, events=[
            {"event": "Begin", "id": 0, "button": "MouseLeft", "x": 600, "y": 300}])
        wait_events(c2, lambda ev: touches(ev, "Begin"))
        r = s.invoke("wm-pause", plane=p2["id"])
        e2 = wait_events(c2, lambda ev: touches(ev, "Cancel"))
        check("pause: the paused plane's press is cancelled", len(touches(e2, "Cancel")) == 1, e2)
        s.ok("input", native=True, events=[
            {"event": "End", "id": 0, "button": "MouseLeft", "x": 600, "y": 300}])
        tap(s, 600, 300, pid=1)
        e1 = wait_events(c1, lambda ev: touches(ev, "End"))
        check("pause: the plane below takes the next tap",
                len(touches(e1, "Begin")) == 1 and near(touches(e1, "Begin")[0], 600, 300), e1)
    finally:
        for c in clients.values():
            if c:
                c.close()
        code = quit_server(proc, s)
        check("input: the server exits with 0", code == 0, code)
    check_log(gapi)

    # 7. density 2
    print(f"== input, density 2 ({gapi})")
    proc, s = start_server(server_bin, client_bin, gapi, density=2)
    clients = {}
    try:
        st = wait_state(s, lambda st: len(st.get("planes", [])) == 2
                and all(p.get("published", 0) > 0 for p in st["planes"]), timeout=30.0)
        top = plane_by_app(st, "app2")
        c = client_session(top["inspector"]) if top else None
        clients = {"app2": c}
        check("density: the top client answers", c is not None)
        if not c:
            return
        c.invoke("client-animation", op="stop")
        cs = c.invoke("client-state") or {}
        check("density: the plane's window has the host density", cs.get("density") == 2.0, cs)
        client_events(c, clear=True)
        tap(s, 200, 300)
        ev = wait_events(c, lambda ev: touches(ev, "End"))
        b = touches(ev, "Begin")
        check("density: the tap arrives in the plane's pixels", len(b) == 1 and near(b[0], 200, 300),
                ev)
        check("density: the scene has it at half of them", len(b) == 1
                and abs(b[0].get("sceneX", 0) - 100) <= 1 and abs(b[0].get("sceneY", 0) - 150) <= 1,
                b)
    finally:
        for c in clients.values():
            if c:
                c.close()
        code = quit_server(proc, s)
        check("density: the server exits with 0", code == 0, code)
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
    stages, gapis, soak, paths = ["basic", "input"], ["vulkan", "soft"], 0, []
    while args:
        a = args.pop(0)
        if a == "--stage":
            stages = [args.pop(0)]
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

    for stage in stages:
        if stage not in ("basic", "input"):
            raise SystemExit(f"unknown stage: {stage}")
    for gapi in gapis:
        if soak > 0:
            run_soak(server_bin, client_bin, gapi, soak)
            continue
        for stage in stages:
            if stage == "basic":
                run_basic(server_bin, client_bin, gapi)
            else:
                run_input(server_bin, client_bin, gapi)

    print(f"{rc.checks} checks, {rc.failures} failures")
    if rc.failures:
        print(f"server log: {SERVER_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
