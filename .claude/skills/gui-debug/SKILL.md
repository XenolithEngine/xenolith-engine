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

## Rendering is on demand — an untouched window does not advance

The engine only produces a frame when something is dirty. A window nobody is
touching therefore **freezes**: a scheduled action does not tick, a timed phase
never fires, a state change made from a callback is never laid out — and then
everything catches up at once as soon as you move the mouse over the window. In a
headless/automated check nobody moves the mouse, so this reads as "my fix did
nothing" or "the test never ran".

**Anything you want to verify for reactivity or interactivity must hold the loop
open itself.** Attach a render-lock action to the layout under test:

```cpp
#include "XLAction.h"
runAction(Rc<RenderContinuously>::create());       // forever
runAction(Rc<RenderContinuously>::create(3.0f));   // for N seconds
```

It draws nothing, damages nothing and changes no state — it just keeps frames
coming. `tests/window` does this for every test in `TestLayout::init()`, so a new
layout there inherits it; a scaffolded app or the installer does not, and needs
its own.

Symptoms that are really this and not a bug in your change: the scene inspector
shows stale sizes that become correct after you touch the window; an animation
that only runs while the pointer moves; `get_logs` missing a phase you scheduled
with `DelayTime`.

**A sleeping monitor stops the loop the same way, and there the render lock does
not help** — the compositor stops delivering frame callbacks, so nothing chains
the next frame and every timed phase in the app silently stalls. It looks exactly
like a hung test. Wake the screen before a headless run and it all proceeds:

```sh
kscreen-doctor --dpms on     # KDE/Wayland; the same trap on any idle session
```

Take this seriously: a run that reports no phase output at all, on a test that
worked minutes ago, is almost always this and not your change.

Screenshots on a KDE/Wayland session: `spectacle -b -n -a -o shot.png` grabs the
active window (the app, freshly launched, usually is it), `-f` grabs the whole
screen. `import -window root` does NOT work — Xwayland here is rootless, so the
root window is empty.

One thing hides the freeze by accident: the FPS counter is marked `AlwaysDirty`,
so a scene showing it looks busier than it is. Turning it off
(`setFpsVisible(false)`, as the damage test does) exposes the real behaviour.

## Known pitfalls (Xenolith-specific)

- **A transparent node hides its children — by design.** Setting a colour with alpha
  (CSS `background-color: transparent`, or `setColor(c, /*withOpacity*/true)`) writes
  that alpha into the node's opacity, and opacity multiplies down the whole subtree,
  exactly like CSS `opacity`. `inspect_scene` still marks the children `V`, because
  that flag is `setVisible`, not the resulting transparency. If you want an invisible
  container with visible children, use a plain `Node` — it draws nothing and leaves
  opacity alone — instead of a transparent `Layer`.
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
