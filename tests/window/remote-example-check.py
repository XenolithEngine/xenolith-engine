#!/usr/bin/env python3
"""
Examples as remote clients.

Every app built on the standard entry point runs as a client of a remote server when started with
`--connect <address>`: the window it would open is asked from the server, with the scene its
DEFINE_PRIMARY_SCENE_CLASS builds, and the key comes from the launch token in XL_LAUNCH_TOKEN. The
server here is the headless testapp in the shape a window manager has: it listens without a window of
its own, opens the windows its clients ask for, and admits only the keys it issued, each with a
label.

For each example that is built (examples/window/{dndtree,form,dock}), unmodified:
  - it connects under the label of the key it was given, and gets a window of its own;
  - the window draws the example's scene (not a blank frame);
  - its popups open, as in-scene overlays: form's drop-down (chosen from the keyboard, input sent
    through the server), search picker and colour picker; dock's tab hint on hover; dndtree's
    context menu on a right click;
  - closing that window on the server ends the client process;
and for one of them, a client killed outright takes its window and leaves the server running.

Usage: remote-example-check.py [--transport shm|unix] [--gapi vulkan|soft] [testapp]
Examples that are not built are skipped and reported. XL_CHECK_SHOTS=<dir> keeps each example's
frame there. Prints "N checks, M failures".
"""
import importlib.util, os, secrets, struct, subprocess, sys, time, zlib

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check", os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check

ROOT = os.path.dirname(os.path.dirname(_here))
TRIPLE = os.environ.get("STAPPLER_TARGET", "x86_64-unknown-linux-gnu")
BUILD = os.environ.get("XL_CHECK_BUILD", "debug")
EXAMPLES = ["dndtree", "form", "dock"]


def example_binary(name):
    return os.path.join(ROOT, "examples", "window", name, "stappler-build", TRIPLE, BUILD, "cc", name)


def spawn_example(binary, share, token, inspector):
    env = dict(os.environ)
    env["XL_LAUNCH_TOKEN"] = token
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + inspector
    try:
        os.unlink(inspector)
    except OSError:
        pass
    return subprocess.Popen([binary, "--connect", share], env=env,
            cwd=os.path.dirname(binary), stdout=open(rc.CLIENT_LOG, "a"), stderr=subprocess.STDOUT)


