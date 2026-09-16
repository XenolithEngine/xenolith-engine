#!/usr/bin/env python3
"""What the software rasterizer costs when it serves N windows (milestone M1).

Two shapes of the same question, because the Embox window manager will meet both:

  * `--mode windows` -- N windows of ONE process, each with its own Director, scene and
    presentation engine, all rasterizing on the same context thread;
  * `--mode clients` -- N remote clients over `shm:`, each asking the server for a window of its
    own (S4). The scenes run in N separate processes; the server rasterizes all of them.

What is reported per window is frames per second, measured as the delta of the window's own
`presented` counter (the inspector's `frame` command with `count: 0`) over a fixed wall-clock
window -- and the server's CPU, in cores, sampled from /proc around the same window.

    tests/window/soft-bench.py [--mode windows|clients] [--windows 1,2,4] [--seconds 10]
                               [--width 800] [--height 600] [--gapi soft]

Two things the first runs turned up, so nobody re-derives them: N clients share the server fairly
and the bottleneck is the frame protocol rather than the rasterizer (a 25x larger window costs
almost nothing), while N windows of ONE process do not share at all -- they are served by a single
context thread and the first to free-run keeps it.

Read `docs/agents/measuring-frames.md` before believing any number this prints. In particular:
the FPS overlay is AlwaysDirty and costs more than half a frame on a slow target (it is off here),
`XL_SOFT_PROFILE`'s counters are process-wide statics and say nothing per window, and a debug build
measures a debug build.
"""
import argparse, importlib.util, os, secrets, sys, time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("remote_check",
        os.path.join(_here, "remote-check.py"))
rc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(rc)


# --- CPU sampling -------------------------------------------------------------------------------
#
# /proc/<pid>/stat fields 14/15 are utime/stime in clock ticks. The difference across the measured
# window, divided by its wall-clock length, is the number of cores the process kept busy.
CLOCK_TICKS = os.sysconf("SC_CLK_TCK")


