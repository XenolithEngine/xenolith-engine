---
name: gui-debug
description: >-
  Debug and drive a Xenolith app over its inspector socket — headless by default
  (`--headless`, no window system), so a GUI can be inspected, screenshotted and
  driven with no display, no compositor and no mouse. MCP tools: `inspect_scene`
  (the node tree), `get_logs` (the log ring buffer), `screenshot`,
  `list_commands`/`invoke_command` (scene-registered actions), `send_input`,
  `step_frame`, `window_control`, `quit_app`. Use when a Xenolith GUI app
  (installer, scaffolded app, tests/window) is misbehaving visually — missing
  elements, wrong layout, invisible nodes, overlap, wrong stacking, an element you
  can't find, "why is this hidden", "where did this node go"; when verifying
  business logic (catalogue load, install progress, errors, crashes) works BEFORE
  or independently of the UI; or when you need a screenshot of an app on a machine
  with no session at all. Complements source-level debugging (lldb/wine-debug).
---

# Debugging a Xenolith GUI: run it headless, drive it over the socket

**Default to `--headless`.** It is the same engine and the same renderer with the
window system removed: Vulkan draws into a pseudo-swapchain of ordinary device
images instead of a compositor surface. Measured on `tests/window`
(Linux/Vulkan), the headless frame is **byte-identical to the windowed one** —
0 differing pixels out of 3 145 728 outside the live FPS counter. So a headless
screenshot is evidence about the real rendering, not an approximation.

What you gain by not opening a window:

- **No display needed** — works over SSH, in CI, on a server, with the monitor
  asleep.
- **Deterministic frames.** Headless renders on demand: `step_frame` draws
  exactly N frames. No "the window froze because nobody moved the mouse", no
  render-lock actions, no sleeping-compositor stalls (see "Windowed mode" below).
- **Resizable from the client.** `window_control op:"resize"` really works; with
  a WM in play the size is the WM's to decide.
- **Nothing to clean up.** No stray window stealing focus, no screenshot tool.

Reach for a real window only for the short list under "Windowed mode" below.

## Start the app

```sh
XENOLITH_INSPECTOR_ADDRESS=unix:/tmp/xl-$$.sock \
  ./myapp --headless --width 1024 --height 768 &
```

- **Use a per-run socket path.** The default `/tmp/xenolith-inspector.sock` is
  unlinked before bind, so two apps silently fight over it. Point the MCP client
  at the same `XENOLITH_INSPECTOR_ADDRESS`.
- **`--width/--height` are the surface in pixels.** `--density` only changes the
  logical scale — it does *not* multiply the surface. To reproduce a HiDPI window
  (a 1024×768 window at density 2), pass the physical size explicitly:
  `--width 2048 --height 1536 --density 2`.
- **Stop it with `quit_app`**, not a kill: it closes the window, tears the context
  down and exits with status 0.

The listener is armed in DEBUG builds, whenever `XENOLITH_INSPECTOR_ADDRESS` is
set, and always in headless mode — so a release build works too.

## Tools

| Tool | What it does |
|---|---|
| `inspect_scene` | live scene graph as an indented text tree (one node per line) |
| `get_logs` | application log ring buffer (last ~4096 entries, `[LEVEL][tag] message`) |
| `step_frame` | render N frames — headless draws nothing until you ask |
| `screenshot` | write the current frame to a PNG |
| `list_commands` | the actions this scene registered for external control |
| `invoke_command` | run one of them |
| `send_input` | inject synthetic pointer/key events |
| `window_control` | read constraints, resize, close |
| `quit_app` | shut the process down |

### When to use which

- **`inspect_scene`** — visual/structural problems: invisible button, wrong layout,
  overlap, z-order, "did my node appear", "why is everything gone" (ancestor
  opacity 0 / `background-color: transparent`). Read content-size/position/z-order.
- **`get_logs`** — behavioral problems: did the catalogue load? did install start?
  what error did the worker throw? did the controller init? Use it to verify
  business logic **before** building UI (call the controller methods from a scene
  hook, then `get_logs`), and to diagnose crashes/misbehavior without a debugger.
