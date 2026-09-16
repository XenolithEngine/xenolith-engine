#!/usr/bin/env python3
"""End-to-end check of a server serving several remote clients at once.

remote-check.py proves one client end to end; this proves the server keeps clients apart. It starts
the server with XL_REMOTE_MAX_CLIENTS=2 and asserts:

  * the first client takes the primary window, and a second client connected next to it is not
    offered that window at all;
  * a window the server reserves for the second client reaches only that client, which draws it;
  * a third client is refused while both slots are taken;
  * a client that dies releases only its own windows: the other client keeps drawing and, the
    released window being unreserved, takes it;
  * a client arriving when every window is served is offered none, and takes them when their owner
    leaves -- whose reservations end with it.

    tests/window/remote-multi-check.py [--transport shm|unix|quic] [--gapi vulkan|soft]
                                      [path-to-testapp] [path-to-clientapp]

The helpers (server start, inspector sessions, screenshots) are remote-check.py's. A client is known
to the server by the pid its transport reports; on QUIC, which reports none, by arrival order.

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, secrets, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check", os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check
PRIMARY = rc.PRIMARY_WINDOW_ID
SECOND = rc.SECOND_WINDOW_ID


def window_entry(status, name):
    for w in status.get("windows") or []:
        if w.get("name") == name:
            return w
    return {}


def session_of(status, proc, known=()):
    """The session serving `proc`: by pid, or the newest one not in `known` when pids are unknown."""
    sessions = status.get("sessions") or []
    for it in sessions:
        if it.get("pid") == proc.pid:
            return it
    rest = [it for it in sessions if it.get("pid", -1) < 0 and it.get("id") not in known]
    return max(rest, key=lambda it: it.get("id")) if rest else {}


def open_inspector(path, timeout=20.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return rc.Session(path, timeout=25.0)
            except OSError:
                pass
        time.sleep(0.1)
    return None


def client_windows(session):
    return [w["id"] for w in (session.ok("windows") or {}).get("windows", [])]


def step(s, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        s.ok("frame", count=1)
        time.sleep(0.1)


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

    pid = os.getpid()
    sock = f"/tmp/xl-remote-multi-{pid}.sock"
    sock_a = f"/tmp/xl-remote-multi-a-{pid}.sock"
    sock_b = f"/tmp/xl-remote-multi-b-{pid}.sock"
    sock_c = f"/tmp/xl-remote-multi-c-{pid}.sock"
    if transport == "unix":
        share = f"unix:/tmp/xl-remote-multi-{pid}.xlsock"
    elif transport == "shm":
        share = f"shm:/dev/shm/xl-remote-multi-{pid}"
    elif transport == "quic":
        share = f"quic://127.0.0.1:{24000 + (pid % 20000)}"
    else:
        raise SystemExit(f"unknown transport: {transport}")
    token = secrets.token_hex(16)

    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"
    server = a = b = c = third = None
    try:
        print(f"transport: {transport}\ngapi:   {gapi or 'default'}\nshare:  {share}")
        server = rc.start_server(server_bin, sock, share, token, gapi=gapi)
        s = rc.Session(sock)
        s.ok("frame", count=3)
        status = rc.wait_for(s, lambda st: st.get("listening"))
        if not status.get("listening"):
            raise SystemExit("the server never started sharing")
        spki = status.get("spki") or ""
        before = rc.grab(s)

        # --- the first client takes the primary window ------------------------------------------
        a = rc.spawn_client(client_bin, share, token, spki, inspector=sock_a)
        st = rc.wait_for(s, lambda x: window_entry(x, PRIMARY).get("owner", 0) != 0, timeout=30.0)
        id_a = session_of(st, a).get("id")
        check("the first client takes the primary window",
                id_a is not None and window_entry(st, PRIMARY).get("owner") == id_a, str(st))
        step(s, 1.0)
        shot = rc.grab(s)
        check("and draws it", shot.startswith(b"\x89PNG") and shot != before,
                "the frame is still the server's own scene")
        ca = open_inspector(sock_a)
        if ca is None:
            raise SystemExit("the first client never opened its inspector")

        # --- a second client is not offered it -----------------------------------------------------
        b = rc.spawn_client(client_bin, share, token, spki, inspector=sock_b)
        st = rc.wait_for(s, lambda x: x.get("clients") == 2, timeout=30.0)
        check("a second client is accepted next to the first", st.get("clients") == 2, str(st))
        step(s, 2.0)
        st = s.invoke("remote") or {}
        id_b = session_of(st, b, known=(id_a,)).get("id")
        check("the second client has a session of its own", id_b is not None and id_b != id_a,
                str(st))
        check("the primary window stays with the first client",
                window_entry(st, PRIMARY).get("owner") == id_a, str(st))
        check("the second client serves no window",
                session_of(st, b, known=(id_a,)).get("windows") == [], str(st))

        # --- a window reserved for the second client ---------------------------------------------
        s.invoke("remote-assign", window=SECOND, session=id_b)
        s.invoke("remote-share-second")
        # Generous: a window shares itself when its scene is first PRESENTED, and a window opened
        # while another one is drawing flat out can wait a long time for that first frame -- the
        # windows of one process are served by a single thread and do not take turns (see M1).
        st = rc.wait_for(s, lambda x: window_entry(x, SECOND).get("owner") == id_b, timeout=90.0)
        check("the reserved window goes to the client it was reserved for",
                window_entry(st, SECOND).get("owner") == id_b
                        and window_entry(st, SECOND).get("assigned") == id_b, str(st))

        cb = open_inspector(sock_b)
        windows_b = client_windows(cb) if cb else []
        check("the second client has exactly that window", windows_b == [SECOND], str(windows_b))
        step(s, 1.0)
        windows_a = client_windows(ca)
        check("the first client never saw it", windows_a == [PRIMARY], str(windows_a))

        if cb:
            state = cb.ok("invoke", name="client-state", window=SECOND) or {}
            check("the second client mirrors the window it draws",
                    (state.get("constraintsWidth"), state.get("constraintsHeight")) == (320, 240),
                    str(state))
            step(s, 1.0)
            stats = cb.ok("invoke", name="client-stats", window=SECOND) or {}
            check("frames of that window reach the second client",
                    stats.get("avgFrameInterval", 0) > 0, str(stats))
        shot1 = rc.grab(s)
        shot2 = rc.grab(s, window=SECOND)
        check("both windows produce frames, each its own picture",
                shot1.startswith(b"\x89PNG") and shot2.startswith(b"\x89PNG") and shot1 != shot2,
                f"{len(shot1)} / {len(shot2)} bytes")

        # --- a frame started and abandoned (S5) ---------------------------------------------------
        #
        # The other deadline: the client answers AcquireFrame on time, so the server arms the frame
        # and holds the window for input -- and then sends none. Nothing used to time that out, so
        # the window stopped presenting for good. It is cancelled now, and the session is untouched.
        if cb:
            st = s.invoke("remote") or {}
            late_before = next((it.get("lateFrames", 0) for it in (st.get("sessions") or [])
                    if it.get("id") == id_b), 0)
            cb.invoke("client-frame-delay", frames=1, silent=True)
            late_after = late_before
            deadline = time.monotonic() + 20.0
            while time.monotonic() < deadline:
                s.ok("frame", count=1)
                time.sleep(0.2)
                st = s.invoke("remote") or {}
                late_after = next((it.get("lateFrames", 0) for it in (st.get("sessions") or [])
                        if it.get("id") == id_b), 0)
                if late_after > late_before:
                    break
            check("a frame whose input never arrives is given up on", late_after > late_before,
                    f"{late_before} -> {late_after} late frames")
            check("and its client keeps the session",
                    st.get("clients") == 2 and b.poll() is None, str(st))
            shot = rc.grab(s, window=SECOND)
            check("the window goes on presenting", shot.startswith(b"\x89PNG"),
                    f"{len(shot)} bytes")

        # --- a third client is refused ------------------------------------------------------------
        third = rc.spawn_client(client_bin, share, token, spki)
        for _ in range(40):
            s.ok("frame", count=1)
            time.sleep(0.1)
            if third.poll() is not None:
                break
        check("a third client is turned away while both slots are taken", third.poll() is not None,
                "still running after 4s")
        st = s.invoke("remote") or {}
        check("and the two sessions are untouched", st.get("clients") == 2, str(st))
        rc.kill(third)
        third = None

        # --- the first client dies ----------------------------------------------------------------
        ca.close()
        ca = None
        rc.kill(a)
        a = None
        st = rc.wait_for(s, lambda x: x.get("clients") == 1, timeout=15.0)
        check("a dead client frees its slot", st.get("clients") == 1, str(st))

        # Nobody reserved the primary window, so, freed, it is offered to every client again -- and
        # the one still connected takes it.
        st = rc.wait_for(s, lambda x: window_entry(x, PRIMARY).get("owner") == id_b, timeout=20.0)
        check("its window is offered again and the remaining client takes it",
                window_entry(st, PRIMARY).get("owner") == id_b, str(st))
        check("the other client keeps its reserved window",
                window_entry(st, SECOND).get("owner") == id_b, str(st))
        check("and is still running", b.poll() is None, f"exited with {b.poll()}")
        if cb:
            windows_b = []
            deadline = time.monotonic() + 15.0
            while time.monotonic() < deadline:
                step(s, 0.4)
                windows_b = sorted(client_windows(cb))
                if windows_b == sorted([PRIMARY, SECOND]):
                    break
            check("the remaining client now has both windows", windows_b == sorted([PRIMARY, SECOND]),
                    str(windows_b))
        shot = rc.grab(s, window=SECOND)
        check("the reserved window still produces frames", shot.startswith(b"\x89PNG"),
                f"{len(shot)} bytes")

        # --- a client arriving when everything is taken, then the owner leaves ----------------------
        c = rc.spawn_client(client_bin, share, token, spki, inspector=sock_c)
        st = rc.wait_for(s, lambda x: x.get("clients") == 2, timeout=30.0)
        step(s, 1.0)
        st = s.invoke("remote") or {}
        id_c = session_of(st, c, known=(id_a, id_b)).get("id")
        check("a new client is accepted into the freed slot", id_c is not None, str(st))
        check("and is offered nothing, every window being served",
                session_of(st, c, known=(id_a, id_b)).get("windows") == [], str(st))

        if cb:
            cb.close()
            cb = None
        rc.kill(b)
        b = None
        st = rc.wait_for(s, lambda x: window_entry(x, PRIMARY).get("owner") == id_c
                        and window_entry(x, SECOND).get("owner") == id_c, timeout=30.0)
        check("when the owner leaves, the waiting client takes both windows",
                window_entry(st, PRIMARY).get("owner") == id_c
                        and window_entry(st, SECOND).get("owner") == id_c, str(st))
        check("the reservation ended with the session it was for",
                window_entry(st, SECOND).get("assigned") == 0, str(st))
        step(s, 1.0)
        shot = rc.grab(s)
        check("and draws them", shot.startswith(b"\x89PNG") and shot != before,
                "the frame is the server's own scene")

        # --- late once is a frame, late always is a client (S5) -----------------------------------
        #
        # The limit is what keeps "a late frame is not fatal" from meaning "a client that never
        # draws is welcome forever": after a few in a row the session goes. Last, because it ends
        # the only session still running.
        cc = open_inspector(sock_c)
        if cc:
            cc.invoke("client-frame-delay", ms=3000, frames=8)
            gone = False
            deadline = time.monotonic() + 90.0
            while time.monotonic() < deadline:
                s.ok("frame", count=1)
                s.ok("frame", count=1, window=SECOND)
                time.sleep(0.2)
                st = s.invoke("remote") or {}
                if st.get("clients") == 0:
                    gone = True
                    break
            check("a client that is late again and again is dropped", gone, str(st))
            check("and the windows it served are free again",
                    window_entry(st, PRIMARY).get("owner") == 0
                            and window_entry(st, SECOND).get("owner") == 0, str(st))
            check("the server itself is untouched", server.poll() is None,
                    f"exited with {server.returncode}")
            try:
                cc.close()
            except OSError:
                pass
        if cb:
            cb.close()
    finally:
        for p in (a, b, c, third):
            rc.kill(p)
        rc.kill(server)
        for p in (sock, sock_a, sock_b, sock_c):
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