def decode_png(raw):
    """8-bit RGB/RGBA, non-interlaced - what the inspector writes. Returns (w, h, rows of (r, g, b))."""
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat, width, height, channels = 8, b"", 0, 0, 4
    while pos < len(raw):
        length = struct.unpack(">I", raw[pos:pos + 4])[0]
        kind = raw[pos + 4:pos + 8]
        body = raw[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and color in (2, 6), (depth, color)
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    data = zlib.decompress(idat)
    stride = width * channels
    rows, prev, at = [], bytearray(stride), 0
    for _ in range(height):
        filt = data[at]
        line = bytearray(data[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if filt == 1:
                line[i] = (line[i] + a) & 0xFF
            elif filt == 2:
                line[i] = (line[i] + b) & 0xFF
            elif filt == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append([tuple(line[x * channels:x * channels + 3]) for x in range(width)])
        prev = line
    return width, height, rows


def distinct_colors(png):
    """How many colours a 16x16 grid of samples sees: a blank frame has one."""
    w, h, rows = decode_png(png)
    seen = set()
    for gy in range(16):
        for gx in range(16):
            seen.add(rows[min(h - 1, gy * h // 16)][min(w - 1, gx * w // 16)])
    return len(seen)


def session_by_label(status, label):
    return next((it for it in status.get("sessions") or [] if it.get("label") == label), {})


def window_of(status, session_id):
    return next((w for w in status.get("windows") or []
            if session_id and w.get("owner") == session_id), {})


def step(s, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        s.ok("frame", count=1)
        time.sleep(0.1)


def wait_exit(s, proc, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline and proc.poll() is None:
        step(s, 0.2)
    return proc.poll() is not None


def open_inspector(path, timeout=30.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return rc.Session(path, timeout=25.0)
            except OSError:
                pass
        time.sleep(0.1)
    return None


def key(code):
    return [{"event": "KeyPressed", "keycode": code}, {"event": "KeyReleased", "keycode": code}]


def scene_text(c):
    return (c.ok("scene") or {}).get("text", "")


def client_window_count(c):
    return len((c.ok("windows") or {}).get("windows", []))


def wait_scene(s, c, predicate, seconds=5.0):
    deadline = time.monotonic() + seconds
    text = ""
    while time.monotonic() < deadline:
        step(s, 0.2)
        text = scene_text(c)
        if predicate(text):
            return True, text
    return False, text


def form_popups(s, c, win):
    """The drop-down, the search picker and the colour picker, all in the client's own scene."""
    role = lambda: ((c.invoke("form.collect", form="left") or {}).get("collected") or {}).get("role")
    before = role()
    r = c.invoke("form.open", form="left", field="role") or {}
    check("form: the drop-down opens", bool(r.get("ok")), str(r))
    check("form: as an overlay, the client opens no window", client_window_count(c) == 1)
    step(s, 0.6)
    s.ok("input", window=win, native=True, events=key("DOWN"))
    step(s, 0.4)
    s.ok("input", window=win, native=True, events=key("ENTER"))
    deadline = time.monotonic() + 5.0
    after = before
    while time.monotonic() < deadline and after == before:
        step(s, 0.2)
        after = role()
    check("form: a choice made from the keyboard, through the server, changes the value",
            before is not None and after != before, f"{before} -> {after}")

    r = c.invoke("form.open", form="left", field="country") or {}
    check("form: the search picker opens", bool(r.get("ok")), str(r))
    c.invoke("form.close", form="left", field="country")

    r = c.invoke("form.open", form="left", field="accent") or {}
    step(s, 0.6)
    picker = c.invoke("form.picker") or {}
    check("form: the colour picker opens in the scene", bool(r.get("ok")) and "error" not in picker,
            f"{r} {picker}")
    c.invoke("form.close", form="left", field="accent")


def dock_popups(s, c, win):
    """The rail tab's hint: hover through the server, the hint appears in the client's scene."""
    s.ok("input", window=win, native=True, events=[
        {"event": "MouseMove", "x": 25, "y": 693, "button": "None"}])
    found, text = wait_scene(s, c, lambda t: "#aux-tip" in t)
    check("dock: a tab's hint shows on hover", found)


def dndtree_popups(s, c, win):
    """A right click on a row opens its context menu in the client's scene."""
    before = scene_text(c)
    rows = (c.invoke("dndtree.rows", tree="left") or {}).get("rows") or []
    s.ok("input", window=win, native=True, events=[
        {"event": "MouseMove", "x": 200, "y": 600, "button": "None"},
        {"event": "Begin", "x": 200, "y": 600, "button": "MouseRight"},
        {"event": "End", "x": 200, "y": 600, "button": "MouseRight"}])
    found, text = wait_scene(s, c, lambda t: ".xl-ui-menu" in t and ".xl-ui-menu" not in before)
    check("dndtree: a right click opens a context menu", found, f"{len(rows)} rows")


POPUPS = {"form": form_popups, "dock": dock_popups, "dndtree": dndtree_popups}


def connect_example(s, name, share, inspector):
    """Issue a key, start the example with it; returns (process, label, status) once it draws."""
    token = secrets.token_hex(16)
    label = f"example:{name}"
    r = s.invoke("remote-add-key", token=token, label=label, single=True) or {}
    if not r.get("ok"):
        return None, label, {}
    proc = spawn_example(example_binary(name), share, token, inspector)

    def ready(x):
        sess = session_by_label(x, label)
        return bool(window_of(x, sess.get("id")))

    return proc, label, rc.wait_for(s, ready, timeout=90.0)


def main():
    transport = "shm"
    gapi = None
    argv = sys.argv[1:]
    while argv and argv[0].startswith("--"):
        opt, argv = argv[0], argv[1:]
        if "=" in opt:
            opt, value = opt.split("=", 1)
        else:
            if not argv:
                raise SystemExit(f"{opt} needs a value")
            value, argv = argv[0], argv[1:]
        if opt == "--transport":
            transport = value
        elif opt == "--gapi":
            gapi = value
        else:
            raise SystemExit(f"unknown option: {opt}")

    server_bin = argv[0] if argv else os.path.join(_here, "stappler-build", TRIPLE, BUILD, "cc",
            "testapp")
    if not os.path.exists(server_bin):
        raise SystemExit(f"missing binary: {server_bin}\nbuild it: xenolith-cli build tests/window")

    built = [name for name in EXAMPLES if os.path.exists(example_binary(name))]
    for name in EXAMPLES:
        if name not in built:
            print(f"  skip {name}: not built (xenolith-cli build examples/window/{name})")
    if not built:
        print("0 checks, 0 failures (no example is built)")
        return 0

    pid = os.getpid()
    sock = f"/tmp/xl-remote-example-{pid}.sock"
    if transport == "shm":
        share = f"shm:/dev/shm/xl-remote-example-{pid}"
    elif transport == "unix":
        share = f"unix:/tmp/xl-remote-example-{pid}.xlsock"
    else:
        raise SystemExit(f"unknown transport: {transport}")

    # A window manager's server: nothing of its own, every window a client's, only issued keys.
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "4"
    os.environ["XL_REMOTE_LABELLED_KEYS"] = "1"

    server = None
    procs = []
    sockets = [sock]
    try:
        print(f"transport: {transport}\ngapi:   {gapi or 'default'}\nshare:  {share}")
        server = rc.start_server(server_bin, sock, share, secrets.token_hex(16), gapi=gapi,
                keep_running=True)
        s = rc.Session(sock)
        s.ok("frame", count=3)
        rc.wait_for(s, lambda st: st.get("listening"))

        for name in built:
            inspector = f"/tmp/xl-remote-example-{name}-{pid}.sock"
            sockets.append(inspector)
            proc, label, st = connect_example(s, name, share, inspector)
            if proc:
                procs.append(proc)
            sess = session_by_label(st, label)
            win = window_of(st, sess.get("id"))
            check(f"{name}: connects under the label of its launch key", bool(sess), str(st))
            check(f"{name}: gets a window of its own",
                    bool(win) and win.get("creator") == sess.get("id"), str(st))
            if not win:
                continue

            step(s, 3.0)  # glyphs reach a client in round trips; let the first ones land
            shot = rc.grab(s, window=win.get("name"))
            if os.environ.get("XL_CHECK_SHOTS"):
                with open(os.path.join(os.environ["XL_CHECK_SHOTS"], f"{name}.png"), "wb") as f:
                    f.write(shot)
            colors = distinct_colors(shot) if shot.startswith(b"\x89PNG") else 0
            check(f"{name}: the window draws the example's scene", colors >= 3,
                    f"{colors} distinct colours in the frame")

            c = open_inspector(inspector)
            if c is None:
                check(f"{name}: the client opens its inspector", False)
            else:
                POPUPS[name](s, c, win.get("name"))
                c.close()

            s.ok("window", op="close", window=win.get("name"))
            check(f"{name}: closing its window ends the client", wait_exit(s, proc, 20.0),
                    "the client is still running")
            st = rc.wait_for(s, lambda x: not session_by_label(x, label), timeout=20.0)
            check(f"{name}: and its session", not session_by_label(st, label), str(st))

        # A client killed outright: its window goes, the server stays.
        name = built[0]
        inspector = f"/tmp/xl-remote-example-{name}-kill-{pid}.sock"
        sockets.append(inspector)
        proc, label, st = connect_example(s, name, share, inspector)
        if proc:
            procs.append(proc)
        win = window_of(st, session_by_label(st, label).get("id"))
        rc.kill(proc)
        st = rc.wait_for(s, lambda x: not session_by_label(x, label)
                and not any(w.get("name") == win.get("name") for w in x.get("windows") or []),
                timeout=20.0)
        check(f"{name}: a killed client takes its window with it",
                bool(win) and not session_by_label(st, label)
                        and server.poll() is None and bool(st.get("listening")), str(st))
    finally:
        for p in procs + [server]:
            rc.kill(p)
        for p in sockets:
            try:
                os.unlink(p)
            except OSError:
                pass

    print(f"{rc.checks} checks, {rc.failures} failures")
    if rc.failures:
        print(f"server log: {rc.SERVER_LOG}\nclient log: {rc.CLIENT_LOG}")
    return 1 if rc.failures else 0


if __name__ == "__main__":
    sys.exit(main())
