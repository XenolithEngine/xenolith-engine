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
import base64, importlib.util, io, os, secrets, sys, time

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check

# Per glyph, on the server's rasterizer: enough that a frame drawn before its glyphs is caught every
# time (without gating, 10 ms failed 6 runs of 6, none at all 5 of 6). Not more: the delay sleeps
# the server's workers, and at 50 ms a loaded machine sometimes lost the virtual window's pipelines
# for good (see os-examples-m6.2-plan.md).
GLYPH_DELAY_US = os.environ.get("XL_CHECK_GLYPH_DELAY_US", "10000")


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


def settle(s, name, quiet=1.0, timeout=10.0):
    """Wait until the window has presented nothing new for `quiet` seconds; the client's frames
    come by themselves, the host is only stepped."""
    last, since = presented(s, name), time.monotonic()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        pump(s, 0.1)
        now = presented(s, name)
        if now != last:
            last, since = now, time.monotonic()
        elif time.monotonic() - since >= quiet:
            return True
    return False


def session_counters(s):
    sessions = (s.invoke("remote") or {}).get("sessions", [])
    first = sessions[0] if sessions else {}
    return first.get("id"), first.get("readyRequests", 0), first.get("readyDeclined", 0)


def raw_shot(s, name):
    """The window's published frame, taken without asking the window for a frame."""
    shot = s.ok("screenshot", window=name) or {}
    return shot.get("data") or ""


def dark_pixels(data):
    """Pixels of the client's black label text: nothing else in its scene is that dark."""
    if not (isinstance(data, str) and data.startswith("BASE64:")):
        return 0
    raw = data[len("BASE64:"):]
    img = Image.open(io.BytesIO(base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4))))
    px = img.convert("RGB").tobytes()
    return sum(1 for i in range(0, len(px), 3) if px[i] < 60 and px[i + 1] < 60 and px[i + 2] < 60)


def plane(s, name):
    return s.ok("window", window=name, op="plane") or {}


def type_keys(s, name, text):
    events = []
    for ch in text:
        events.append({"event": "KeyPressed", "keycode": ch.upper(), "keychar": ord(ch)})
        events.append({"event": "KeyReleased", "keycode": ch.upper(), "keychar": ord(ch)})
    s.ok("input", window=name, native=True, events=events)


