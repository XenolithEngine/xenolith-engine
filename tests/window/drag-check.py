#!/usr/bin/env python3
"""Run the four drag-and-drop stands headless and report what each of them concluded.

Unlike the other check scripts here, this one asserts nothing of its own: each stand runs its
phases from a Sequence of DelayTimes and does its own checking, ending with

    [W][DragBasicTest] SUMMARY: 51 checks, 0 failures

So what a driver owes them is time, not assertions - and in headless there is no time except the
frames it asks for. Hence the loop: step a couple of frames, read the log, stop at the SUMMARY.
Waiting a fixed number of frames instead would encode this machine's frame pacing into the test.

Each stand gets its own PROCESS, which is the point of running them here rather than through the
`layout` command. The DragSystem lives on the scene content, so two stands in one process share it,
and a switch that lands mid-sequence leaves a session in flight for the next one to trip over. The
stands cancel their own drag on the way out, so the sweep passes either way - but a fresh process
per stand is what makes a failure mean what it says.

    tests/window/drag-check.py [path-to-testapp]

Prints one line per stand and exits non-zero if any of them failed or never finished.
"""
import json, os, socket, struct, subprocess, sys, time

# name, the variable that selects it, and the log tag it signs its summary with
STANDS = [
    ("drag-basic", "XL_DRAG_BASIC_TEST", "DragBasicTest"),
    ("drag-actions", "XL_DRAG_ACTIONS_TEST", "DragActionsTest"),
    # RUN HEADLESS, which this script always does: the clipboard round trip is deterministic only
    # where the controller keeps the data in process
    ("drag-payload", "XL_DRAG_PAYLOAD_TEST", "DragPayloadTest"),
    ("drag-text", "XL_DRAG_TEXT_TEST", "DragTextTest"),
]

# The whole sequence is under two seconds of stand time; this is the budget for a debug build on a
# busy machine, not an expected duration
TIMEOUT = 30.0


class Session:
    def __init__(self, path, timeout=25.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
        # the greeting is a LINE, before any frame - read it to the newline or the first length
        # word is consumed as part of it
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

    def close(self):
        self.s.close()


def logs(path):
    """The one-shot text protocol: send a word, read until EOF."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(path)
    s.sendall(b"logs\n")
    out = b""
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        out += chunk
    s.close()
    return out.decode("utf-8", "replace")


def start_app(binary, env_name, addr):
    env = dict(os.environ)
    env[env_name] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + addr
    try:
        os.unlink(addr)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(addr):
            try:
                Session(addr).close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up for " + env_name)


def run(binary, name, env_name, tag):
    addr = "/tmp/xl-%s-check.sock" % name
    proc = start_app(binary, env_name, addr)
    s = Session(addr)
    summary = None
    try:
        deadline = time.time() + TIMEOUT
        while time.time() < deadline:
            s.call("frame", count=2)
            time.sleep(0.05)
            for line in logs(addr).splitlines():
                if "SUMMARY" in line and tag in line:
                    summary = line.strip()
            if summary:
                break
        errors = [l.strip() for l in logs(addr).splitlines()
                if l.startswith("[E]") or l.startswith("[F]")]
    finally:
        s.close()
        proc.kill()
        try:
            os.unlink(addr)
        except OSError:
            pass
    return summary, errors


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

failed = 0
for name, env_name, tag in STANDS:
    summary, errors = run(binary, name, env_name, tag)
    if summary is None:
        print("  FAIL %-13s never reached its summary" % name)
        failed += 1
        continue
    # the stand counts its own checks; " 0 failures" is the whole verdict
    ok = " 0 failures" in summary
    print("  %s %-13s %s" % ("ok  " if ok else "FAIL", name, summary.split("SUMMARY: ")[-1]))
    if not ok:
        failed += 1
        for line in errors[:20]:
            print("       " + line)

print("\n%d stands, %d failed" % (len(STANDS), failed))
sys.exit(1 if failed else 0)