def cpu_ticks(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            fields = f.read().rsplit(") ", 1)[1].split()
        return int(fields[11]) + int(fields[12])
    except (OSError, IndexError, ValueError):
        return None


def cores_used(before, after, seconds):
    if before is None or after is None or seconds <= 0:
        return None
    return (after - before) / CLOCK_TICKS / seconds


def thread_cores(pid, before, after, seconds):
    """Per-thread cores, keyed by the thread's name with the `<tid>:` prefix stripped."""
    out = {}
    for tid, ticks_after in (after or {}).items():
        ticks_before = (before or {}).get(tid)
        if ticks_before is None:
            continue
        name = ticks_after[1]
        value = cores_used(ticks_before[0], ticks_after[0], seconds)
        if value:
            out[name] = out.get(name, 0.0) + value
    return out


def thread_ticks(pid):
    """{tid: (ticks, name)}. Thread names are `<tid>:<name>` and `comm` truncates at 15 chars."""
    out = {}
    try:
        tids = os.listdir(f"/proc/{pid}/task")
    except OSError:
        return out
    for tid in tids:
        try:
            with open(f"/proc/{pid}/task/{tid}/stat") as f:
                fields = f.read().rsplit(") ", 1)[1].split()
            ticks = int(fields[11]) + int(fields[12])
            with open(f"/proc/{pid}/task/{tid}/comm") as f:
                name = f.read().strip()
        except (OSError, IndexError, ValueError):
            continue
        # `1234567:Main` -> `Main`; `1234567:Main:Wo` is a truncated pool worker.
        if ":" in name:
            name = name.split(":", 1)[1]
        out[tid] = (ticks, name)
    return out


# --- the two stands -----------------------------------------------------------------------------


def presented(session, window=None):
    r = (session.ok("frame", count=0, window=window) if window
            else session.ok("frame", count=0)) or {}
    return r.get("presented", 0)


def measure(session, windows, seconds, pids):
    """Frames per second per window, and cores per process, over one wall-clock window."""
    before = {w: presented(session, w) for w in windows}
    cpu_before = {name: cpu_ticks(pid) for name, pid in pids.items()}
    threads_before = thread_ticks(pids.get("server"))

    start = time.monotonic()
    time.sleep(seconds)
    elapsed = time.monotonic() - start

    after = {w: presented(session, w) for w in windows}
    cpu_after = {name: cpu_ticks(pid) for name, pid in pids.items()}
    threads_after = thread_ticks(pids.get("server"))

    fps = {w: (after[w] - before[w]) / elapsed for w in windows}
    cores = {name: cores_used(cpu_before[name], cpu_after[name], elapsed) for name in pids}
    return fps, cores, thread_cores(pids.get("server"), threads_before, threads_after, elapsed)


def run_windows(args, count):
    """N windows of one process. Each is told to render continuously, then left alone."""
    pid = os.getpid()
    sock = f"/tmp/xl-soft-bench-{pid}.sock"
    env_backup = dict(os.environ)
    os.environ["XL_HIDE_FPS"] = "1"  # AlwaysDirty, and its text changes every frame
    if args.full_redraw:
        os.environ["XL_SOFT_FORCE_FULL_REDRAW"] = "1"
    server = None
    try:
        server = rc.start_server(args.binary, sock, "", "", gapi=args.gapi, keep_running=True)
        s = rc.Session(sock)
        s.ok("frame", count=3)

        windows = [None]  # the session's own window answers without a `window` argument
        if count > 1:
            r = s.invoke("open-windows", count=count - 1, width=args.width, height=args.height)
            if not r or not r.get("ok"):
                raise SystemExit(f"open-windows failed: {r}")
            deadline = time.monotonic() + 30.0
            while time.monotonic() < deadline:
                names = [w["id"] for w in (s.ok("windows") or {}).get("windows", [])]
                if len(names) >= count:
                    windows = [None] + [n for n in names if n.startswith("bench-")]
                    break
                s.ok("frame", count=1)
                time.sleep(0.2)
            if len(windows) < count:
                raise SystemExit(f"only {len(windows)} of {count} windows appeared")

        # Continuous rendering: a scene with an active action asks for the next frame at the end of
        # every visit, so the run is the engine's own rate rather than this driver's poll rate.
        # Continuous rendering has to be kicked once per window: the action asks for the NEXT frame
        # at the end of a visit, so a window that is not drawing has nothing to ask from.
        #
        # And a target interval, because without one the windows do not share: they are all served
        # by one context thread, and the first to free-run keeps it -- the others get a frame every
        # few seconds. With a target, each window asks at its own rate and what is measured is
        # whether the rasterizer can keep up with N of them.
        for w in windows:
            if args.interval_us:
                s.ok("window", op="frame-interval", value=args.interval_us, window=w) if w \
                        else s.ok("window", op="frame-interval", value=args.interval_us)
            if w:
                s.ok("render", seconds=args.seconds + 20, window=w)
                s.ok("frame", count=1, window=w)
            else:
                s.ok("render", seconds=args.seconds + 20)
                s.ok("frame", count=1)
        time.sleep(1.0)  # settle before measuring

        return measure(s, windows, args.seconds, {"server": server.pid})
    finally:
        rc.kill(server)
        os.environ.clear()
        os.environ.update(env_backup)
        try:
            os.unlink(sock)
        except OSError:
            pass


def run_clients(args, count):
    """N remote clients, each asking for its own window; the server rasterizes all of them."""
    pid = os.getpid()
    sock = f"/tmp/xl-soft-bench-{pid}.sock"
    share = f"shm:/dev/shm/xl-soft-bench-{pid}"
    token = secrets.token_hex(16)
    env_backup = dict(os.environ)
    os.environ["XL_HIDE_FPS"] = "1"
    if args.full_redraw:
        os.environ["XL_SOFT_FORCE_FULL_REDRAW"] = "1"
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"  # the window-manager shape: share nothing of ours
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = str(count)

    server = None
    clients = []
    try:
        server = rc.start_server(args.binary, sock, share, token, gapi=args.gapi,
                keep_running=True)
        s = rc.Session(sock)
        s.ok("frame", count=3)
        st = rc.wait_for(s, lambda x: x.get("listening"))
        spki = st.get("spki") or ""

        for i in range(count):
            clients.append(rc.spawn_client(args.client, share, token, spki, extra_env={
                "XL_CLIENT_CREATE_WINDOW": f"bench{i}:{args.width}x{args.height}",
            }))
        st = rc.wait_for(s, lambda x: x.get("sharedWindows", 0) >= count, timeout=60.0)
        if st.get("sharedWindows", 0) < count:
            raise SystemExit(f"only {st.get('sharedWindows')} of {count} client windows appeared")

        windows = [w["name"] for w in (st.get("windows") or [])]
        # The client scene runs a repeating action, so each window free-runs once it has started.
        for _ in range(20):
            s.ok("frame", count=1)
            time.sleep(0.1)

        pids = {"server": server.pid}
        for i, c in enumerate(clients):
            pids[f"client{i}"] = c.pid
        return measure(s, windows, args.seconds, pids)
    finally:
        for c in clients:
            rc.kill(c)
        rc.kill(server)
        os.environ.clear()
        os.environ.update(env_backup)
        try:
            os.unlink(sock)
        except OSError:
            pass


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mode", choices=("windows", "clients"), default="windows")
    p.add_argument("--windows", default="1,2,4", help="comma-separated window counts")
    p.add_argument("--seconds", type=float, default=10.0)
    p.add_argument("--width", type=int, default=800)
    p.add_argument("--height", type=int, default=600)
    p.add_argument("--gapi", default="soft")
    p.add_argument("--release", action="store_true", help="measure the release build")
    p.add_argument("--interval-us", dest="interval_us", type=int, default=16666,
            help="target frame interval per window in us (0: as fast as the engine will go, which "
                 "on one context thread means the first window keeps it)")
    p.add_argument("--damage", dest="full_redraw", action="store_false",
            help="keep damage tracking on; by default every frame is redrawn in full, so what is "
                 "measured is a frame rather than how little of it changed")
    p.set_defaults(full_redraw=True)
    p.add_argument("--binary", default=None)
    p.add_argument("--client", default=None)
    args = p.parse_args()

    triple = "x86_64-unknown-linux-gnu"
    build = "release" if args.release else "debug"
    args.binary = args.binary or os.path.join(_here, "stappler-build", triple, build, "cc",
            "testapp")
    args.client = args.client or os.path.join(_here, "client", "stappler-build", triple, build,
            "cc", "clientapp")
    for path in (args.binary,) + ((args.client,) if args.mode == "clients" else ()):
        if not os.path.exists(path):
            raise SystemExit(f"missing binary: {path}")

    counts = [int(x) for x in args.windows.split(",") if x.strip()]
    print(f"mode: {args.mode}   gapi: {args.gapi}   build: {build}   "
            f"size: {args.width}x{args.height}   window: {args.seconds:.0f}s   "
            f"full redraw: {'on' if args.full_redraw else 'off'}   cores: {os.cpu_count()}")
    print(f"{'N':>2} {'fps/window':>28} {'total fps':>10} {'server cores':>13} {'client cores':>13}")

    for count in counts:
        fps, cores, threads = (run_windows(args, count) if args.mode == "windows"
                else run_clients(args, count))
        per = "  ".join(f"{v:.1f}" for v in fps.values())
        total = sum(fps.values())
        server_cores = cores.get("server")
        client_cores = sum(v for k, v in cores.items() if k.startswith("client") and v) or None
        print(f"{count:>2} {per:>28} {total:>10.1f} "
                f"{(f'{server_cores:.2f}' if server_cores is not None else '-'):>13} "
                f"{(f'{client_cores:.2f}' if client_cores is not None else '-'):>13}")
        if threads:
            top = sorted(threads.items(), key=lambda kv: -kv[1])[:4]
            print("     server threads: "
                    + ", ".join(f"{name} {value:.2f}" for name, value in top))
    return 0


if __name__ == "__main__":
    sys.exit(main())
