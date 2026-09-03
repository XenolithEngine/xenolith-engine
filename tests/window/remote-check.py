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

    tests/window/remote-check.py [--transport quic|unix] [path-to-testapp] [path-to-clientapp]

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


def start_server(binary, addr, share, token):
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
    proc = subprocess.Popen([binary, "--headless", "--width", "800", "--height", "600"],
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


def spawn_client(binary, share, token, spki, inspector=None):
    env = dict(os.environ)
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
    argv = [a for a in sys.argv[1:]]
    if argv and argv[0] == "--transport":
        transport = argv[1]
        argv = argv[2:]
    elif argv and argv[0].startswith("--transport="):
        transport = argv[0].split("=", 1)[1]
        argv = argv[1:]
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
        print(f"transport: {transport}\nserver: {server_bin}\nclient: {client_bin}\n"
                f"share:  {share}")
        server = start_server(server_bin, sock, share, token)
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

        # --- the real client ---------------------------------------------------------------------
        client = spawn_client(client_bin, share, token, spki, inspector=client_sock)
        st = wait_for(s, lambda x: x.get("clientConnected"), timeout=25.0)
        check("the client is connected and authenticated", bool(st.get("clientConnected")), str(st))

        dump = s.ok("logs") or {}
        lines = "\n".join(str(x) for x in (dump.get("lines") or []))
        check("server logged the authentication", "client authenticated" in lines,
                "not in %d log lines" % len(dump.get("lines") or []))

        # The client's own scene graph, over the client's own inspector: proof that the process on
        # the other end really built a scene, and which one.
        tree = ""
        for _ in range(100):
            try:
                cs = Session(client_sock, timeout=5.0)
                tree = cs.ok("scene") or ""
                cs.close()
                break
            except OSError:
                time.sleep(0.1)
        if isinstance(tree, dict):
            tree = tree.get("text", "")
        # ClientScene draws a 256x256 blue square and a 100x120 cursor region; the server's scene has
        # neither. Finding them in the CLIENT's own tree is what proves the far end really built the
        # scene it is supposed to render.
        check("the client built its own scene", "sz=256.0x256.0" in tree,
                (tree or "<no answer>")[:160].replace("\n", " | "))
        check("the client scene carries the cursor region", "sz=100.0x120.0" in tree,
                (tree or "<no answer>")[:160].replace("\n", " | "))

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
        # KNOWN DEFECT, deliberately left failing rather than removed. D13 in the plan.
        #
        # Routing is NOT the problem, and the logs say so: after the takeover exactly one frame is
        # handed to the remote client and none to the server. That one frame never completes -- the
        # client forwards its runtime material with a gating font-atlas dependency and the frame
        # waits on it -- so presentation never advances and the swapchain keeps the last frame the
        # server drew. Every later step_frame finds a frame still in flight and schedules nothing.
        #
        # Hence: identical bytes here mean the first remote frame is stuck, not that the capture
        # path is wrong. Fixing it is the forwarded-material/dependency path, not the screenshot.
        check("the captured frame reflects the client that took the window over", after != before,
                "byte-identical: the first remote frame never completed (stuck on its material dep)")

        shot = f"/tmp/xl-remote-check-{os.getpid()}.png"
        if after:
            with open(shot, "wb") as f:
                f.write(after)
            print(f"  screenshot: {shot} ({len(after)} bytes)")

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
