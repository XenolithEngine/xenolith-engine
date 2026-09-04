#!/usr/bin/env python3
"""End-to-end check of the remote render session: server headless, client a separate process.

WHAT THIS PROVES THAT NOTHING ELSE DOES. Every other check in this directory drives one process.
The remote path is two, split the way X11 splits them: the server owns the window, the GPU and the
input; the client owns the scene graph and produces the frames. Between them sits the whole
transport - QUIC handshake, bearer key, SPKI pin, the message codec, the queue mirror, the frame
protocol - and NONE of it is exercised by a single-process run. It compiles, and that is all a build
can tell you.

So this starts a real server, hands its address, token and certificate fingerprint to a real client,
and then asserts the things a screenshot cannot:

  * the listener came up and reported an SPKI fingerprint (the client has something to pin);
  * the client got through the handshake and holds the connection slot (server side);
  * the client compiled the shared queue and attached it, so the server routes frames to it;
  * frames actually flow: the server's window keeps presenting while the client renders it;
  * a client started with the WRONG token is refused - the bearer key is load-bearing, not decorative.

    tests/window/remote-check.py [--transport quic|unix] [--gapi vulkan|soft|gles]
                                [path-to-testapp] [path-to-clientapp]

`--gapi` runs the server on another backend (the backend has to be linked in:
`SOFT=1 xenolith-cli build tests/window` for `--gapi soft`). The client's scene does not change and is not
recompiled: it asks for a queue by what it needs, and the server's queues say what they are (gAPI
and shape), so the same client renders through a Vulkan server's shadow queue or a Software server's
flat one. Before that, the client recognised one hardcoded queue NAME and a Software server could
not be shared at all.

`--transport unix` runs the identical scenario over an AF_UNIX socket instead of QUIC. That is the
point of the transport abstraction and the only way to show it holds: the protocol above the seam
does not change a line, and the two runs differ only in how the peer is authenticated -- a pinned
SPKI for QUIC, kernel-vouched credentials (SO_PEERCRED) for unix, where the bearer key is not
required at all.

Prints "N checks, M failures"; exit status is the result.
"""
import base64, json, os, secrets, socket, struct, subprocess, sys, time


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
        try:
            self.s.close()
        except OSError:
            pass


# Both processes' output, kept: a two-process failure is not diagnosable without them.
SERVER_LOG = f"/tmp/xl-remote-check-server-{os.getpid()}.log"
CLIENT_LOG = f"/tmp/xl-remote-check-client-{os.getpid()}.log"

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


def start_server(binary, addr, share, token, gapi=None):
    env = dict(os.environ)
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    env["XL_REMOTE_SHARE"] = share
    env["XL_REMOTE_TOKEN"] = token
    # The counter must be OFF. It is AlwaysDirty and its text changes every frame, so with it on any
    # two screenshots differ -- which would make the before/after comparison below pass for a reason
    # that has nothing to do with the client. (XL_HIDE_FPS hides the counter for any value but "0".)
    env["XL_HIDE_FPS"] = "1"
    try:
        os.unlink(addr)
    except OSError:
        pass
    # cwd matters: bundled resources are looked up relative to it, and a server started elsewhere
    # renders with missing textures (see "Bundled resources still have to be found" in gui-debug).
    cmd = [binary, "--headless", "--width", "800", "--height", "600"]
    if gapi:
        cmd += ["--gapi", gapi]
    proc = subprocess.Popen(cmd,
            env=env, cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(SERVER_LOG, "w"), stderr=subprocess.STDOUT)
    for _ in range(600):
        if proc.poll() is not None:
            raise SystemExit(f"server exited early with {proc.returncode}")
        if os.path.exists(addr):
            try:
                Session(addr).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("server did not come up")


def wait_for(session, predicate, timeout=20.0, step=0.1):
    """Poll the `remote` command until `predicate(status)` holds. Returns the last status."""
    deadline = time.monotonic() + timeout
    status = {}
    while time.monotonic() < deadline:
        status = session.invoke("remote") or {}
        if predicate(status):
            return status
        # The server only advances a headless frame when asked, and the listener is pumped from the
        # app update tick - so keep stepping while waiting, or a client can never be accepted.
        session.ok("frame", count=1)
        time.sleep(step)
    return status


