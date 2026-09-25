#!/usr/bin/env python3
"""What a remote client's frames look like on the server, and what they cost to redraw.

The client here is testapp itself, run in client mode (`--connect`): the same layouts a local run
shows, drawn by the server into a virtual window of its own. Three things are checked:

  * partial redraw works for a client - the damage stand (XL_DAMAGE_TEST) walks its red square
    while frames are held by a slow reader, and every picture has exactly one square; the damage
    log shows the frames took the partial path. That needs the client's data identities on the
    wire: before, every data set was a new one on the server, and every frame a full redraw;
  * the client draws what a local run draws - the gradient layout (XL_GRADIENT_TEST: a linear
    gradient and a shaded outline, which live in the sprite's state extension, not in its vertex
    data) matches the same layout rendered locally, pixel for pixel within a small tolerance;
  * the state extension crosses the wire - `gradient.step` changes no vertex data, only the
    gradient's generation, and the server must repaint for it (a partial frame in its damage log),
    still matching a local run stepped the same way.

The renderer itself draws neither gradients nor shaded outlines today (the flat queue ignores
both; see os-examples-plan.md), so the parity holds for the picture as it is and the repaint is
what proves the gradient arrived.

    tests/window/remote-render-check.py [--gapi vulkan|soft] [path-to-testapp]

Without --gapi both backends run (testapp must be built with SOFT=1 for soft).

Prints "N checks, M failures"; exit status is the result.
"""
import base64, importlib.util, io, os, secrets, subprocess, sys, time

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check

WIDTH, HEIGHT = 1024, 768
SQUARE = 200  # DamageLayout
STEP_DELAY, STEPS = 0.5, 3  # DamageLayout::StepDelay, ::MoveSteps
TOLERANCE = 2  # per channel, local vs remote


def open_inspector(path, timeout=40.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return rc.Session(path)
            except OSError:
                pass
        time.sleep(0.1)
    return None


def decode(session, window=None):
    shot = (session.ok("screenshot", window=window) if window else session.ok("screenshot")) or {}
    data = shot.get("data") or ""
    if isinstance(data, str) and data.startswith("BASE64:"):
        raw = data[len("BASE64:"):]
        data = base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4))
    return Image.open(io.BytesIO(data)).convert("RGB")


def red_area(img):
    px = img.tobytes()
    return sum(1 for i in range(0, len(px), 3)
            if px[i] > 200 and px[i + 1] < 110 and px[i + 2] < 110)


def mismatch(a, b):
    """Pixels differing by more than TOLERANCE in any channel; None when the sizes differ."""
    if a.size != b.size:
        return None
    pa, pb = a.tobytes(), b.tobytes()
    bad = 0
    for i in range(0, len(pa), 3):
        if abs(pa[i] - pb[i]) > TOLERANCE or abs(pa[i + 1] - pb[i + 1]) > TOLERANCE \
                or abs(pa[i + 2] - pb[i + 2]) > TOLERANCE:
            bad += 1
    return bad


