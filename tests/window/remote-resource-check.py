#!/usr/bin/env python3
"""Sampled image uploads, material bindings and lifetime across two remote sessions."""
import importlib.util
import os
from pathlib import Path
import secrets
import sys
import time

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("remote_render", HERE / "remote-render-check.py")
rr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rr)
rc = rr.rc
COLORS = {(230, 20, 40), (20, 210, 60), (30, 70, 230), (240, 190, 20)}


def patch(image):
    points = [(x, y) for y in range(image.height) for x in range(image.width)
              if image.getpixel((x, y)) in COLORS]
    if len(points) != 128 * 128:
        return None
    left, top = min(x for x, _ in points), min(y for _, y in points)
    return image.crop((left, top, left + 128, top + 128)).tobytes()


def run(binary, gapi):
    prefix = f"/tmp/xl-resource-{os.getpid()}-{gapi}"
    share, token = "shm:" + prefix + ".shm", secrets.token_hex(16)
    procs, sessions = [], []
    env = {"XL_RESOURCE_UPLOAD_TEST": "1", "XL_HIDE_FPS": "1", "XL_FLAT_QUEUE": "1"}
    os.environ["XL_REMOTE_CLIENT_WINDOWS"] = "1"
    os.environ["XL_REMOTE_VIRTUAL_WINDOWS"] = "1"
    os.environ["XL_REMOTE_SHARE_PRIMARY"] = "0"
    os.environ["XL_REMOTE_MAX_CLIENTS"] = "2"
    try:
        server = rc.start_server(binary, prefix + ".sock", share, token, gapi=gapi,
                                 keep_running=True, extra_env={"XL_FLAT_QUEUE": "1"})
        procs.append(server)
        s = rc.Session(prefix + ".sock", timeout=120)
        sessions.append(s)
        local, l = rr.start_local(binary, prefix + "-local.sock", gapi, env)
        procs.append(local)
        if not l:
            rc.check(f"{gapi}: local image scene starts", False)
            return
        sessions.append(l)
        pictures = []
        for variant in range(2):
            before = {w["id"] for w in (s.ok("windows") or {}).get("windows", [])}
            csock = prefix + f"-client{variant}.sock"
            current_env = dict(env)
            if variant:
                current_env["XL_UPLOAD_VARIANT"] = "1"
            client = rr.spawn_testapp_client(binary, share, token, csock, current_env)
            procs.append(client)
            c = rr.open_inspector(csock, timeout=120)
            rc.check(f"{gapi}: client {variant} starts", c is not None)
            if not c:
                return
            sessions.append(c)
            name, pixels = None, None
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                s.ok("frame", count=1)
                for window in (s.ok("windows") or {}).get("windows", []):
                    if window.get("virtual") and window["id"] not in before:
                        name = window["id"]
                if name and (c.invoke("resource-upload.state") or {}).get("loaded"):
                    pixels = patch(rr.decode(s, window=name))
                    if pixels:
                        break
                time.sleep(0.05)
            rc.check(f"{gapi}: client {variant} draws all uploaded pixels", pixels is not None)
            rc.check(f"{gapi}: a pending or finalized atlas has no image index",
                     (c.invoke("resource-upload.state") or {}).get("dynamicLifetime"))
            rc.check(f"{gapi}: a blank remote frame has an empty command list",
                     (c.invoke("resource-upload.state") or {}).get("emptyFrame"))
            if not pixels:
                return
            pictures.append((name, pixels))
        deadline, baseline = time.monotonic() + 120, None
        while time.monotonic() < deadline:
            l.ok("frame", count=1)
            if (l.invoke("resource-upload.state") or {}).get("loaded"):
                baseline = patch(rr.decode(l))
                if baseline:
                    break
        rc.check(f"{gapi}: uploaded pixels match local compilation", pictures[0][1] == baseline)
        rc.check(f"{gapi}: same resource name in two sessions keeps distinct pixels",
                 pictures[0][1] != pictures[1][1])
        rc.check(f"{gapi}: second upload preserves the first client's image",
                 patch(rr.decode(s, window=pictures[0][0])) == pictures[0][1])
        sessions[2].ok("quit")
        procs[2].wait(timeout=120)
        rc.check(f"{gapi}: client teardown exits cleanly", procs[2].returncode == 0)
        rc.check(f"{gapi}: another client keeps its image after owner departure",
                 patch(rr.decode(s, window=pictures[1][0])) == pictures[1][1])
    finally:
        for session in sessions:
            session.close()
        for proc in reversed(procs):
            rc.kill(proc)
        for path in Path("/tmp").glob(Path(prefix).name + "*.sock"):
            path.unlink(missing_ok=True)


if __name__ == "__main__":
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else
                     HERE / "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp").resolve())
    for backend in ("soft", "vulkan"):
        run(binary, backend)
    print(f"{rc.checks} checks, {rc.failures} failures")
    raise SystemExit(bool(rc.failures))