def spawn_client(binary, share, token, spki, inspector=None, extra_env=None):
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    if inspector:
        # The client is a full app with an inspector of its own; pointing it at a private socket is
        # what lets this check look at the CLIENT's scene graph rather than infer it.
        env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + inspector
        try:
            os.unlink(inspector)
        except OSError:
            pass
    return subprocess.Popen([binary, share, token, spki], env=env,
            cwd=os.path.dirname(os.path.abspath(binary)) or None,
            stdout=open(CLIENT_LOG, "a"), stderr=subprocess.STDOUT)


def grab(session):
    """One screenshot as PNG bytes. Steps first: headless renders only when asked."""
    session.ok("frame", count=3)
    time.sleep(0.3)
    img = session.ok("screenshot") or {}
    data = img.get("data") or ""
    if isinstance(data, str) and data.startswith("BASE64:"):
        raw = data[len("BASE64:"):]
        return base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4))
    return b""


def kill(proc):
    if proc and proc.poll() is None:
        proc.kill()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass


def main():
    transport = "quic"
    gapi = None
    argv = [a for a in sys.argv[1:]]
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
    if transport not in ("quic", "unix"):
        raise SystemExit(f"unknown transport: {transport}")

    root = os.path.dirname(os.path.abspath(__file__))
    triple = "x86_64-unknown-linux-gnu"
    server_bin = os.path.join(root, "stappler-build", triple, "debug", "cc", "testapp")
    client_bin = os.path.join(root, "client", "stappler-build", triple, "debug", "cc", "clientapp")

    if len(argv) > 0:
        server_bin = argv[0]
    if len(argv) > 1:
        client_bin = argv[1]

    for p in (server_bin, client_bin):
        if not os.path.exists(p):
            raise SystemExit(f"missing binary: {p}\nbuild it: xenolith-cli build tests/window")

    sock = f"/tmp/xl-remote-check-{os.getpid()}.sock"
    client_sock = f"/tmp/xl-remote-check-client-{os.getpid()}.sock"
    # An ephemeral port would be better, but the listener does not report the port it bound; a
    # per-run high port keeps two concurrent runs from colliding in practice.
    if transport == "unix":
        # No port, no certificate: the path IS the endpoint and its permissions are the access control.
        share = f"unix:/tmp/xl-remote-check-{os.getpid()}.xlsock"
    else:
        port = 24000 + (os.getpid() % 20000)
        share = f"quic://127.0.0.1:{port}"
    token = secrets.token_hex(16)

    server = None
    client = None
    bad_client = None
    try:
        print(f"transport: {transport}\ngapi:   {gapi or 'default'}\nserver: {server_bin}\n"
                f"client: {client_bin}\nshare:  {share}")
        server = start_server(server_bin, sock, share, token, gapi=gapi)
        s = Session(sock)
        s.ok("frame", count=3)

        status = wait_for(s, lambda st: st.get("listening"))
        check("listener is up", bool(status.get("listening")), str(status))

        spki = status.get("spki") or ""
        if transport == "quic":
            check("listener reports an SPKI fingerprint", len(spki) == 64, f"got {spki!r}")
        else:
            # A unix socket has no certificate to pin, and must not invent one: the peer is
            # identified by credentials instead.
            check("a unix listener reports no certificate", spki == "", f"got {spki!r}")
        if not status.get("listening"):
            raise SystemExit("the server never started sharing - nothing left to check")

        # A frame of the server rendering its OWN scene, to compare against later. This is the
        # only way to tell "the client connected" from "the client is what draws the window": both
        # look identical in every status field.
        before = grab(s)
        check("the server renders before any client", before.startswith(b"\x89PNG"),
                f"{len(before)} bytes")

        # --- what identity means on this transport --------------------------------------------
        #
        # The two transports authenticate differently, so the SAME scenario has opposite expected
        # outcomes here, and that difference IS the policy under test:
        #
        #   quic -- the bearer key is the only identity there is, so a wrong token must be refused;
        #   unix -- SO_PEERCRED already told the server which uid is on the other end, which is
        #           stronger than a shared secret, so the key is not required and a wrong token must
        #           be accepted. Access control there is the socket's filesystem permissions.
        if transport == "quic":
            bad_client = spawn_client(client_bin, share, token + "-wrong", spki)
            st = wait_for(s, lambda x: x.get("clientConnected"), timeout=6.0)
            check("a client with the wrong token is refused", not st.get("clientConnected"), str(st))
            kill(bad_client)
            bad_client = None
            # Let the refusal's cool-off (250ms after one failure) lapse before the real client.
            for _ in range(10):
                s.ok("frame", count=1)
                time.sleep(0.1)
        else:
            # Deliberately wrong for the whole run: if the session below works end to end anyway,
            # the bearer key genuinely played no part and the credentials did.
            token = token + "-deliberately-wrong"

        # --- a client whose build cannot exchange raw structs with this server -------------------
        #
        # Until the wire format is build-independent, InputEvents and UpdateLayers are raw
        # struct dumps: a build mismatch is memory corruption in one of the two processes, not a
        # message somebody rejects. So the server asks who the client is BEFORE announcing
        # anything, and refuses on a mismatched ABI tag.
        #
        # XL_REMOTE_FAKE_ABI is what makes this runnable at all -- two binaries built from one tree
        # necessarily agree, so the rejection path would otherwise be a claim nobody had executed.
        bad_client = spawn_client(client_bin, share, token, spki,
                extra_env={"XL_REMOTE_FAKE_ABI": "0123456789abcdef"})
        st = wait_for(s, lambda x: x.get("clientConnected"), timeout=8.0)
        check("a client with a different ABI does not hold the connection",
                not st.get("clientConnected"), str(st))
        dump = s.ok("logs") or {}
        lines = "\n".join(str(x) for x in (dump.get("lines") or []))
        check("the server records that the client refused the session",
                "client refused ServerInfo" in lines,
                "not in %d log lines" % len(dump.get("lines") or []))
        # The client is the side that refuses in this scenario, and that is the honest order: it
        # computes the same tag from the same facts, so it sees the mismatch the moment ServerInfo
        # arrives -- before the server has announced anything or sent it a single raw struct.
        bad_log = open(CLIENT_LOG).read() if os.path.exists(CLIENT_LOG) else ""
        check("the client says WHY, naming both builds",
                "incompatible server build" in bad_log and "server:" in bad_log
                        and "client:" in bad_log,
                bad_log[-200:].replace("\n", " | "))
        kill(bad_client)
        bad_client = None
        for _ in range(10):
            s.ok("frame", count=1)
            time.sleep(0.1)

        # --- the real client ---------------------------------------------------------------------
        client = spawn_client(client_bin, share, token, spki, inspector=client_sock)
        st = wait_for(s, lambda x: x.get("clientConnected"), timeout=25.0)
        check("the client is connected and authenticated", bool(st.get("clientConnected")), str(st))

        dump = s.ok("logs") or {}
        lines = "\n".join(str(x) for x in (dump.get("lines") or []))
        check("server logged the authentication", "client authenticated" in lines,
                "not in %d log lines" % len(dump.get("lines") or []))

        # One long-lived session to the CLIENT's own inspector. It used to be opened and closed for
        # a single `scene` call; from here on the driver talks to both sides throughout, and the
        # window-control checks below need the client's socket to outlive one request.
        c = None
        for _ in range(100):
            try:
                c = Session(client_sock, timeout=25.0)
                break
            except OSError:
                time.sleep(0.1)
        if c is None:
            raise SystemExit("the client never opened its inspector socket")

        tree = c.ok("scene") or ""
        if isinstance(tree, dict):
            tree = tree.get("text", "")
        # ClientScene draws a 256x256 blue square and a 100x120 cursor region; the server's scene has
        # neither. Finding them in the CLIENT's own tree is what proves the far end really built the
        # scene it is supposed to render.
        check("the client built its own scene", "sz=256.0x256.0" in tree,
                (tree or "<no answer>")[:160].replace("\n", " | "))
        check("the client scene carries the cursor region", "sz=100.0x120.0" in tree,
                (tree or "<no answer>")[:160].replace("\n", " | "))

        # --- the client knows what it is drawing for --------------------------------------------
        #
        # The scene runs here but the window, the GPU and the OS are the server's. Before this the
        # client knew nothing about the far end at all, so anything platform-shaped in a scene was a
        # local `#if` that is simply wrong when the scene is remote.
        client_log = open(CLIENT_LOG).read() if os.path.exists(CLIENT_LOG) else ""
        server_ident = ""
        for line in client_log.splitlines():
            if "ClientAppThread: server:" in line:
                server_ident = line.split("ClientAppThread: server:", 1)[1].strip()
        check("the client learned who the server is", bool(server_ident), "no identity line logged")
        expect_api = {"soft": "Software", "gles": "GLES", "webgpu": "WebGPU"}.get(gapi, "Vulkan")
        check("the server identity names its gAPI, OS and window system",
                all(x in server_ident for x in ("Linux", expect_api, "headless")),
                server_ident[:160])

        # --- the queue was chosen by what it IS -------------------------------------------------
        #
        # The server's shared queue is named "SharedWindowQueue". The client renders through it, so
        # it found it -- and the name occurs NOWHERE in the client, which is the whole claim: the
        # two sides no longer agree on a string neither protocol nor build enforces.
        client_src = os.path.join(root, "client")
        hits = []
        for dirpath, dirnames, filenames in os.walk(client_src):
            dirnames[:] = [d for d in dirnames if d != "stappler-build"]
            for fn in filenames:
                if not fn.endswith((".cpp", ".h", ".cc", ".css")):
                    continue
                full = os.path.join(dirpath, fn)
                with open(full, "r", errors="replace") as f:
                    if "SharedWindowQueue" in f.read():
                        hits.append(os.path.relpath(full, root))
        check("the client never names the server's queue", not hits, str(hits))

        # --- the window's own mirrors on the client ----------------------------------------------
        #
        # Two DIFFERENT claims, and the checks keep them apart on purpose. The client's SCENE has
        # always followed a server resize -- FrameConstraints ride in every AcquireFrame and the
        # client's Director applies them exactly as a local one does. What never followed is what
        # the client's WINDOW says about itself: getConstraints() answered the announce-time value
        # for the life of the session, and getWindowGeometry() answered zeros because the announce
        # did not carry geometry at all.
        def geom(sess):
            g = sess.ok("window", op="geometry") or {}
            return (g.get("width"), g.get("height"), g.get("hasPosition"))

        def extent(sess):
            g = sess.ok("window", op="constraints") or {}
            return (g.get("width"), g.get("height"))

        check("the client knows where its window is", geom(c) == (800, 600, True), str(geom(c)))

        s.ok("window", op="resize", width=640, height=480)
        # Headless renders on demand and the listener is pumped from the app update tick, so the
        # push needs frames and a moment to cross. Same dosing as geometry-check.py.
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)

        check("a server resize reaches the client's constraints mirror", extent(c) == (640, 480),
                str(extent(c)))
        check("a server resize reaches the client's geometry mirror", geom(c)[:2] == (640, 480),
                str(geom(c)))

        # REGRESSION GUARD, not a proof: this passes without any of the above, through the
        # per-frame constraints path. It is here so that touching the mirror cannot break the
        # relayout that already worked.
        tree = c.ok("scene") or ""
        if isinstance(tree, dict):
            tree = tree.get("text", "")
        check("the client scene still relayouts (guard, passes without M4)",
                "sz=640.0x480.0" in tree, (tree or "<no answer>")[:200].replace("\n", " | "))

        # Back, so the rest of the run sees the size it expects -- and so a one-shot latch would
        # show up here rather than passing unnoticed.
        s.ok("window", op="resize", width=800, height=600)
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)
        check("the mirrors follow a second resize", extent(c) == (800, 600) and geom(c)[:2] == (800, 600),
                f"{extent(c)} {geom(c)}")

        # --- frame telemetry (M4.4) ---------------------------------------------------------------
        #
        # getFrameTiming() is a synchronous by-value getter, so a remote client cannot answer it with
        # a request -- it needs a mirror fed by a push. Until it had one, Director::getAvgFps fell
        # back to its "no data" sentinel and the client's own FPS panel read "FPS: 1.0 SPF: 1.0 /
        # GPU: 1.0 (1.0) / Ver: 0.0". Those are the numbers this check refuses to accept.
        for _ in range(4):
            s.ok("frame", count=2)
            time.sleep(0.2)
        st = c.invoke("client-stats") or {}
        check("the client received real frame timing", st.get("avgFrameInterval", 0) > 0, str(st))
        check("the client received real draw statistics",
                (st.get("drawVertexes", 0) > 0) or (st.get("pixelsTotal", 0) > 0), str(st))
        if gapi == "soft":
            # Only a CPU rasterizer reports these, and the number is one the client cannot know by
            # any other route: it is the server's current surface area.
            check("the draw stat is the SERVER's, not an invention",
                    st.get("pixelsTotal") == 800 * 600, str(st.get("pixelsTotal")))

        # --- window control, client -> server (M4.3) ----------------------------------------------
        #
        # The strongest single check in this milestone: a resize asked for by the REMOTE SCENE.
        # It exercises the whole loop at once -- the client validates locally, sends WindowControl,
        # the server actually resizes its window, and the change comes back to the client's mirrors.
        # Before M4 `RemoteWindow` inherited the base setWindowExtent, which answered
        # ErrorNotSupported without the request ever leaving the process.
        #
        # It also settles the one scheduling question this driver had: this call BLOCKS python while
        # the server's app thread has to run to answer it. The server is pumped by its own update
        # timer as well as by socket readiness, so it does not need us to step frames meanwhile --
        # but that was a belief until this check passed.
        c.ok("window", op="resize", width=720, height=560)
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)

        check("a resize asked for by the remote scene reaches the server", extent(s) == (720, 560),
                str(extent(s)))
        check("and comes back to the client's mirror", extent(c) == (720, 560), str(extent(c)))

        # What the two sides agree may be changed at all. The rules moved onto the shared channel
        # base in this milestone exactly so these cannot differ; a base reading the wrong mirror
        # would show up here as two different numbers rather than as a mysterious refusal.
        #
        # Both answer 0 on this stand, and that is the CORRECT answer rather than a stub: the
        # headless window grants Allowed* only for the WindowCreationFlags the app asked for, and
        # this one asks for none. What is new is that the client can answer the question at all --
        # the method did not exist on its channel before M4.
        su = (s.ok("window", op="updatable") or {}).get("bits")
        cu = (c.ok("window", op="updatable") or {}).get("bits")
        check("both sides agree on which window states are updatable", su == cu and su is not None,
                f"server={su} client={cu}")

        # And the live window state, which is not zero: the client mirrors it from the forwarded
        # WindowState events. Together with the line above this says both inputs to the shared rule
        # -- capabilities and state -- are the same on both sides, so agreeing on the OUTPUT is not
        # two stubs agreeing on nothing.
        ss = (s.ok("window", op="state") or {}).get("bits")
        cs_ = (c.ok("window", op="state") or {}).get("bits")
        check("both sides see the same live window state", ss == cs_ and ss, f"server={ss} client={cs_}")

        # Paired with the check above, this says the refusal is for the SAME reason on both sides
        # rather than being a stub that always says no. Headless has no Fullscreen capability, so
        # the honest answer is a refusal -- from the client without the request leaving it.
        sf = (s.ok("window", op="enable-state", state="Fullscreen") or {}).get("accepted")
        cf = (c.ok("window", op="enable-state", state="Fullscreen") or {}).get("accepted")
        check("an unsupported state is refused identically on both sides",
                sf is False and cf is False, f"server={sf} client={cf}")

        s.ok("window", op="resize", width=800, height=600)
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)

        # --- text input, both directions (M4.5) ---------------------------------------------------
        #
        # The state belongs to the IME on the OS side, never to the application: a scene only
        # REQUESTS a state, and what it gets back is the answer. That contract is what these checks
        # hold the remote path to -- nothing below reads a value the client wrote for itself.
        st = c.invoke("client-text") or {}
        check("the client's field took focus", st.get("focused") is True, str(st))

        # Driven from the CLIENT: performTextInput -> the server's processor -> echo -> the widget.
        # Before M4 performTextInput fell through to the base no-op and nothing happened at all.
        c.ok("text", op="insert", text="Hi")
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)
        st = c.invoke("client-text") or {}
        check("text typed by the remote scene comes back through the server",
                st.get("text") == "Hi", str(st))

        # Driven from the SERVER, as a keyboard would. This is the only check that proves
        # acquireTextInput actually crossed the wire: native input reaches the processor first, and
        # the processor only claims printable keys while text input is ENABLED -- which it is only
        # because the client asked for it. Without that, "ab" would arrive as plain key events and
        # the field would stay "Hi".
        s.ok("input", native=True, events=[
            {"event": "KeyPressed", "keycode": "A", "keychar": ord("a")},
            {"event": "KeyReleased", "keycode": "A", "keychar": ord("a")},
            {"event": "KeyPressed", "keycode": "B", "keychar": ord("b")},
            {"event": "KeyReleased", "keycode": "B", "keychar": ord("b")},
        ])
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)
        st = c.invoke("client-text") or {}
        check("keys typed at the SERVER reach the remote field (so acquireTextInput crossed)",
                st.get("text") == "Hiab", str(st))

        # Composition, which no keystroke can express -- and the codec's hard case: non-ASCII text
        # whose UTF-16 length differs from its UTF-8 length, with a marked range indexing it.
        c.ok("text", op="marked", text="にほ", markedStart=4, markedLength=2)
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)
        st = c.invoke("client-text") or {}
        check("composition survives the round trip, cursors included",
                st.get("text") == "Hiabにほ" and st.get("markedLength") == 2, str(st))

        c.ok("text", op="unmark")
        for _ in range(6):
            s.ok("frame", count=1)
            time.sleep(0.15)
        st = c.invoke("client-text") or {}
        check("unmarking commits the composed text", st.get("text") == "Hiabにほ"
                        and st.get("markedLength") == 0, str(st))

        # Frames must keep flowing while the client renders.
        s.ok("frame", count=5)
        time.sleep(0.5)
        st = s.invoke("remote") or {}
        check("the client is still connected after rendering", bool(st.get("clientConnected")),
                str(st))
        check("the client process is still alive", client.poll() is None,
                f"exited with {client.returncode}")

        # THE assertion: after the takeover the window's content is produced by the client, so the
        # frame must no longer be the server's own scene. Comparing the encoded bytes is enough --
        # the two scenes are not remotely similar (a button column against a blue square + "REMOTE").
        after = grab(s)
        check("a frame was captured after the takeover", after.startswith(b"\x89PNG"),
                f"{len(after)} bytes")
        # THE assertion, and the one that found D13/D14: identical bytes mean the window is still
        # showing the last frame the SERVER drew, i.e. the first remote frame never completed. Only
        # a real capture can tell that apart from a healthy session -- every status field looks the
        # same either way.
        check("the captured frame reflects the client that took the window over", after != before,
                "byte-identical: the first remote frame never completed")

        shot = f"/tmp/xl-remote-check-{os.getpid()}.png"
        if after:
            with open(shot, "wb") as f:
                f.write(after)
            print(f"  screenshot: {shot} ({len(after)} bytes)")

        # --- and the remote scene closes the window it was drawing into -----------------------
        #
        # Last, because it ends the session. `RemoteWindow::close` was an empty body: a scene could
        # ask to be closed and simply be ignored. This replaces the driver's own `quit` -- shutting
        # the server down from the far end is strictly the stronger way to end the run.
        #
        # The server answers the request BEFORE it closes, or the reply would die with the
        # connection and the client would sit out its deadline for a window that had already gone.
        c.ok("window", op="close")
        closed = False
        for _ in range(100):
            if server.poll() is not None:
                closed = True
                break
            time.sleep(0.1)
        check("the remote scene can close the window it draws into", closed,
                "the server was still running 10s later")
        if not closed:
            # Report the failure rather than hanging on it.
            s.ok("quit")
        s.close()
    finally:
        kill(bad_client)
        kill(client)
        kill(server)
        for path in (sock, client_sock, share[5:] if share.startswith("unix:") else ""):
            if not path:
                continue
            try:
                os.unlink(path)
            except OSError:
                pass

    print(f"logs: {SERVER_LOG} {CLIENT_LOG}")
    print(f"{checks} checks, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
