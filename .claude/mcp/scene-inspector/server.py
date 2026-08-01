#!/usr/bin/env python3
"""
Xenolith scene-graph inspector — MCP server (debug only).

Exposes two tools, `inspect_scene` and `get_logs`, that connect to the running
Xenolith app's debug inspector listener (served by XLSceneInspector in DEBUG
builds through the dispatch::Looper socket API), send a one-line text command
("scene\n" or "logs\n") and return the text reply (read until EOF).

Dependency-free: implements the minimal JSON-RPC 2.0 / MCP stdio subset by hand
(initialize / notifications/initialized / tools/list / tools/call).

The listener address defaults to unix:/tmp/xenolith-inspector.sock and can be
overridden with the XENOLITH_INSPECTOR_ADDRESS environment variable, matching
the engine-side format:
    unix:/path/to.sock   - UNIX socket path
    unix:@name           - Linux abstract namespace (Android default; use
                           `adb forward tcp:4490 localabstract:xenolith-inspector`
                           and point this at 127.0.0.1:4490 instead)
    host:port            - numeric IPv4 (Windows default: 127.0.0.1:4490)
    :port                - IPv4 loopback
"""

import json
import os
import socket
import sys

ADDRESS = os.environ.get(
    "XENOLITH_INSPECTOR_ADDRESS",
    # legacy alias from the two-socket protocol era
    os.environ.get("XENOLITH_INSPECTOR_SOCK", "unix:/tmp/xenolith-inspector.sock"),
)
PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "xenolith-scene-inspector", "version": "0.2.0"}

TOOLS = [
    {
        "name": "inspect_scene",
        "description": (
            "Snapshot the live scene-graph of the running Xenolith app (DEBUG builds only) "
            "and return it as an indented text tree: one node per line with type, #name, "
            ".classes, V/is-visible, content size, position, z-order. Use this to debug a GUI "
            "without seeing the window. The app must be running in debug mode."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "timeout": {
                    "type": "number",
                    "description": "Per-connect timeout in seconds (default 3).",
                },
            },
        },
    },
    {
        "name": "get_logs",
        "description": (
            "Read the application log ring buffer of the running Xenolith app (DEBUG builds "
            "only) and return it as text: one log entry per line, formatted [LEVEL][tag] message. "
            "Use this to verify business logic (e.g. the installer controller: catalogue load, "
            "install progress, errors) works BEFORE building UI, or to diagnose crashes/misbehavior. "
            "Each call returns the whole current buffer (capped at the last ~4096 lines). The app "
            "must be running in debug mode."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "timeout": {
                    "type": "number",
                    "description": "Per-connect timeout in seconds (default 3).",
                },
            },
        },
    },
]


def connect(timeout: float) -> socket.socket:
    """Connect to ADDRESS using the engine-side address grammar."""
    if ADDRESS.startswith("unix:"):
        path = ADDRESS[len("unix:"):]
        if not path:
            raise OSError(f"empty unix path in address: {ADDRESS!r}")
        if path.startswith("@"):
            # Linux abstract namespace: leading NUL instead of '@'
            path = "\0" + path[1:]
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect(path)
        return s
    host, sep, port = ADDRESS.rpartition(":")
    if not sep or not port.isdigit():
        raise OSError(f"cannot parse address: {ADDRESS!r}")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((host or "127.0.0.1", int(port)))
    return s


def query(command: str, timeout: float = 3.0) -> str:
    """Send one command line, read the reply until EOF."""
    s = connect(timeout)
    try:
        s.sendall(command.encode("utf-8") + b"\n")
        chunks = []
        while True:
            try:
                c = s.recv(8192)
            except socket.timeout:
                break
            if not c:
                break
            chunks.append(c)
    finally:
        s.close()
    return b"".join(chunks).decode("utf-8", "replace")


def send(msg: dict) -> None:
    sys.stdout.write(json.dumps(msg))
    sys.stdout.write("\n")
    sys.stdout.flush()


def result(req_id, text: str) -> None:
    send({
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "content": [{"type": "text", "text": text}],
            "isError": False,
        },
    })


def error(req_id, code: int, message: str) -> None:
    send({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}})


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue

        method = req.get("method")
        req_id = req.get("id")
        params = req.get("params") or {}

        if method == "initialize":
            send({
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "protocolVersion": PROTOCOL_VERSION,
                    "capabilities": {"tools": {}},
                    "serverInfo": SERVER_INFO,
                },
            })
        elif method == "notifications/initialized":
            pass  # no response needed
        elif method == "tools/list":
            send({"jsonrpc": "2.0", "id": req_id, "result": {"tools": TOOLS}})
        elif method == "tools/call":
            name = params.get("name")
            timeout = float(params.get("arguments", {}).get("timeout", 3.0))
            if name in ("inspect_scene", "get_logs"):
                command = "scene" if name == "inspect_scene" else "logs"
                try:
                    text = query(command, timeout)
                except (OSError, socket.error) as e:
                    error(
                        req_id,
                        -32603,
                        f"cannot reach inspector at {ADDRESS}: {e}. "
                        "Is the app running in DEBUG mode?",
                    )
                    continue
                if not text:
                    if name == "get_logs":
                        text = "(no logs captured yet)"
                    else:
                        error(req_id, -32603, f"inspector at {ADDRESS} returned no data")
                        continue
                result(req_id, text)
            else:
                error(req_id, -32601, f"unknown tool: {name}")
        else:
            if req_id is not None:
                error(req_id, -32601, f"method not found: {method}")


if __name__ == "__main__":
    main()
