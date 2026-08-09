#!/usr/bin/env python3
"""
Xenolith scene-graph inspector — MCP server.

Talks to the running Xenolith app's inspector listener (served by XLSceneInspector
through the dispatch::Looper socket API) over two protocols that share one socket:

  * legacy one-shot text — send "scene\n" or "logs\n", read the text reply until
    EOF. Used by `inspect_scene` and `get_logs`.

  * framed session — send "xenolith/1 json\n", read the "# xenolith/1 ok json"
    greeting, then exchange length-prefixed frames:
        [u32 little-endian payload size][JSON payload]
        request  { "serial": u32, "cmd": "...", ...arguments }
        response { "serial": u32, "status": "ok"|"error", "error": "...", "result": ... }
    Used by everything else (screenshots, scene commands, input injection, frame
    stepping, window control, shutdown). Binary payloads (a PNG) arrive as
    "BASE64:<base64url, unpadded>" because that is how data::Value encodes Bytes
    into JSON.

The listener is armed in DEBUG builds, whenever XENOLITH_INSPECTOR_ADDRESS is set,
and always in headless mode (`--headless`), where the socket is the only interface
the process has.

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

import base64
import json
import os
import socket
import struct
import sys

ADDRESS = os.environ.get(
    "XENOLITH_INSPECTOR_ADDRESS",
    # legacy alias from the two-socket protocol era
    os.environ.get("XENOLITH_INSPECTOR_SOCK", "unix:/tmp/xenolith-inspector.sock"),
)
PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "xenolith-scene-inspector", "version": "0.3.0"}

# A screenshot frame is the largest thing that crosses this socket
MAX_FRAME_SIZE = 64 * 1024 * 1024

TIMEOUT_PROP = {
    "type": "number",
    "description": "Per-connect timeout in seconds (default 3).",
}

TOOLS = [
    {
        "name": "inspect_scene",
        "description": (
            "Snapshot the live scene-graph of the running Xenolith app and return it as an "
            "indented text tree: one node per line with type, #name, .classes, V/is-visible, "
            "content size, position, z-order. Use this to debug a GUI without seeing the window."
        ),
        "inputSchema": {"type": "object", "properties": {"timeout": TIMEOUT_PROP}},
    },
    {
        "name": "get_logs",
        "description": (
            "Read the application log ring buffer of the running Xenolith app and return it as "
            "text: one log entry per line, formatted [LEVEL][tag] message. Use this to verify "
            "business logic works BEFORE building UI, or to diagnose crashes/misbehavior. Each "
            "call returns the whole current buffer (capped at the last ~4096 lines)."
        ),
        "inputSchema": {"type": "object", "properties": {"timeout": TIMEOUT_PROP}},
    },
    {
        "name": "screenshot",
        "description": (
            "Capture what the app is currently showing and write it to a PNG file. In headless "
            "mode this reads back the last presented pseudo-swapchain image; otherwise the engine "
            "renders one extra offscreen frame. Returns the path plus the image size. ALWAYS call "
            "step_frame first (headless renders only on demand, so an un-stepped screenshot "
            "returns the previous frame), and again after anything that changes the scene."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Where to write the PNG (default /tmp/xenolith-screenshot.png).",
                },
                "timeout": TIMEOUT_PROP,
            },
        },
    },
    {
        "name": "list_commands",
        "description": (
            "List the commands the running scene registered with the inspector "
            "(SceneInspector::addCommand), with their descriptions. These are the actions this "
            "particular app exposes for external control; run one with invoke_command."
        ),
        "inputSchema": {"type": "object", "properties": {"timeout": TIMEOUT_PROP}},
    },
    {
        "name": "invoke_command",
        "description": (
            "Run one of the scene-registered commands reported by list_commands and return its "
            "result. Arguments are passed through verbatim as a JSON object."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Command name."},
                "args": {"type": "object", "description": "Command arguments."},
                "timeout": TIMEOUT_PROP,
            },
            "required": ["name"],
        },
    },
    {
        "name": "send_input",
        "description": (
            "Inject synthetic input events into the app, as if they came from the window system. "
            "Each event is an object: {event, id, button, modifiers, x, y} for pointer events "
            "(Begin/Move/End/Cancel/MouseMove/Scroll) or {event, keycode, keysym, keychar} for key "
            "events (KeyPressed/KeyRepeated/KeyReleased/KeyCanceled). Names match the engine's "
            "own (getInputEventName / getInputButtonName / getInputKeyCodeName); integers are "
            "accepted too, and keychar may be written as the character itself. A click is a Begin "
            "followed by an End at the same point. Set native=true to type into a focused text "
            "field — see that flag's description."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "events": {
                    "type": "array",
                    "items": {"type": "object"},
                    "description": "Events to inject, in order.",
                },
                "native": {
                    "type": "boolean",
                    "description": (
                        "Inject at the OS-window level, so the events pass through the platform's "
                        "text-input processor first: printable keys, Backspace, Delete and Escape "
                        "are consumed by the focused text field exactly as a real keystroke would "
                        "be. REQUIRED to type text. Default false, which delivers straight to the "
                        "scene and bypasses text input entirely."
                    ),
                },
                "timeout": TIMEOUT_PROP,
            },
            "required": ["events"],
        },
    },
    {
        "name": "send_text",
        "description": (
            "Drive the focused text field's input processor directly — the IME-level path that "
            "key events cannot express. op='marked' then op='unmark' reproduces composition (CJK, "
            "dead keys); op='insert' inserts text, optionally replacing the range "
            "[replaceStart, replaceLength); op='delete-backward'/'delete-forward' delete around "
            "the cursor; op='cancel' releases text input. op='state' reads the current state back "
            "(text, cursor, marked range, enabled, hasHandler). Mutating ops are applied "
            "asynchronously — call step_frame before reading state or taking a screenshot. Use "
            "send_input with native=true when you want the real keyboard path instead."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "op": {
                    "type": "string",
                    "enum": ["insert", "marked", "unmark", "delete-backward", "delete-forward",
                             "cancel", "state"],
                },
                "text": {"type": "string", "description": "Text for insert/marked."},
                "replaceStart": {"type": "number"},
                "replaceLength": {"type": "number"},
                "markedStart": {"type": "number"},
                "markedLength": {"type": "number"},
                "compose": {"type": "number", "description": "InputKeyComposeState, default 0."},
                "timeout": TIMEOUT_PROP,
            },
            "required": ["op"],
        },
    },
    {
        "name": "step_frame",
        "description": (
            "Ask the app to render N frames. Headless windows render on demand, so nothing is "
            "drawn (and a screenshot stays stale) until this is called — run it before every "
            "screenshot and after every change to the scene. Harmless on a windowed app, where "
            "it just requests a redraw."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "count": {"type": "number", "description": "Frames to render (default 1)."},
                "timeout": TIMEOUT_PROP,
            },
        },
    },
    {
        "name": "window_control",
        "description": (
            "Inspect or control the app window: op='constraints' reports the current extent, "
            "density and frame interval; op='resize' resizes it (only a headless pseudo-window "
            "can honour this — a window manager owns the size otherwise); op='close' closes it."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "op": {"type": "string", "enum": ["constraints", "resize", "close"]},
                "width": {"type": "number"},
                "height": {"type": "number"},
                "timeout": TIMEOUT_PROP,
            },
            "required": ["op"],
        },
    },
    {
        "name": "quit_app",
        "description": (
            "Shut the application down: closes the window, which tears down the context and ends "
            "the process. Use this to stop an app started for a headless session."
        ),
        "inputSchema": {"type": "object", "properties": {"timeout": TIMEOUT_PROP}},
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
    """Legacy protocol: send one command line, read the reply until EOF."""
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


class Session:
    """One framed-protocol connection. Short-lived: opened per tool call."""

    def __init__(self, timeout: float = 3.0):
        self.sock = connect(timeout)
        self.buf = b""
        self.serial = 0
        self.sock.sendall(b"xenolith/1 json\n")
        greeting = self._read_line()
        if not greeting.startswith("# xenolith/1 ok"):
            raise OSError(f"unexpected handshake reply: {greeting!r}")

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def _fill(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise OSError("inspector closed the connection")
        self.buf += chunk

    def _read_line(self) -> str:
        while b"\n" not in self.buf:
            self._fill()
        line, _, self.buf = self.buf.partition(b"\n")
        return line.decode("utf-8", "replace")

    def _read_frame(self) -> dict:
        while len(self.buf) < 4:
            self._fill()
        size = struct.unpack("<I", self.buf[:4])[0]
        if size > MAX_FRAME_SIZE:
            raise OSError(f"frame too large: {size}")
        while len(self.buf) < 4 + size:
            self._fill()
        payload = self.buf[4:4 + size]
        self.buf = self.buf[4 + size:]
        return json.loads(payload.decode("utf-8"))

    def call(self, cmd: str, **args) -> dict:
        self.serial += 1
        request = dict(args)
        request["serial"] = self.serial
        request["cmd"] = cmd
        payload = json.dumps(request).encode("utf-8")
        self.sock.sendall(struct.pack("<I", len(payload)) + payload)
        # replies are correlated by serial: a slow screenshot may be overtaken
        while True:
            response = self._read_frame()
            if response.get("serial") == self.serial:
                return response


def decode_bytes(value: str) -> bytes:
    """data::Value encodes Bytes into JSON as "BASE64:<base64url, unpadded>"."""
    if value.startswith("BASE64:"):
        value = value[len("BASE64:"):]
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


def run_framed(name: str, args: dict, timeout: float) -> str:
    """Execute one framed-protocol tool and render its result as text."""
    session = Session(timeout)
    try:
        if name == "screenshot":
            path = args.get("path") or "/tmp/xenolith-screenshot.png"
            response = session.call("screenshot")
            check(response)
            info = response["result"]
            data = decode_bytes(info["data"])
            with open(path, "wb") as f:
                f.write(data)
            return (f"wrote {len(data)} bytes to {path} "
                    f"({info['width']}x{info['height']} {info['format']})")

        if name == "list_commands":
            response = session.call("commands")
            check(response)
            commands = response["result"].get("commands") or []
            if not commands:
                return "(the running scene registered no commands)"
            return "\n".join(f"{c['name']}: {c['description']}" for c in commands)

        if name == "invoke_command":
            response = session.call("invoke", name=args["name"], args=args.get("args") or {})
            check(response)
            return json.dumps(response["result"], indent=2)

        if name == "send_input":
            response = session.call("input", events=args["events"],
                                    native=bool(args.get("native", False)))
            check(response)
            result = response["result"]
            how = "natively" if result.get("native") else "into the scene"
            return f"injected {result['accepted']} event(s) {how}"

        if name == "send_text":
            payload = {k: v for k, v in args.items() if k != "timeout"}
            response = session.call("text", **payload)
            check(response)
            return json.dumps(response["result"], indent=2)

        if name == "step_frame":
            count = int(args.get("count", 1))
            response = session.call("frame", count=count)
            check(response)
            return f"requested {response['result']['count']} frame(s)"

        if name == "window_control":
            op = args["op"]
            response = session.call("window", op=op,
                                    width=int(args.get("width", 0)),
                                    height=int(args.get("height", 0)))
            check(response)
            return json.dumps(response["result"], indent=2)

        if name == "quit_app":
            response = session.call("quit")
            check(response)
            return "shutdown requested"

        raise OSError(f"unknown framed tool: {name}")
    finally:
        session.close()


def check(response: dict) -> None:
    if response.get("status") != "ok":
        raise OSError(response.get("error") or "inspector reported an error")


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
            arguments = params.get("arguments") or {}
            timeout = float(arguments.get("timeout", 3.0))

            if name in ("inspect_scene", "get_logs"):
                command = "scene" if name == "inspect_scene" else "logs"
                try:
                    text = query(command, timeout)
                except OSError as e:
                    error(req_id, -32603,
                          f"cannot reach inspector at {ADDRESS}: {e}. Is the app running?")
                    continue
                if not text:
                    if name == "get_logs":
                        text = "(no logs captured yet)"
                    else:
                        error(req_id, -32603, f"inspector at {ADDRESS} returned no data")
                        continue
                result(req_id, text)
            elif any(t["name"] == name for t in TOOLS):
                try:
                    result(req_id, run_framed(name, arguments, timeout))
                except (OSError, KeyError, ValueError, json.JSONDecodeError) as e:
                    error(req_id, -32603, f"{name} failed against {ADDRESS}: {e}")
            else:
                error(req_id, -32601, f"unknown tool: {name}")
        else:
            if req_id is not None:
                error(req_id, -32601, f"method not found: {method}")


if __name__ == "__main__":
    main()
