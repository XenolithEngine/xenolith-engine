#!/usr/bin/env python3
"""End-to-end check of VIRTUAL windows: a client's window with no OS window behind it.

A window manager composes its clients' windows itself, so the window a client asks for must not be
a window of the host's window system. WindowCreationFlags::Virtual is that window: every controller
makes the same one (sprt::window::VirtualWindow), outside the backend, and the compositor that will
read it (M8) decides for it what a window system decides for any other window. This proves that
window with no compositor yet, by hand, through the inspector:

  * the client's window is virtual on the server, and the server's own window still names the
    window system;
  * the client draws into it, and input sent to it at the server reaches the client's scene;
  * it maps Enabled and unfocused, and focus is what the window manager says (`virtual-state`);
  * the client can not resize it or change its state - that is the window manager's decision -
    but the server can resize it;
  * frames: on demand while unclaimed; claimed (`external-display-link`), a frame only per
    `display-link`; released, on demand again;
  * no subwindows: a drop-down on the client is an in-scene overlay;
  * closing it does not end the server, and closing the server's own window does, virtual windows
    or not.

    tests/window/virtual-window-check.py [--gapi vulkan|soft] [path-to-testapp] [path-to-clientapp]

Without --gapi both backends run, one server each (testapp must be built with SOFT=1 for soft).

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, secrets, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check


def windows(session):
    return (session.ok("windows") or {}).get("windows", [])


def entry_of(session, name):
    for w in windows(session):
        if w.get("id") == name:
            return w
    return {}


def open_inspector(path, timeout=25.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return rc.Session(path)
            except OSError:
                pass
        time.sleep(0.1)
    return None


def pump(s, seconds):
    """Let wall time pass while the server keeps stepping (headless frames are on demand)."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        s.ok("frame", count=1)
        time.sleep(0.1)


def presented(s, name):
    """The order of the last frame the window presented; advances with every frame drawn."""
    return int((s.ok("frame", window=name, count=0) or {}).get("presented", 0))


def state_of(session, window=None):
    kw = {"window": window} if window else {}
    return (session.ok("window", op="state", **kw) or {}).get("state", "")


