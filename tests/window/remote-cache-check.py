#!/usr/bin/env python3
"""End-to-end check of the FRAME DATA CACHE: the server keeps a remote client's data sets.

A remote client draws the same vertex sets frame after frame; only their transforms move. The server
keeps each set by the client's identity {id, generation} in a per-session cache, and the client
sends a reference instead of a set the server holds (xenolith/core/XLCoreFrameDataCache.h). The
client decides what the server holds - a mirror of the cache, evicting by last use within the budget
the server announced - and sends the changes in the same FrameInput as the references to them. This
proves it from the outside:

  static  clientapp with its square pulsing: every set is stored once and referenced from then on,
          and what a frame no longer carries is exactly the stored data - compared with a server
          that keeps nothing (XL_REMOTE_FRAME_DATA_BUDGET=0). The picture after the same typing is
          the same with and without the cache, a typed character is still a partial frame, and a
          reset (the server's answer to a reference it can not resolve) starts both sides over
          without breaking a frame;
  load    testapp as the client, switching layouts through a small budget: the client evicts, the
          server never misses, nothing grows past the budget, and the last layout looks the same as
          without the cache. When the client is gone, no entry is left alive.

    tests/window/remote-cache-check.py [--gapi vulkan|soft] [--stage static|load]
            [path-to-testapp] [path-to-clientapp]

Without --gapi both backends run (testapp must be built with SOFT=1 for soft); without --stage
both stages.

Prints "N checks, M failures"; exit status is the result.
"""
import importlib.util, os, secrets, sys, time

_here = os.path.dirname(os.path.abspath(__file__))


def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(_here, file))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


rc = _load("remote_check", "remote-check.py")
vw = _load("virtual_window_check", "virtual-window-check.py")
rr = _load("remote_render_check", "remote-render-check.py")

check = rc.check

# The layouts the load stage cycles through: text, tables, vectors - many distinct sets each.
LOAD_LAYOUTS = ["markdown", "text-view", "table", "shaping", "form", "vector", "dock-layout",
        "flex"]
LOAD_ROUNDS = 2
# Small enough that the layouts above do not fit together, large enough for any one of them.
LOAD_BUDGET = 128 * 1024
# The layout the load stage ends on and compares: still, once settled.
FINAL_LAYOUT = "text-view"


def session_of(s):
    sessions = (s.invoke("remote") or {}).get("sessions") or [{}]
    return sessions[0]


def frame_data(s):
    return session_of(s).get("frameData") or {}