- **`screenshot`** — "what does it actually look like", when the tree reads fine
  but the render does not. **Always `step_frame` first** (see below).
- **`list_commands` / `invoke_command`** — drive app-specific actions the scene
  chose to expose, without synthesizing input.
- **`send_input`** — exercise the real input path (hit-testing, gestures, focus)
  rather than calling code directly.

### The order that matters: step, then shoot

Headless renders only when asked. A `screenshot` without a preceding `step_frame`
returns the *previous* frame — or falls back to rendering one offscreen if nothing
has ever been presented. So every visual check is:

```
step_frame (count: 1-3)  →  short pause  →  screenshot
```

Same after anything that changes the scene: `invoke_command` / `send_input` /
`window_control resize` → `step_frame` → `screenshot`. If a screenshot looks
stale, you skipped the step.

## Debug loop

```
build (debug)  →  run --headless  →  get_logs / inspect_scene / step_frame+screenshot
     ↑                                                        ↓
     └──────── fix code/CSS, rebuild, quit_app, relaunch, re-check ←┘
```

Re-call a tool after each fix to confirm — no need to close/reopen between reads
(each call reconnects), but you **must rebuild + relaunch** for code/CSS changes.

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

## Scene-registered commands

`list_commands` / `invoke_command` reach whatever the running scene chose to
expose. A scene registers them in its `init()`:

```cpp
#include "XLSceneInspector.h"

inspector::addCommand(getContent(), "reload", "Reload the catalogue",
        [this](Value &&args, Function<void(Value &&)> &&done) {
    _controller->loadCatalog([done = sp::move(done)](bool ok) mutable {
        Value result;
        result.setBool(ok, "ok");
        done(sp::move(result));   // may be called later, from another thread hop
    });
});
```

This is the cheapest way to make an app driveable: one command per action you want
to trigger from outside, and the whole flow becomes scriptable without synthesizing
input. `tests/headless` is a minimal worked example (a coloured box exposing
`set-color` and `box-size`); `tests/window` is the full one — `layouts` lists its
26 demo layouts, `layout` switches to one (and answers only once the new layout has
settled, so the reply is the signal to shoot), and whichever layout is on screen
adds its own `<name>.<action>` commands for the duration.

## Windowed mode — only when the window manager is the subject

Drop `--headless` when the thing under test *is* the window system:

- window decorations, chrome, traffic-light buttons, rounding, borders;
- real WM-delivered input, IME/text input, cursors, drag & drop;
- fullscreen, multi-monitor, per-monitor DPI, display-link pacing;
- compositor-specific bugs (Wayland vs XCB paths).

The rest of this section applies to that mode only — headless has none of these
traps.

**An untouched window does not advance.** The engine only produces a frame when
something is dirty, so a window nobody is touching **freezes**: a scheduled action
does not tick, a timed phase never fires, a state change from a callback is never
laid out — and everything catches up at once as soon as you move the mouse. In an
automated check nobody moves the mouse, so this reads as "my fix did nothing".
Anything you want to verify for reactivity must hold the loop open itself:

```cpp
#include "XLAction.h"
runAction(Rc<RenderContinuously>::create());       // forever
runAction(Rc<RenderContinuously>::create(3.0f));   // for N seconds
```

`tests/window` does this for every test in `TestLayout::init()`; a scaffolded app
or the installer does not. Symptoms that are really this: stale sizes in
`inspect_scene` that become correct after you touch the window; an animation that
only runs while the pointer moves; `get_logs` missing a `DelayTime` phase.

**A sleeping monitor stops the loop the same way, and the render lock does not
help** — the compositor stops delivering frame callbacks, so nothing chains the
next frame. It looks exactly like a hung test:

```sh
kscreen-doctor --dpms on     # KDE/Wayland; the same trap on any idle session
```

A run that reports no phase output at all, on a test that worked minutes ago, is
almost always this. **Both traps vanish in headless mode** — `step_frame` drives
the loop directly.

