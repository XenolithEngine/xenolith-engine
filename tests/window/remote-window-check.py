#!/usr/bin/env python3
"""End-to-end check of windows opened because a remote CLIENT asked for them.

remote-check.py proves one client rendering a window the server offered; remote-multi-check.py
proves the server keeps several clients apart. This proves the other direction: a client that
starts up with no window at all and asks the server for one.

  * the server listens while sharing nothing of its own -- the shape a window manager has;
  * a client asks for a window, the server creates it, reserves it for that client and nobody else
    is offered it;
  * the client renders into it, and a second window it asks for is named apart from the first;
  * closing one window does not close the session;
  * killing the client closes every window it asked for, and leaves the other client's alone;
  * a server with no handler for window requests refuses, and the refusal does not cost the session.

    tests/window/remote-window-check.py [--transport shm|unix|quic] [--gapi vulkan|soft] [--async-raster]
                                       [path-to-testapp] [path-to-clientapp]

Two facts about the stand shape the run. A process with no window has no inspector (it is a System
on a SceneContent), so the FIRST window a client has cannot be asked for from outside -- the client
asks for it at startup, driven by XL_CLIENT_CREATE_WINDOW, which is the milestone's story anyway.
And the server keeps its own window (that is what keeps the socket and the `remote` command alive)
but shares none of it, so "a server with no windows of its own" reads as `sharedWindows == 0`.

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, secrets, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)

check = rc.check


def windows_of(status):
    return status.get("windows") or []


def window_entry(status, name):
    for w in windows_of(status):
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


def open_inspector(path, timeout=25.0):
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
    return sorted(w["id"] for w in (session.ok("windows") or {}).get("windows", []))


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
        if opt == "--async-raster":
            rc.SERVER_FLAGS.append(opt)
            continue
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
    sock = f"/tmp/xl-remote-window-{pid}.sock"
    sock_a = f"/tmp/xl-remote-window-a-{pid}.sock"
    sock_b = f"/tmp/xl-remote-window-b-{pid}.sock"
    plain_sock = f"/tmp/xl-remote-window-plain-{pid}.sock"

    def endpoint(tag):
        if transport == "unix":
            return f"unix:/tmp/xl-remote-window-{tag}-{pid}.xlsock"
        if transport == "shm":
            return f"shm:/dev/shm/xl-remote-window-{tag}-{pid}"
        if transport == "quic":
            return f"quic://127.0.0.1:{24000 + (pid + (0 if tag == 'main' else 1)) % 20000}"
        raise SystemExit(f"unknown transport: {transport}")

    share = endpoint("main")
    token = secrets.token_hex(16)

    # The server listens but offers nothing of its own, and answers window requests and
    # application messages.
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"
    os.environ["XL_REMOTE_APP_ECHO"] = "1"

    label_sock = f"/tmp/xl-remote-window-label-{pid}.sock"
    server = plain_server = label_server = a = b = plain_client = None
    ca = cb = None
    label_clients = []
    try:
        print(f"transport: {transport}\ngapi:   {gapi or 'default'}\nshare:  {share}")
        server = rc.start_server(server_bin, sock, share, token, gapi=gapi, keep_running=True)
        s = rc.Session(sock)
        s.ok("frame", count=3)
        status = rc.wait_for(s, lambda st: st.get("listening"))
        check("the server listens while sharing nothing of its own",
                bool(status.get("listening")) and status.get("sharedWindows") == 0, str(status))
        spki = status.get("spki") or ""
        server_scene = rc.grab(s)

        # --- a client that starts with no window and asks for one ---------------------------------
        a = rc.spawn_client(client_bin, share, token, spki, inspector=sock_a,
                extra_env={"XL_CLIENT_CREATE_WINDOW": "own:400x300"})
        # The owner is set when the client attaches its queue, which comes after the window.
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 1
                and all(w.get("owner") for w in windows_of(x)), timeout=40.0)
        id_a = session_of(st, a).get("id")
        entry = windows_of(st)[0] if windows_of(st) else {}
        name_a = entry.get("name")
        check("a client's request becomes a window on the server",
                st.get("sharedWindows") == 1 and st.get("clientWindows") == 1, str(st))
        check("and it belongs to the session that asked",
                entry.get("creator") == id_a and entry.get("assigned") == id_a
                        and entry.get("owner") == id_a, str(entry))
        check("the server renamed it into its own namespace",
                isinstance(name_a, str) and name_a.endswith(".own") and name_a != "own",
                str(name_a))

        ca = open_inspector(sock_a)
        if ca is None:
            raise SystemExit("the client never opened its inspector - it has no window")
        check("the client has exactly the window it asked for", client_windows(ca) == [name_a],
                str(client_windows(ca)))
        state = ca.invoke("client-state") or {}
        check("with the size it asked for",
                (state.get("constraintsWidth"), state.get("constraintsHeight")) == (400, 300),
                str(state))

        step(s, 1.0)
        shot_a = rc.grab(s, window=name_a)
        check("and it is the client that draws it",
                shot_a.startswith(b"\x89PNG") and shot_a != server_scene,
                f"{len(shot_a)} bytes")
        check("asking never cost the session", bool((s.invoke("remote") or {}).get("clientConnected")),
                "the client is no longer connected")

        # --- popups on a client ----------------------------------------------------------------------
        #
        # A client opens no windows of its own, so a drop-down's list is an in-scene overlay there.
        # Before, the widget looked for a local window, found none and silently did nothing.
        before = rc.grab(s, window=name_a)
        r = ca.invoke("client-popup", op="open") or {}
        check("a drop-down opens on a client", bool(r.get("opened")) and bool(r.get("open")), str(r))
        check("as an in-scene overlay, not a window", not r.get("native")
                and client_windows(ca) == [name_a], f"{r} {client_windows(ca)}")
        step(s, 1.0)
        opened = rc.grab(s, window=name_a)
        check("and it is drawn", opened.startswith(b"\x89PNG") and opened != before,
                f"{len(opened)} bytes")
        r = ca.invoke("client-popup", op="close") or {}
        check("and closes again", not r.get("open"), str(r))
        r = ca.invoke("client-popup", op="tip") or {}
        check("a hint shows on a client", bool(r.get("tip")), str(r))

        # --- application messages, both ways -------------------------------------------------------
        state = ca.invoke("client-state") or {}
        check("the server takes application messages", bool(state.get("appMessaging")), str(state))
        greeting = [m for m in state.get("appMessages", [])
                if not m.get("request") and "hello" in (m.get("value") or {})]
        check("the session was greeted as it started", len(greeting) == 1, str(state))

        r = ca.invoke("client-app-request", value={"ping": 7}, timeout=5000) or {}
        reply = r.get("reply") or {}
        check("a client request is answered by the server's application",
                bool(r.get("ok")) and (reply.get("echo") or {}).get("ping") == 7
                        and reply.get("session") == id_a, str(r))

        ca.invoke("client-app-notify", value={"note": "from-a"})
        st = rc.wait_for(s, lambda x: any((n.get("value") or {}).get("note") == "from-a"
                for n in x.get("appNotifications", [])), timeout=10.0)
        note = next((n for n in st.get("appNotifications", [])
                if (n.get("value") or {}).get("note") == "from-a"), {})
        check("a client notification reaches the server with its session",
                note.get("session") == id_a, str(st.get("appNotifications")))

        r = s.invoke("remote-app-request", session=id_a, value={"q": 3}, timeout=5000) or {}
        check("a server request is answered by the client's application",
                bool(r.get("ok")) and ((r.get("reply") or {}).get("clientEcho") or {}).get("q") == 3,
                str(r))

        s.invoke("remote-app-notify", session=id_a, value={"push": 4})
        deadline = time.monotonic() + 10.0
        pushed = False
        while time.monotonic() < deadline and not pushed:
            step(s, 0.2)
            msgs = (ca.invoke("client-state") or {}).get("appMessages", [])
            pushed = any((m.get("value") or {}).get("push") == 4 and not m.get("request")
                    for m in msgs)
        check("a server notification reaches the client", pushed)

        # An answer later than the deadline is a timeout for the request, not for the session, and
        # the answer that does come afterwards is not taken for a new request.
        r = ca.invoke("client-app-request", value={"delay": 1500}, timeout=300) or {}
        check("an application request past its deadline times out",
                not r.get("ok") and "Timeout" in str(r.get("status")), str(r))
        step(s, 2.5)
        st = s.invoke("remote") or {}
        r = ca.invoke("client-app-request", value={"after": 1}, timeout=5000) or {}
        check("and the session outlives it",
                st.get("clients") == 1 and a.poll() is None and bool(r.get("ok")), f"{st} {r}")

        # --- a second client, with a window of its own --------------------------------------------
        b = rc.spawn_client(client_bin, share, token, spki, inspector=sock_b,
                extra_env={"XL_CLIENT_CREATE_WINDOW": "mine:320x240"})
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 2, timeout=40.0)
        id_b = session_of(st, b, known=(id_a,)).get("id")
        name_b = next((w.get("name") for w in windows_of(st) if w.get("name") != name_a), None)
        check("a second client gets a window of its own",
                st.get("sharedWindows") == 2 and window_entry(st, name_b).get("creator") == id_b,
                str(st))

        cb = open_inspector(sock_b)
        check("neither client is offered the other's window",
                client_windows(ca) == [name_a] and (cb is not None)
                        and client_windows(cb) == [name_b],
                f"a={client_windows(ca)} b={client_windows(cb) if cb else None}")

        # --- the same name twice -------------------------------------------------------------------
        r = ca.invoke("client-create-window", id="own", width=200, height=150) or {}
        check("a second request with the same name is granted a different one",
                bool(r.get("ok")) and isinstance(r.get("id"), str) and r.get("id") != name_a,
                str(r))
        name_a2 = r.get("id")
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 3, timeout=40.0)
        check("and it reaches the server as a third window",
                st.get("sharedWindows") == 3 and window_entry(st, name_a2).get("creator") == id_a,
                str(st))
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline and client_windows(ca) != sorted([name_a, name_a2]):
            step(s, 0.4)
        check("the client has both of its windows", client_windows(ca) == sorted([name_a, name_a2]),
                str(client_windows(ca)))

        # --- closing a window is not closing the session -------------------------------------------
        ca.ok("window", op="close", window=name_a2)
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) <= 2, timeout=30.0)
        check("a window the client closes goes away on the server",
                st.get("sharedWindows") == 2 and not window_entry(st, name_a2), str(st))
        check("and the session outlives it",
                st.get("clients") == 2 and a.poll() is None and client_windows(ca) == [name_a],
                str(client_windows(ca)))

        # --- a client that dies takes its windows, and only its own --------------------------------
        ca.close()
        ca = None
        rc.kill(a)
        a = None
        st = rc.wait_for(s, lambda x: x.get("clients") == 1 and x.get("sharedWindows", 0) <= 1,
                timeout=30.0)
        check("a dead client's windows are closed with it",
                st.get("sharedWindows") == 1 and not window_entry(st, name_a), str(st))
        check("the other client keeps its own",
                window_entry(st, name_b).get("creator") == id_b and b.poll() is None, str(st))
        step(s, 1.0)
        shot_b = rc.grab(s, window=name_b)
        check("which still produces frames", shot_b.startswith(b"\x89PNG"), f"{len(shot_b)} bytes")

        # --- open and close in a loop -------------------------------------------------------------
        #
        # Every window carries a compiled queue and the gAPI objects its encoding minted. They used
        # to be released only when the listener stopped, so a server that serves window requests
        # grew by a queue's worth of objects per window, forever.
        if cb:
            baseline = (s.invoke("remote") or {}).get("sharedObjects", 0)
            for i in range(3):
                r = cb.invoke("client-create-window", id=f"cycle{i}", width=200, height=150) or {}
                name = r.get("id")
                rc.wait_for(s, lambda x: window_entry(x, name), timeout=30.0)
                # The server lists the window before the client's scene for it is up, and the
                # close below is addressed to that scene.
                deadline = time.monotonic() + 30.0
                while name not in client_windows(cb) and time.monotonic() < deadline:
                    step(s, 0.2)
                cb.ok("window", op="close", window=name)
                rc.wait_for(s, lambda x: not window_entry(x, name), timeout=30.0)
            grown = (s.invoke("remote") or {}).get("sharedObjects", 0)
            check("a window opened and closed in a loop leaves nothing behind",
                    grown <= baseline, f"{baseline} -> {grown} shared objects after 3 cycles")

        # --- a server that does not serve window requests ------------------------------------------
        #
        # The refusal is a Status in the reply, not a protocol error, so what this checks is that the
        # client is told and STAYS: a policy answer must not cost the session.
        del os.environ["XL_REMOTE_CLIENT_WINDOWS"]
        plain_share = endpoint("plain")
        plain_token = secrets.token_hex(16)
        plain_server = rc.start_server(server_bin, plain_sock, plain_share, plain_token, gapi=gapi,
                keep_running=True)
        ps = rc.Session(plain_sock)
        ps.ok("frame", count=3)
        pst = rc.wait_for(ps, lambda x: x.get("listening"))
        plain_client = rc.spawn_client(client_bin, plain_share, plain_token, pst.get("spki") or "",
                extra_env={"XL_CLIENT_CREATE_WINDOW": "nope:320x240"})
        pst = rc.wait_for(ps, lambda x: x.get("clients") == 1, timeout=30.0)
        step(ps, 3.0)
        pst = ps.invoke("remote") or {}
        check("a server with no handler opens no window",
                pst.get("sharedWindows") == 0 and pst.get("clientWindows") == 0, str(pst))
        check("and the refused client keeps its session",
                pst.get("clients") == 1 and plain_client.poll() is None, str(pst))
        log = open(rc.CLIENT_LOG).read() if os.path.exists(rc.CLIENT_LOG) else ""
        check("the client says why it did not even ask",
                "does not open windows on request" in log, log[-200:].replace("\n", " | "))

        # --- keys with labels ----------------------------------------------------------------------
        #
        # A server that launches its clients hands each its own token, and the session is known by
        # the label of the key it presented -- on this transport too, which needs no key otherwise.
        os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
        os.environ["XL_REMOTE_LABELLED_KEYS"] = "1"
        label_share = endpoint("label")
        label_server = rc.start_server(server_bin, label_sock, label_share, secrets.token_hex(16),
                gapi=gapi, keep_running=True)
        del os.environ["XL_REMOTE_LABELLED_KEYS"]
        ls = rc.Session(label_sock)
        ls.ok("frame", count=3)
        lst = rc.wait_for(ls, lambda x: x.get("listening"))
        label_spki = lst.get("spki") or ""
        launch_token = secrets.token_hex(16)
        r = ls.invoke("remote-add-key", token=launch_token, label="app:one", single=True) or {}
        check("the server issues a labelled key", bool(r.get("ok")), str(r))

        stranger = rc.spawn_client(client_bin, label_share, secrets.token_hex(16), label_spki)
        label_clients.append(stranger)
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline and stranger.poll() is None:
            step(ls, 0.2)
        log = open(rc.CLIENT_LOG).read() if os.path.exists(rc.CLIENT_LOG) else ""
        check("a client without a labelled key is refused",
                stranger.poll() is not None and "handshake refused: AuthFailed" in log,
                log[-200:].replace("\n", " | "))
        step(ls, 1.0)  # past the refusal's backoff

        one = rc.spawn_client(client_bin, label_share, "", label_spki,
                extra_env={"XL_LAUNCH_TOKEN": launch_token, "XL_CLIENT_CREATE_WINDOW": "own:320x240"})
        label_clients.append(one)
        lst = rc.wait_for(ls, lambda x: x.get("clients") == 1 and x.get("sharedWindows", 0) >= 1,
                timeout=40.0)
        sess = (lst.get("sessions") or [{}])[0]
        check("the launched client is known by its label",
                sess.get("label") == "app:one" and lst.get("sharedWindows") == 1, str(lst))

        step(ls, 1.0)
        again = rc.spawn_client(client_bin, label_share, "", label_spki,
                extra_env={"XL_LAUNCH_TOKEN": launch_token})
        label_clients.append(again)
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline and again.poll() is None:
            step(ls, 0.2)
        lst = ls.invoke("remote") or {}
        check("a single-use key does not let a second client in",
                again.poll() is not None and lst.get("clients") == 1 and one.poll() is None,
                str(lst))
    finally:
        for sess in (ca, cb):
            if sess:
                sess.close()
        for p in [a, b, plain_client, server, plain_server, label_server] + label_clients:
            rc.kill(p)
        for p in (sock, sock_a, sock_b, plain_sock, label_sock):
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