def wait_gone(s, timeout=20.0):
    """Until the server has no session left: a killed client is noticed a moment later."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = s.invoke("remote") or {}
        if not status.get("clients"):
            return status
        vw.pump(s, 0.2)
    return s.invoke("remote") or {}


# Every server's log, for the validation check at the end
LOGS = []


def use_log(tag):
    """A log of its own for each server: start_server truncates the one it writes."""
    rc.SERVER_LOG = f"/tmp/xl-remote-cache-server-{tag}.log"
    LOGS.append(rc.SERVER_LOG)


def log_mark():
    return len(open(rc.SERVER_LOG, errors="replace").read())


def log_since(mark):
    return open(rc.SERVER_LOG, errors="replace").read()[mark:]


def run_clientapp(server_bin, client_bin, gapi, budget, reset):
    """One server and clientapp: the counters of a still scene, then the picture after typing
    "abcd" into the client's field - with a reset before the last key when `reset` is set."""
    pid = os.getpid()
    tag = f"{gapi}-{budget}-{pid}"
    sock = f"/tmp/xl-remote-cache-{tag}.sock"
    sock_a = f"/tmp/xl-remote-cache-a-{tag}.sock"
    share = f"shm:/dev/shm/xl-remote-cache-{tag}"
    token = secrets.token_hex(16)
    ret = {}
    server = client = None
    s = ca = None
    use_log(tag)
    try:
        server = rc.start_server(server_bin, sock, share, token, gapi=gapi,
                extra_env={"XL_REMOTE_FRAME_DATA_BUDGET": str(budget)})
        s = rc.Session(sock)
        status = rc.wait_for(s, lambda st: st.get("listening"))
        client = rc.spawn_client(client_bin, share, token, status.get("spki") or "",
                inspector=sock_a, extra_env={"XL_CLIENT_CREATE_WINDOW": "own:400x300"})
        status = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= 1
                and all(w.get("owner") for w in x.get("windows", [])), timeout=40.0)
        wins = status.get("windows", [])
        name = wins[0].get("name") if wins else None
        ca = vw.open_inspector(sock_a)
        if not name or ca is None:
            check(f"clientapp gets its window (budget {budget})", False, str(status))
            return ret

        # The square pulses: every frame moves a transform and changes no data.
        vw.pump(s, 2.0)
        ret["first"] = session_of(s)
        vw.pump(s, 1.5)
        ret["second"] = session_of(s)
        ret["client"] = (ca.invoke("client-stats") or {}).get("frameData") or {}

        ca.invoke("client-animation", op="stop")
        vw.settle(s, name)

        # The first characters bring every slot of the ring up to the still picture (they were left
        # with frames of the pulse, and a character is two frames); the next must be partial.
        for ch in "ab":
            vw.type_keys(s, name, ch)
            vw.settle(s, name)
        mark = log_mark()
        vw.type_keys(s, name, "c")
        vw.settle(s, name)
        ret["typed"] = log_since(mark)

        if reset:
            before = session_of(s)
            r = s.invoke("remote-frame-data-reset", session=before.get("id")) or {}
            ret["resetEpoch"] = r.get("epoch")
            vw.settle(s, name)
            vw.type_keys(s, name, "d")
            vw.settle(s, name)
            ret["beforeReset"] = before
            ret["afterReset"] = session_of(s)
            ret["clientAfterReset"] = (ca.invoke("client-stats") or {}).get("frameData") or {}
        else:
            vw.type_keys(s, name, "d")
            vw.settle(s, name)
        ret["text"] = (ca.invoke("client-text") or {}).get("text")
        ret["shot"] = rr.decode(s, window=name)

        rc.kill(client)
        client = None
        ret["gone"] = wait_gone(s)
    finally:
        for sess in (ca, s):
            if sess:
                sess.close()
        for proc in (client, server):
            rc.kill(proc)
        for p in (sock, sock_a):
            try:
                os.unlink(p)
            except OSError:
                pass
    ret["log"] = log_since(0)
    return ret


