#!/usr/bin/env python3
"""
Xenolith scene-graph inspector — MCP server (debug only).

Exposes one tool, `inspect_scene`, that connects to the running Xenolith app's
debug inspector socket (a UNIX socket served by XLSceneInspector in DEBUG builds)
and returns the live node tree as text.

Dependency-free: implements the minimal JSON-RPC 2.0 / MCP stdio subset by hand
(initialize / notifications/initialized / tools/list / tools/call).

The socket path defaults to /tmp/xenolith-inspector.sock and can be overridden with
the XENOLITH_INSPECTOR_SOCK environment variable.
"""

import json
import os
import socket
import sys

SOCK = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xenolith-inspector.sock")
PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "xenolith-scene-inspector", "version": "0.1.0"}

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
    }
]


def read_snapshot(timeout: float = 3.0) -> str:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(SOCK)
    chunks = []
    while True:
        try:
            c = s.recv(8192)
        except socket.timeout:
            break
        if not c:
            break
        chunks.append(c)
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
            if name != "inspect_scene":
                error(req_id, -32601, f"unknown tool: {name}")
                continue
            timeout = float(params.get("arguments", {}).get("timeout", 3.0))
            try:
                text = read_snapshot(timeout)
            except (OSError, socket.error) as e:
                error(
                    req_id,
                    -32603,
                    f"cannot reach inspector at {SOCK}: {e}. "
                    "Is the app running in DEBUG mode?",
                )
                continue
            if not text:
                error(req_id, -32603, f"inspector at {SOCK} returned no data")
                continue
            result(req_id, text)
        else:
            if req_id is not None:
                error(req_id, -32601, f"method not found: {method}")


if __name__ == "__main__":
    main()