def vpump(s, name, seconds):
    """Step the host and the client's window: an idle client does not ask for frames by itself."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        s.ok("frame", count=1)
        s.ok("frame", window=name, count=1)
        time.sleep(0.1)


def spawn_testapp_client(binary, share, token, sock, gapi_env):
    env = dict(os.environ)
    env.update(gapi_env)
    env["XL_LAUNCH_TOKEN"] = token
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    try:
        os.unlink(sock)
    except OSError:
        pass
    return subprocess.Popen([binary, "--connect", share, "--width", str(WIDTH), "--height",
            str(HEIGHT)], env=env, cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(rc.CLIENT_LOG, "a"), stderr=subprocess.STDOUT)


def start_local(binary, sock, gapi, test_env):
    env = dict(os.environ)
    env.update(test_env)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock
    try:
        os.unlink(sock)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", str(WIDTH), "--height", str(HEIGHT),
            "--gapi", gapi], env=env, cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(os.environ.get("XL_LOCAL_LOG", os.devnull), "w"), stderr=subprocess.STDOUT)
    return proc, open_inspector(sock)


def client_window(s):
    for w in (s.ok("windows") or {}).get("windows", []):
        if w.get("virtual"):
            return w.get("id")
    return None


def run(binary, gapi):
    pid = os.getpid()
    sock = f"/tmp/xl-remote-render-{gapi}-{pid}.sock"
    csock = f"/tmp/xl-remote-render-client-{gapi}-{pid}.sock"
    lsock = f"/tmp/xl-remote-render-local-{gapi}-{pid}.sock"
    share = f"shm:/dev/shm/xl-remote-render-{gapi}-{pid}"
    token = secrets.token_hex(16)
    print(f"--- gapi: {gapi}")

    server = client = local = None
    s = c = l = None
    try:
        server = rc.start_server(binary, sock, share, token, gapi=gapi, keep_running=True)
        s = rc.Session(sock)
        rc.wait_for(s, lambda st: st.get("listening"))

        # --- the damage stand in the client ------------------------------------------------------
        mark = len(open(rc.SERVER_LOG, errors="replace").read())
        client = spawn_testapp_client(binary, share, token, csock,
                {"XL_DAMAGE_TEST": "1", "XL_HIDE_FPS": "1", "XL_FLAT_QUEUE": "1"})
        name = None
        deadline = time.monotonic() + 40.0
        while name is None and time.monotonic() < deadline:
            s.ok("frame", count=1)
            name = client_window(s)
            time.sleep(0.1)
        check("testapp in client mode gets a virtual window on the server", name is not None,
                "no virtual window appeared")
        if name is None:
            return
        deadline = time.monotonic() + 20.0
        while not (s.ok("window", window=name, op="plane") or {}).get("serial") \
                and time.monotonic() < deadline:
            vpump(s, name, 0.1)

        square = SQUARE ** 2
        areas = []
        deadline = time.monotonic() + STEP_DELAY * (STEPS + 3)
        while time.monotonic() < deadline:
            s.ok("window", window=name, op="plane-hold", ms=400)
            vpump(s, name, 0.15)
            areas.append(red_area(decode(s, window=name)))
        bad = [a for a in areas if not (0.9 * square <= a <= 1.1 * square)]
        check("the client's walking square leaves no trail", not bad and len(areas) >= 5,
                f"expected ~{square} red pixels, got {areas}")
        text = open(rc.SERVER_LOG, errors="replace").read()[mark:]
        word = "damage: partial redraw" if gapi == "vulkan" else "damage: repainting"
        check("and the client's frames took the partial path", text.count(word) > 0,
                f"no '{word}' in {rc.SERVER_LOG}")
        rc.kill(client)
        client = None
        deadline = time.monotonic() + 20.0
        while client_window(s) and time.monotonic() < deadline:
            s.ok("frame", count=1)
            time.sleep(0.1)

        # --- the gradient layout: client against a local run --------------------------------------
        test_env = {"XL_GRADIENT_TEST": "1", "XL_HIDE_FPS": "1", "XL_FLAT_QUEUE": "1"}
        client = spawn_testapp_client(binary, share, token, csock, test_env)
        name = None
        deadline = time.monotonic() + 40.0
        while name is None and time.monotonic() < deadline:
            s.ok("frame", count=1)
            name = client_window(s)
            time.sleep(0.1)
        c = open_inspector(csock)
        local, l = start_local(binary, lsock, gapi, test_env)
        if name is None or c is None or l is None:
            check("the gradient layout runs in the client and locally", False,
                    f"window={name} client={c is not None} local={l is not None}")
            return
        vpump(s, name, 3.0)  # glyphs of the caption arrive over the font server
        l.ok("frame", count=10)
        time.sleep(1.0)
        l.ok("frame", count=10)
        remote_img = decode(s, window=name)
        local_img = decode(l)
        if os.environ.get("XL_CHECK_SHOTS"):
            remote_img.save(os.path.join(os.environ["XL_CHECK_SHOTS"], f"gradient-remote-{gapi}.png"))
            local_img.save(os.path.join(os.environ["XL_CHECK_SHOTS"], f"gradient-local-{gapi}.png"))
        bad = mismatch(remote_img, local_img)
        check("the client's picture of the gradient layout matches a local run",
                bad == 0, f"{bad} pixels differ by more than {TOLERANCE}, sizes "
                f"{remote_img.size} vs {local_img.size}")

        mark = len(open(rc.SERVER_LOG, errors="replace").read())
        c.invoke("gradient.step")
        l.invoke("gradient.step")
        vpump(s, name, 1.5)
        l.ok("frame", count=10)
        time.sleep(0.5)
        remote_img = decode(s, window=name)
        local_img = decode(l)
        bad = mismatch(remote_img, local_img)
        check("after a gradient change the client still matches a local run", bad == 0,
                f"{bad} pixels differ by more than {TOLERANCE}")
        text = open(rc.SERVER_LOG, errors="replace").read()[mark:]
        check("and the server repainted for it (the gradient crossed the wire)", text.count(word) > 0,
                f"no '{word}' after gradient.step ({rc.SERVER_LOG})")
    finally:
        for sess in (c, l, s):
            if sess:
                sess.close()
        for proc in (client, local, server):
            rc.kill(proc)
        for p in (sock, csock, lsock):
            try:
                os.unlink(p)
            except OSError:
                pass


def main():
    gapis = ["vulkan", "soft"]
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
        else:
            raise SystemExit(f"unknown option: {opt}")

    binary = os.path.join(_here, "stappler-build", "x86_64-unknown-linux-gnu", "debug", "cc",
            "testapp")
    if argv:
        binary = argv[0]
    if not os.path.exists(binary):
        raise SystemExit(f"missing binary: {binary}\nbuild it: xenolith-cli build tests/window")

    # The server serves client windows as virtual ones, through the light queue with partial
    # redraw, and says what each frame decided.
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_VIRTUAL_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"
    os.environ["XL_FLAT_QUEUE"] = "1"
    os.environ["XL_VK_DAMAGE_LOG"] = "1"
    os.environ["XL_SOFT_DAMAGE_LOG"] = "1"

    for gapi in gapis:
        run(binary, gapi)
        if gapi == "vulkan":
            log = open(rc.SERVER_LOG, errors="replace").read()
            check("no Vulkan validation error on the server", "Validation Error" not in log,
                    rc.SERVER_LOG)

    print(f"{rc.checks} checks, {rc.failures} failures")
    print(f"logs: {rc.SERVER_LOG} {rc.CLIENT_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