def stage_static(server_bin, client_bin, gapi):
    cached = run_clientapp(server_bin, client_bin, gapi, 32 * 1024 * 1024, True)
    cached_log = cached.get("log", "")
    plain = run_clientapp(server_bin, client_bin, gapi, 0, False)
    if not cached.get("second") or not plain.get("second"):
        check("both clientapp runs came up", False, f"{bool(cached)} {bool(plain)}")
        return

    first = cached["first"].get("frameData") or {}
    second = cached["second"].get("frameData") or {}
    check("a still scene's sets are stored once",
            first.get("stores", 0) > 0 and second.get("stores") == first.get("stores")
            and second.get("storedBytes") == first.get("storedBytes"),
            f"{first} -> {second}")
    check("and referenced by every frame after",
            second.get("hits", 0) > first.get("hits", 0) and second.get("misses") == 0,
            f"{first} -> {second}")
    mirror = cached.get("client") or {}
    check("the client's mirror agrees with the server's cache",
            mirror.get("entries") == second.get("entries") and mirror.get("bytes") == second.get(
                    "bytes") and mirror.get("inlined") == 0, f"client {mirror}, server {second}")

    # What a frame no longer carries is the data: the same frame without a cache is heavier by the
    # sets' bodies (the rest is the header, the states and the per-command transforms).
    with_cache = cached["second"].get("lastInputBytes", 0)
    without = plain["second"].get("lastInputBytes", 0)
    stored = second.get("storedBytes", 0)
    check("a frame of the still scene no longer carries its data",
            with_cache > 0 and without - with_cache >= 0.9 * stored,
            f"{with_cache} bytes with the cache, {without} without, {stored} bytes of data stored")
    print(f"       frame input: {with_cache} bytes with the cache, {without} without")
    check("the server that keeps nothing stores nothing",
            (plain["second"].get("frameData") or {}).get("entries", 0) == 0,
            str(plain["second"].get("frameData")))

    word = "damage: partial redraw" if gapi == "vulkan" else "damage: repainting"
    typed = cached.get("typed", "")
    check("a typed character is still a partial frame",
            typed.count(word) > 0 and "damage: full" not in typed, f"see {rc.SERVER_LOG}")

    before = cached.get("beforeReset") or {}
    after = cached.get("afterReset") or {}
    bfd, afd = before.get("frameData") or {}, after.get("frameData") or {}
    cfd = cached.get("clientAfterReset") or {}
    check("a reset starts a new epoch on both sides",
            cached.get("resetEpoch") == 1 and afd.get("epoch") == 1 and cfd.get("epoch") == 1
            and cfd.get("resets") == 1, f"server {afd}, client {cfd}")
    check("and the client stores its sets again",
            afd.get("resets") == 1 and afd.get("misses") == 0
            and afd.get("stores", 0) > bfd.get("stores", 0) and afd.get("entries", 0) > 0,
            f"{bfd} -> {afd}")
    check("without losing a frame to a miss", after.get("lateFrames", 0) == 0
            and "is not in the session's cache" not in cached_log, f"{after}")
    # (the resources a testapp started from its build directory misses are not this check's)
    errors = [l for l in cached_log.splitlines() if "[E]" in l
            and ("RemoteRenderClient" in l or "FrameContextHandle2d" in l or "frame data" in l)]
    check("the reset is the only error of the remote session in the log",
            len(errors) == 1 and "frame data cache" in errors[0], "\n".join(errors[:5]))

    check("the text typed around the reset arrived", cached.get("text") == "abcd"
            and plain.get("text") == "abcd", f"{cached.get('text')} / {plain.get('text')}")
    bad = rr.mismatch(cached["shot"], plain["shot"])
    check("the picture is the same with and without the cache", bad == 0,
            f"{bad} pixels differ by more than {rr.TOLERANCE}")

    for label, run in (("cache", cached), ("no cache", plain)):
        gone = run.get("gone") or {}
        check(f"no frame data is left alive after the client ({label})",
                not gone.get("clients") and gone.get("frameDataLive") == 0, str(gone))


def run_testapp(binary, gapi, budget):
    """testapp as the client, through the layouts; the counters after every switch, the picture of
    the last layout."""
    pid = os.getpid()
    tag = f"{gapi}-{budget}-{pid}"
    sock = f"/tmp/xl-remote-cache-load-{tag}.sock"
    csock = f"/tmp/xl-remote-cache-load-c-{tag}.sock"
    share = f"shm:/dev/shm/xl-remote-cache-load-{tag}"
    token = secrets.token_hex(16)
    ret = {"steps": []}
    server = client = None
    s = c = None
    use_log(f"load-{tag}")
    try:
        server = rc.start_server(binary, sock, share, token, gapi=gapi, keep_running=True,
                extra_env={"XL_REMOTE_FRAME_DATA_BUDGET": str(budget)})
        s = rc.Session(sock)
        rc.wait_for(s, lambda st: st.get("listening"))
        client = rr.spawn_testapp_client(binary, share, token, csock,
                {"XL_HIDE_FPS": "1", "XL_FLAT_QUEUE": "1"})
        name = None
        deadline = time.monotonic() + 40.0
        while name is None and time.monotonic() < deadline:
            s.ok("frame", count=1)
            name = rr.client_window(s)
            time.sleep(0.1)
        c = rr.open_inspector(csock)
        if name is None or c is None:
            check(f"testapp gets its window as a client (budget {budget})", False,
                    f"window={name} inspector={c is not None}")
            return ret
        vw.pump(s, 1.0)

        for _ in range(LOAD_ROUNDS):
            for layout in LOAD_LAYOUTS:
                c.ok("invoke", name="layout", args={"name": layout, "settle": 0})
                vw.pump(s, 0.8)
                ret["steps"].append((layout, frame_data(s)))
        c.ok("invoke", name="layout", args={"name": FINAL_LAYOUT, "settle": 0})
        vw.settle(s, name, quiet=1.5)
        ret["session"] = session_of(s)
        ret["shot"] = rr.decode(s, window=name)

        rc.kill(client)
        client = None
        ret["gone"] = wait_gone(s)
    finally:
        for sess in (c, s):
            if sess:
                sess.close()
        for proc in (client, server):
            rc.kill(proc)
        for p in (sock, csock):
            try:
                os.unlink(p)
            except OSError:
                pass
    return ret