**Screenshots.** Prefer the `screenshot` tool even here: it captures the app's own
frame, without decorations or compositor scaling. Grab the composited window only
when the decorations are what you are checking: `spectacle -b -n -a -o shot.png`
(active window), `-f` (whole screen). `import -window root` does NOT work —
Xwayland here is rootless, so the root window is empty.

One thing hides the freeze by accident: the FPS counter is marked `AlwaysDirty`,
so a scene showing it looks busier than it is. Turning it off
(`setFpsVisible(false)`, as the damage test does) exposes the real behaviour — and
also makes two screenshots comparable, since the counter is the one region that
differs between any two runs.

## Known pitfalls (Xenolith-specific)

- **A transparent node hides its children — by design.** Setting a colour with alpha
  (CSS `background-color: transparent`, or `setColor(c, /*withOpacity*/true)`) writes
  that alpha into the node's opacity, and opacity multiplies down the whole subtree,
  exactly like CSS `opacity`. `inspect_scene` still marks the children `V`, because
  that flag is `setVisible`, not the resulting transparency. If you want an invisible
  container with visible children, use a plain `Node` — it draws nothing and leaves
  opacity alone — instead of a transparent `Layer`.
- **Bundled resources still have to be found.** Headless changes nothing about
  resource lookup: `Fail to add image: …, file not found` in `get_logs` means the
  app was launched from the wrong working directory, not that headless broke it.
- **Custom window chrome** (traffic-light buttons, rounding, border) lives in
  `XLUiButton.cc` / `SPRTWinMacosWindow.mm` / `SPRTWinMacosView.mm`. First-click on
  the OS buttons being swallowed = missing `acceptsFirstMouse:` in the view.
- **Verify business logic before UI.** Call controller methods from a scene hook
  (e.g. `InstallerSceneContent::handleEnter` → `_controller->loadCatalog()`), then
  `get_logs` — confirm `[I][installer] loadCatalog: ok, rows=N` before building the
  table that displays them.

## Transport (only if you are writing a client)

One listener address, two protocols sharing it. `inspect_scene` and `get_logs` use
the original one-shot text form (send `scene\n` / `logs\n`, read until EOF).
Everything else opens a framed session (`xenolith/1 json\n`, then
`[u32 LE size][JSON payload]` request/response frames correlated by `serial`),
which is what makes binary screenshots and long-lived sessions work. **The
handshake is answered with a greeting LINE — `# xenolith/1 ok json\n` — before
any frame; read it to the newline first.** A client that starts framing straight
away consumes those bytes as a length and then blocks forever, which looks
exactly like "the app executes commands but never answers". The request key is
`cmd` (`scene`, `logs`, `commands`, `invoke`, `screenshot`, `input`, `frame`,
`window`, `quit`), not `command`. Binary
payloads arrive as `"BASE64:<base64url, unpadded>"`. Address format:
`unix:/path`, `unix:@abstract`, `host:port` or `:port`. Per-platform defaults:

| Platform | Default |
|---|---|
| Linux / macOS | `unix:/tmp/xenolith-inspector.sock` |
| Android | `unix:@xenolith-inspector` (abstract; from the host: `adb forward tcp:4490 localabstract:xenolith-inspector` and point the client at `127.0.0.1:4490`) |
| Windows | `127.0.0.1:4490` (TCP loopback — Python has no practical AF_UNIX there) |
| wasm | not available (no sockets in the browser sandbox; the inspector stays off) |

## Related

- `cli-build` skill — build/run the debug app.
- `xenolith-build` skill — low-level engine/target builds.
- Engine side: `XLSceneInspector.cc` (listener, protocols, command registry and
  log sink, built on `Looper::listenSocket`); the headless controller in
  `runtime/window/headless/`; the pseudo-swapchain in
  `xenolith/backend/vk/XLVkHeadlessPresentation.cc`; scene graph nodes under
  `xenolith/application/nodes/`; UI atoms under `xenolith/renderer/ui/atoms/`.