def wait_exit(proc, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            return True
        time.sleep(0.1)
    return False


def run(server_bin, client_bin, gapi):
    pid = os.getpid()
    sock = f"/tmp/xl-virtual-window-{gapi}-{pid}.sock"
    sock_a = f"/tmp/xl-virtual-window-a-{gapi}-{pid}.sock"
    share = f"shm:/dev/shm/xl-virtual-window-{gapi}-{pid}"
    token = secrets.token_hex(16)

    server = client = None
    s = ca = None
    try:
        print(f"--- gapi: {gapi}")
        # No --keep-running: the host window is what keeps a window manager alive, and closing it
        # must end the process even with virtual windows left.
        server = rc.start_server(server_bin, sock, share, token, gapi=gapi)
        s = rc.Session(sock)
        s.ok("frame", count=3)
        status = rc.wait_for(s, lambda st: st.get("listening"))
        spki = status.get("spki") or ""
        server_scene = rc.grab(s)
        host = [w for w in windows(s) if w.get("default")]
        host_id = host[0].get("id") if host else rc.PRIMARY_WINDOW_ID

        client = rc.spawn_client(client_bin, share, token, spki, inspector=sock_a,
                extra_env={"XL_CLIENT_CREATE_WINDOW": "own:400x300"})
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 1
                and all(w.get("owner") for w in x.get("windows", [])), timeout=40.0)
        wins = st.get("windows", [])
        name = wins[0].get("name") if wins else None
        check("a client's request becomes a window on the server", bool(name), str(st))

        # --- the window is virtual ----------------------------------------------------------------
        e = entry_of(s, name)
        check("the client's window is virtual", e.get("virtual") is True, str(e))
        check("and a Root window of the size asked for",
                e.get("type") == "Root" and (e.get("width"), e.get("height")) == (400, 300), str(e))
        check("the server's own window is not", entry_of(s, host_id).get("virtual") is False,
                str(windows(s)))
        # Two compositor frames in flight, one image drawn and one published and not yet taken.
        check("its swapchain is the compositor's ring of four images",
                e.get("imageCount") == 4 and entry_of(s, host_id).get("imageCount") == 3,
                f"{e.get('imageCount')} / host {entry_of(s, host_id).get('imageCount')}")

        ca = open_inspector(sock_a)
        if ca is None:
            raise SystemExit("the client never opened its inspector - it has no window")
        wm = (ca.invoke("client-state") or {}).get("serverWm")
        check("the server's window system is its own window's, not the virtual one's",
                wm == "headless", str(wm))

        # --- the client draws it ------------------------------------------------------------------
        pump(s, 1.0)
        shot = rc.grab(s, window=name)
        check("the client draws the virtual window", shot.startswith(b"\x89PNG")
                and len(shot) > 2000 and shot != server_scene, f"{len(shot)} bytes")

        # --- state: Enabled, not focused; focus is the window manager's ----------------------------
        pump(s, 0.5)
        cs = state_of(ca)
        check("it maps Enabled and unfocused", "Enabled" in cs and "Focused" not in cs, cs)
        host_before = state_of(s, host_id)
        s.ok("window", window=name, op="virtual-state", state="Focused", value=True)
        pump(s, 0.5)
        cs = state_of(ca)
        check("focus is what the window manager says", "Focused" in cs, cs)
        check("and the host window keeps its own focus", state_of(s, host_id) == host_before,
                f"{host_before} -> {state_of(s, host_id)}")
        r = s.call("window", window=name, op="virtual-state", state="Fullscreen", value=True)
        pump(s, 0.3)
        check("only the state a window manager owns is set that way",
                "Fullscreen" not in state_of(ca), state_of(ca))

        # --- input sent at the server reaches the client ------------------------------------------
        # The client's field is focused at startup; keys typed into the virtual window at the server
        # are text only if the client's request for text input crossed over, and the characters only
        # arrive if the server routed the window's input to the client.
        s.ok("input", window=name, native=True, events=[
            {"event": "KeyPressed", "keycode": "A", "keychar": ord("a")},
            {"event": "KeyReleased", "keycode": "A", "keychar": ord("a")},
            {"event": "KeyPressed", "keycode": "B", "keychar": ord("b")},
            {"event": "KeyReleased", "keycode": "B", "keychar": ord("b")},
        ])
        pump(s, 1.0)
        text = ca.invoke("client-text") or {}
        check("keys typed into the virtual window reach the client's field",
                text.get("text") == "ab", str(text))

        # --- the client does not decide its geometry -----------------------------------------------
        r = ca.call("window", op="resize", width=500, height=350)
        check("the client can not resize its virtual window",
                r.get("status") != "ok" and "Declined" in str(r.get("error")), str(r))
        acc = (ca.ok("window", op="enable-state", state="Fullscreen") or {}).get("accepted")
        check("nor make it fullscreen", acc is False, str(acc))
        pump(s, 0.5)
        check("and the session survived both",
                bool((s.invoke("remote") or {}).get("clientConnected")), "client disconnected")
        s.ok("window", window=name, op="resize", width=480, height=320)
        pump(s, 1.0)
        cst = ca.invoke("client-state") or {}
        check("the server resizes it, and the client follows",
                (cst.get("constraintsWidth"), cst.get("constraintsHeight")) == (480, 320),
                str(cst))

        # --- no subwindows: a drop-down is an overlay ----------------------------------------------
        r = ca.invoke("client-popup", op="open") or {}
        check("a drop-down on the client opens as an in-scene overlay",
                bool(r.get("opened")) and not r.get("native"), str(r))
        ca.invoke("client-popup", op="close")

        # --- frames: on demand, claimed, released --------------------------------------------------
        ca.ok("render", seconds=60)
        f0 = presented(s, name)
        pump(s, 1.0)
        f1 = presented(s, name)
        check("unclaimed, an animating client gets frames", f1 > f0 + 3, f"{f0} -> {f1}")

        s.ok("window", window=name, op="external-display-link", value=True)
        pump(s, 0.5)
        f2 = presented(s, name)
        pump(s, 1.0)
        f3 = presented(s, name)
        check("claimed, no frame starts without a display link", f3 <= f2 + 1, f"{f2} -> {f3}")

        s.ok("window", window=name, op="display-link", count=3)
        pump(s, 1.0)
        f4 = presented(s, name)
        check("each display link starts at most one frame", f3 < f4 <= f3 + 4, f"{f3} -> {f4}")

        s.ok("window", window=name, op="external-display-link", value=False)
        pump(s, 1.0)
        f5 = presented(s, name)
        check("released, frames run on demand again", f5 > f4 + 3, f"{f4} -> {f5}")
        ca.ok("render", stop=True)

        # --- closing: the client's window, then the host -------------------------------------------
        ca.ok("window", op="close")
        pump(s, 1.0)
        st = s.invoke("remote") or {}
        check("closing the virtual window does not end the server",
                server.poll() is None and st.get("sharedWindows") == 0, str(st))

        client_b = rc.spawn_client(client_bin, share, token, spki,
                extra_env={"XL_CLIENT_CREATE_WINDOW": "own:320x240"})
        try:
            st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 1, timeout=40.0)
            check("a second client gets its virtual window",
                    any(w.get("virtual") for w in windows(s) if w.get("id") != host_id), str(st))
            s.ok("window", op="close", graceful=False)
            check("closing the host window ends the server, virtual windows or not",
                    wait_exit(server, 20.0), "the server is still running")
            check("and the client goes with it", wait_exit(client_b, 20.0),
                    "the client is still running")
        finally:
            rc.kill(client_b)
    finally:
        for sess in (ca, s):
            if sess:
                sess.close()
        rc.kill(client)
        rc.kill(server)
        for p in (sock, sock_a):
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

    triple = "x86_64-unknown-linux-gnu"
    server_bin = os.path.join(_here, "stappler-build", triple, "debug", "cc", "testapp")
    client_bin = os.path.join(_here, "client", "stappler-build", triple, "debug", "cc", "clientapp")
    if len(argv) > 0:
        server_bin = argv[0]
    if len(argv) > 1:
        client_bin = argv[1]
    for p in (server_bin, client_bin):
        if not os.path.exists(p):
            raise SystemExit(f"missing binary: {p}\nbuild it: xenolith-cli build tests/window")

    # The server listens without sharing its own window, and makes virtual windows for clients.
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_VIRTUAL_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"

    for gapi in gapis:
        run(server_bin, client_bin, gapi)

    print(f"{rc.checks} checks, {rc.failures} failures")
    print(f"logs: {rc.SERVER_LOG} {rc.CLIENT_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