def stage_load(binary, gapi):
    cached = run_testapp(binary, gapi, LOAD_BUDGET)
    plain = run_testapp(binary, gapi, 0)
    if not cached.get("session") or not plain.get("session"):
        check("both testapp runs came up", False, f"{bool(cached.get('session'))} "
                f"{bool(plain.get('session'))}")
        return

    fd = cached["session"].get("frameData") or {}
    over = [(l, st.get("bytes")) for l, st in cached["steps"] if st.get("bytes", 0) > LOAD_BUDGET]
    check("the cache never holds more than the budget", not over and fd.get("bytes", 0)
            <= LOAD_BUDGET, f"{over} / {fd}")
    check("switching layouts through a small budget evicts", fd.get("drops", 0) > 0, str(fd))
    check("and never misses or resets", fd.get("misses") == 0 and fd.get("resets") == 0
            and fd.get("declined") == 0, str(fd))
    check("the client kept referencing what the server holds", fd.get("hits", 0) > 0, str(fd))
    print(f"       load: {fd.get('stores')} stores, {fd.get('drops')} drops, "
            f"{fd.get('hits')} hits, {fd.get('bytes')} of {LOAD_BUDGET} bytes held")
    check("no frame was late under eviction", cached["session"].get("lateFrames", 0) == 0,
            str(cached["session"]))

    bad = rr.mismatch(cached["shot"], plain["shot"])
    check("after the eviction the last layout looks the same as without the cache", bad == 0,
            f"{bad} pixels differ by more than {rr.TOLERANCE}")
    for label, run in (("cache", cached), ("no cache", plain)):
        gone = run.get("gone") or {}
        check(f"no frame data is left alive after the testapp client ({label})",
                not gone.get("clients") and gone.get("frameDataLive") == 0, str(gone))


def main():
    gapis = ["vulkan", "soft"]
    stages = ["static", "load"]
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
        elif opt == "--stage":
            stages = [value]
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

    # Client windows are virtual and draw through the light queue with partial redraw, as in a
    # window manager; the frame counter is hidden, so that a scene can be still.
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_VIRTUAL_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"
    os.environ["XL_FLAT_QUEUE"] = "1"
    os.environ["XL_HIDE_FPS"] = "1"
    os.environ["XL_VK_DAMAGE_LOG"] = "1"
    os.environ["XL_SOFT_DAMAGE_LOG"] = "1"

    for gapi in gapis:
        print(f"--- gapi: {gapi}")
        first = len(LOGS)
        if "static" in stages:
            stage_static(server_bin, client_bin, gapi)
        if "load" in stages:
            stage_load(server_bin, gapi)
        if gapi == "vulkan":
            bad = [p for p in LOGS[first:]
                    if "Validation Error" in open(p, errors="replace").read()]
            check("no Vulkan validation error on the server", not bad, " ".join(bad))

    print(f"{rc.checks} checks, {rc.failures} failures")
    print(f"logs: {' '.join(LOGS)} {rc.CLIENT_LOG}")
    sys.exit(1 if rc.failures else 0)


if __name__ == "__main__":
    main()