def wait_exit(proc, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            return True
        time.sleep(0.1)
    return False


def run(server_bin, client_bin, gapi):
    pid = os.getpid()
    # One server log per backend: the second run would otherwise overwrite the first.
    rc.SERVER_LOG = f"/tmp/xl-remote-check-server-{pid}-{gapi}.log"
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
        server = rc.start_server(server_bin, sock, share, token, gapi=gapi,
                extra_env={"XL_FONT_GLYPH_DELAY_US": GLYPH_DELAY_US})
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

        # --- the plane: published frames and the slots readers hold --------------------------------
        #
        # What a compositor reads: every presented image is published, and its slot stays out of the
        # ring while anybody holds the frame. `plane-hold` is a reader that is slow on purpose.
        p0 = plane(s, name)
        pump(s, 0.5)
        p1 = plane(s, name)
        check("presented frames are published to the plane",
                p1.get("imageCount") == 4 and p1.get("serial", 0) > p0.get("serial", 0)
                        and p1.get("published", 0) > p0.get("published", 0), f"{p0} -> {p1}")
        check("the latest frame's slot is pinned", p1.get("slot") in p1.get("pinned", []),
                str(p1))

        held = s.ok("window", window=name, op="plane-hold", ms=2500) or {}
        held_slot, held_serial = held.get("slot"), held.get("serial")
        newer, pinned_all_along = [], True
        f0 = presented(s, name)
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline:
            pump(s, 0.1)
            p = plane(s, name)
            pinned_all_along = pinned_all_along and held_slot in p.get("pinned", [])
            if p.get("serial", 0) > held_serial:
                newer.append((p.get("serial"), p.get("slot")))
        f1 = presented(s, name)
        check("while a frame is held, the client keeps drawing",
                f1 > f0 + 3 and len({it[0] for it in newer}) > 2, f"{f0} -> {f1}, {newer}")
        check("the held frame's slot stays pinned", pinned_all_along, str(plane(s, name)))
        check("and is never drawn into", all(slot != held_slot for _, slot in newer),
                f"held slot {held_slot}, newer frames {newer}")
        pump(s, 1.5)
        p = plane(s, name)
        check("let go, the slot returns to the ring",
                held_slot not in p.get("pinned", []) or p.get("slot") == held_slot, str(p))

        # Two slow readers and the latest frame: three slots out of four held, and the client still
        # draws through the one left.
        a = s.ok("window", window=name, op="plane-hold", ms=2500) or {}
        deadline = time.monotonic() + 5.0
        while plane(s, name).get("serial", 0) <= a.get("serial", 0) \
                and time.monotonic() < deadline:
            pump(s, 0.1)
        b = s.ok("window", window=name, op="plane-hold", ms=2500) or {}
        f0 = presented(s, name)
        most_pinned = 0
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline:
            pump(s, 0.1)
            most_pinned = max(most_pinned, len(plane(s, name).get("pinned", [])))
        f1 = presented(s, name)
        check("with three of four slots held the client draws through the fourth",
                a.get("slot") != b.get("slot") and most_pinned >= 3 and f1 > f0 + 3,
                f"held {a.get('slot')} and {b.get('slot')}, {most_pinned} pinned, {f0} -> {f1}")
        ca.ok("render", stop=True)

        # Frames drawn while others are held land in slots out of order. Typed text and a drop-down
        # change the picture while frames are held; a resize back and forth redraws everything from
        # scratch - the two pictures must agree. The square stops first, so that the scene is still
        # when both are taken.
        ca.invoke("client-animation", op="stop")
        check("the stopped client settles without a kick", settle(s, name), "frames kept coming")

        # --- the client asks for its frames itself ---------------------------------------------
        #
        # Nothing below steps the client's window: only the host is pumped. An idle client is quiet,
        # an animating one draws, and a change made from outside the client's own frames - an
        # AppNotify handled between frames - gets its frame. The server rasterizes glyphs slowly
        # here (XL_FONT_GLYPH_DELAY_US), so a frame drawn before its glyphs would show the text with
        # holes; the first frame must already be the complete picture.
        sid, req0, _ = session_counters(s)
        p0 = presented(s, name)
        pump(s, 3.0)
        _, req1, _ = session_counters(s)
        check("an idle client draws nothing and asks for nothing",
                presented(s, name) == p0 and req1 == req0,
                f"presented {p0} -> {presented(s, name)}, requests {req0} -> {req1}")

        ca.ok("render", seconds=2)
        p0 = presented(s, name)
        pump(s, 1.0)
        check("an animating client gets its frames without a kick", presented(s, name) > p0 + 3,
                f"{p0} -> {presented(s, name)}")
        settle(s, name)

        before = raw_shot(s, name)
        _, req0, _ = session_counters(s)
        p0 = presented(s, name)
        s.ok("invoke", name="remote-app-notify", args={"session": sid, "value": {"label": "Wqz"}})
        first = None
        deadline = time.monotonic() + 10.0
        while first is None and time.monotonic() < deadline:
            if presented(s, name) != p0:
                first = raw_shot(s, name)
            else:
                time.sleep(0.02)
        pump(s, 1.5)
        later = raw_shot(s, name)
        _, req1, _ = session_counters(s)
        label = (ca.invoke("client-state") or {}).get("label")
        check("a label changed by an AppNotify gets its frame without a kick",
                first is not None and label == "Wqz", f"label {label!r}")
        # The label is black text, the only thing that dark in the scene: the glyphs are there when
        # its pixels are, and complete when the picture does not change once they have all landed.
        check("and that first frame already has the new glyphs",
                first is not None and dark_pixels(first) > 500 and first == later
                        and first != before,
                f"{dark_pixels(first)} text pixels first, {dark_pixels(later)} later"
                if first else "no frame")
        check("the change cost a few requests, not a stream", req1 - req0 <= 3,
                f"{req1 - req0} requests")

        _, _, dec0 = session_counters(s)
        ca.invoke("client-ready", window=987654321)
        pump(s, 0.5)
        _, _, dec1 = session_counters(s)
        check("a frame asked for a window the session may not draw is declined", dec1 == dec0 + 1,
                f"declined {dec0} -> {dec1}")
        p0 = presented(s, name)
        s.ok("invoke", name="remote-app-notify", args={"session": sid, "value": {"label": "REMOTE"}})
        pump(s, 1.5)
        check("and the client still gets frames afterwards", presented(s, name) > p0,
                f"{p0} -> {presented(s, name)}")

        # The client's data identities cross the wire, so a character typed on a still scene is a
        # partial frame on the server. The first one brings every slot up to the same picture (the
        # held frames above left them with older ones); the second must then be partial in each.
        type_keys(s, name, "w")
        pump(s, 1.0)
        mark = len(open(rc.SERVER_LOG, errors="replace").read())
        type_keys(s, name, "v")
        pump(s, 1.0)
        typed = open(rc.SERVER_LOG, errors="replace").read()[mark:]
        word = "damage: partial redraw" if gapi == "vulkan" else "damage: repainting"
        check("a typed character is a partial frame of the client's window",
                typed.count(word) > 0 and typed.count("damage: full") == 0,
                f"{typed.count(word)} partial, {typed.count('damage: full')} full "
                f"({rc.SERVER_LOG})")

        # The rest goes into the middle of the text: each new glyph lands inside a box the label
        # already fills, so a repaint that only followed the box would have nothing to go on.
        s.ok("input", window=name, native=True, events=[
            {"event": "KeyPressed", "keycode": "LEFT"}, {"event": "KeyReleased", "keycode": "LEFT"},
            {"event": "KeyPressed", "keycode": "LEFT"}, {"event": "KeyReleased", "keycode": "LEFT"},
        ])
        pump(s, 0.4)
        s.ok("window", window=name, op="plane-hold", ms=700)
        type_keys(s, name, "k")
        pump(s, 0.4)

        for ch in "xyz":
            s.ok("window", window=name, op="plane-hold", ms=700)
            type_keys(s, name, ch)
            pump(s, 0.4)
            s.ok("window", window=name, op="plane-hold", ms=700)
            ca.invoke("client-popup", op="open" if ch != "z" else "close")
            pump(s, 0.4)
        pump(s, 1.5)
        shot_a = rc.grab(s, window=name)
        size = entry_of(s, name)
        w, h = size.get("width"), size.get("height")
        s.ok("window", window=name, op="resize", width=w + 40, height=h)
        pump(s, 1.0)
        s.ok("window", window=name, op="resize", width=w, height=h)
        pump(s, 1.5)
        shot_b = rc.grab(s, window=name)
        ca.invoke("client-animation", op="start")
        text = ca.invoke("client-text") or {}
        if shot_a != shot_b:
            for tag, shot in (("partial", shot_a), ("full", shot_b)):
                with open(f"/tmp/xl-virtual-window-{gapi}-{tag}.png", "wb") as f:
                    f.write(shot)
        check("frames drawn while others are held leave the same picture as a full redraw",
                shot_a.startswith(b"\x89PNG") and shot_a == shot_b,
                f"{len(shot_a)} vs {len(shot_b)} bytes, text {text.get('text')!r}")
        check("and the screenshot is the published frame", len(shot_a) > 2000,
                f"{len(shot_a)} bytes")

        r = s.ok("window", window=name, op="offscreen") or {}
        check("an offscreen frame of a virtual window completes", r.get("completed") is True,
                str(r))

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
    # A window manager's clients draw through the light queue, the one with partial redraw; the
    # client hides its frame counter so that its scene can be still.
    os.environ["XL_FLAT_QUEUE"] = "1"
    os.environ["XL_HIDE_FPS"] = "1"
    os.environ["XL_VK_DAMAGE_LOG"] = "1"
    os.environ["XL_SOFT_DAMAGE_LOG"] = "1"

    for gapi in gapis:
        run(server_bin, client_bin, gapi)
        if gapi == "vulkan":
            log = open(rc.SERVER_LOG, errors="replace").read()
            check("no Vulkan validation error on the server", "Validation Error" not in log,
                    rc.SERVER_LOG)

    print(f"{rc.checks} checks, {rc.failures} failures")
    print(f"logs: {rc.SERVER_LOG} {rc.CLIENT_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
