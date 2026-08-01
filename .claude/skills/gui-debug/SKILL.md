---
name: gui-debug
description: >-
  Debug a running Xenolith GUI app by inspecting its live scene graph and reading
  its application logs, via two MCP tools: `inspect_scene` (the node tree) and
  `get_logs` (the log ring buffer). Use when a Xenolith GUI app (installer,
  scaffolded app, tests/window) is misbehaving visually — missing elements, wrong
  layout, invisible nodes, overlap, wrong stacking, an element you can't find,
  "why is this hidden", "where did this node go"; OR when verifying business
  logic (catalogue load, install progress, errors, crashes) works BEFORE or
  independently of the UI. Complements source-level debugging (lldb/wine-debug).
---

# Debugging a Xenolith GUI: scene inspector + logs (MCP)

A DEBUG build of any Xenolith GUI app exposes one process-wide debug listener
(served through the platform-independent `dispatch::Looper` socket API on the app
thread), consumed by two MCP tools:

1. **`inspect_scene`** — the live scene graph as an indented text tree (one node
   per line).
2. **`get_logs`** — the application log ring buffer (last ~4096 entries, formatted
   `[LEVEL][tag] message`).

**Transport:** one listener address; the client sends a one-line text command
(`scene\n` or `logs\n`) and reads the reply until EOF. The address comes from the
`XENOLITH_INSPECTOR_ADDRESS` environment variable (both the app and `server.py`
honor it), format `unix:/path`, `unix:@abstract`, `host:port` or `:port`.
Per-platform defaults:

| Platform | Default |
|---|---|
| Linux / macOS | `unix:/tmp/xenolith-inspector.sock` |
| Android | `unix:@xenolith-inspector` (abstract; from the host: `adb forward tcp:4490 localabstract:xenolith-inspector` and point the client at `127.0.0.1:4490`) |
| Windows | `127.0.0.1:4490` (TCP loopback — Python has no practical AF_UNIX there) |
| wasm | not available (no sockets in the browser sandbox; the inspector stays off) |

Wired in `xenolith/application/nodes/XLSceneInspector.cc` (scene snapshot
refreshed ~5×/s; a `stappler::log::CustomLog` sink mirrors every log call into the
ring buffer). Release builds ship neither — zero overhead, no listener.

## When to use which

- **`inspect_scene`** — visual/structural problems: invisible button, wrong layout,
  overlap, z-order, "did my node appear", "why is everything gone" (ancestor
  opacity 0 / `background-color: transparent`). Read content-size/position/z-order.
- **`get_logs`** — behavioral problems: did the catalogue load? did install start?
  what error did the worker throw? did the controller init? Use it to verify
  business logic **before** building UI (call the controller methods from a scene
  hook, then `get_logs`), and to diagnose crashes/misbehavior without a debugger.

## Prerequisites

1. **A DEBUG build** of the app (release ships no sockets). Build via the
   `cli-build` skill, e.g. `xenolith-cli build utils/installer --engine <engine>`.
2. **The app must be running.** Launch it: macOS `open <name>.app`, or pass
   `--run` to `xenolith-cli build`.

## Inspecting the scene

Call `inspect_scene`. Output, one node per line:

```
<type>  #<name>  .class1 .class2  V  <content-size>  @(<x>,<y>)  z<order>
```

- `V` — visible. Absent = invisible (opacity 0, hidden by parent, setVisible(false)).
  **A node with opacity 0 still lays out and appears invisible here** — the common
  "why is everything gone" cause is an ancestor with `opacity:0` /
  `background-color: transparent` (see pitfalls).
- `#name` — stable hook (set in code / via `setName`); grep for it.
- `.class…` — style classes; match the CSS in the app's `.css`.
- `content size` / `position` — laid-out geometry; mismatches reveal layout bugs.
- `z<order>` — draw order within siblings.

## Reading logs

Call `get_logs`. Returns the whole current ring buffer (capped ~4096 lines). Log
levels: `[V]` verbose, `[D]` debug, `[I]` info, `[W]` warn, `[E]` error, `[F]`
fatal. Each line is `[LEVEL][tag] message`. The `installer` tag covers the
InstallerController (catalogue load, install/uninstall, engine query/prepare);
engine/framework tags (`Director`, `Context`, `vk::Loop`, `FontController`, …) come
from the engine itself.

## Debug loop

```
build (debug)  →  run  →  get_logs / inspect_scene  →  read & find the offender
     ↑                                                        ↓
     └──────────── fix code/CSS, rebuild, relaunch, re-check ←┘
```

Re-call a tool after each fix to confirm — no need to close/reopen between reads
(each call reconnects), but you **must rebuild + relaunch** for code/CSS changes.

## Known pitfalls (Xenolith-specific)

- **`background-color: transparent` hides children.** It flips a node's opacity to
  0, suppressing its children's rendering. Use an opaque-or-near-opaque color.
- **Custom window chrome** (traffic-light buttons, rounding, border) lives in
  `XLUiButton.cc` / `SPRTWinMacosWindow.mm` / `SPRTWinMacosView.mm`. First-click on
  the OS buttons being swallowed = missing `acceptsFirstMouse:` in the view.
- **Verify business logic before UI.** Call controller methods from a scene hook
  (e.g. `InstallerSceneContent::handleEnter` → `_controller->loadCatalog()`), then
  `get_logs` — confirm `[I][installer] loadCatalog: ok, rows=N` before building the
  table that displays them.

## Related

- `cli-build` skill — build/run the debug app.
- `xenolith-build` skill — low-level engine/target builds.
- Engine side: `XLSceneInspector.cc` (listener + log sink, built on
  `Looper::listenSocket`); scene graph nodes under `xenolith/application/nodes/`;
  UI atoms under `xenolith/renderer/ui/atoms/`.
