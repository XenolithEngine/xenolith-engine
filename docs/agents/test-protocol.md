# The test protocol: what to run inside an iteration, and what before a commit

*What a change owes at each stage, what each stage costs (measured), which harnesses may run at once, and the
runner that decides all three.*

*Part of the [build & test guide](../../AGENTS.md). What each harness IS lives in
[the ready-made test projects](test-projects.md); building one, in [the quick reference](quick-reference.md).*

## The three tiers

```sh
tests/run-checks.py                  # FAST    - what the working tree's diff can break
tests/run-checks.py console          # CONSOLE - the five console harnesses, no window at all
tests/run-checks.py suite window     # every headless window check
tests/run-checks.py full             # THE GATE - every console harness and all 29 window checks
tests/run-checks.py --list           # the plan, without running it
```

| Tier | What it runs | Cost here | When |
|---|---|---|---|
| `console` | `runtimetest`, `libctest`, `localetest`, `uilayouttest`, `stapplertest` | **12 s**, 3253 assertions | after any edit under `runtime/` or `stappler/` |
| `fast` (default) | `console`, plus the harnesses the changed directories owe, plus the window checks named after the changed files | 20 s – 2 min | after an edit, before the next one |
| `suite window` | all 29 headless window checks | 226 s at `-j4` | when the work is in `xenolith/renderer/ui` |
| `full` | everything above plus `gittest`, `thirdpartytest`, `remotetest` and `tesstest`'s two goldens | 223 s at `-j4`, 124 s at `-j8` | **before a commit** |

**The gate is `full`.** A `fast` run selects by name and by directory, so a change that breaks a widget it is not
named after is invisible to it by construction.

## What it costs, measured

On a 16-core Linux host, debug builds, nothing else running (2026-09-13):

| | jobs | assertions | `-j1` | `-j4` | `-j8` |
|---|---|---|---|---|---|
| the console harnesses | 5 | 3253 | 16 s | **12 s** | 12 s |
| the window checks | 29 | 1631 | 761 s | 226 s | **136 s** |
| the whole gate (`full`) | 35 | 4926 | ~13 min | 223 s | **124 s** |

`runtimetest` is 12 s of the console tier and `stapplertest` 4 s; `libctest`, `localetest` and `uilayouttest` are
under a second each. The window suite's own tail is `slider-check` and `menu-check` at 66 s, then `inline-edit`
(61 s), `chip` (58 s) and `picker` (51 s) - and at the other end `scale9` (3 s), `clipboard` (4 s) and `panel` (4 s).

## What may run at once

**Every window check, and it is proved rather than assumed.** The suite was run at `-j1`, `-j4` and `-j8`, and the
whole gate three times more: green. Contention costs job time and not correctness - 761 s of work becomes 823 s at
`-j4` (+8 %) and 932 s at `-j8` (+22 %) - and unlike the studio's suite this one scales all the way to `-j8`, because
its longest script is 66 s rather than two minutes.

**One script did fail at `-j8`, twice out of two, and it was the script's own budget rather than contention as such.**
`menu-check.py` waited 200 x 0.05 s = 10 s for its `testapp` to bind its socket, and `geometry-check.py` 15 s, where
every other check here waits 30 s; under eight parallel harnesses a startup can take longer than ten seconds, and the
run died with `app did not come up` and nothing else wrong. Both wait 30 s now, and two gates at `-j8` were green
afterwards. **The rule this leaves: a check waits 600 x 0.05 s for its harness**, and a lower number is a script that
will fail on a busy machine and nowhere else.

Each script starts its own `testapp` on its own socket (`/tmp/xl-<name>.sock`, overridable through
`XENOLITH_INSPECTOR_SOCK`) and quits it at the end, so nothing is shared but the machine. What a script collides with
is itself: one runner at a time.

**Stale `testapp`s are killed before every run and reported after it.** A script that dies between its start and its
`quit` leaves a headless binary alive for ever, and the next run then talks to whatever the socket is bound to.

## Two exceptions, and they are exceptions on purpose

- **`xcb-side-check.py` is not headless and cannot be in any tier.** It drives a real X11 session with XTEST and takes
  the keyboard focus, so it fails whenever another window holds it. Run it by hand after touching `XcbWindow`'s key
  handling or `getKeySideModifier`: `XL_TEST_DISPLAY=:1 tests/window/xcb-side-check.py`.
- **`markdown-perf-check.py` measures rather than checks**, and as things stand it cannot run at all: it regenerates
  its corpus with a `gen-big-md.py` that is not in the repository. Times belong to
  [measuring a frame](measuring-frames.md) anyway - a release build and a quiet machine.

## How the selection works

The console harnesses go by directory, which is [the test-projects table](test-projects.md) as code (`OWES` in the
runner): `runtime/` owes `runtimetest` and `libctest`, `runtime/libc_impl` the same pair the other way round,
`stappler/tess` the two `tesstest` goldens, `stappler/` `stapplertest`, `xenolith/font` `localetest`,
`xenolith/core` and `xenolith/backend/vk` `computetest`. `computetest` is not in the `console` tier because it needs
a Vulkan device; on a host without one it prints SKIP and counts no checks, and the runner shows it green.

The window checks go **by name**, because this repository names each script after the widget it drives:
`XLUiSlider.cc` selects `slider-check.py`, `XLUiInlineEditor.cc` selects `inline-edit-check.py`. The file name is
split on camel case rather than searched as a string - a substring search answers `text-input-check` for
`XLContext.cc`, and a plan with four wrong scripts in it is one nobody reads.

It is a heuristic and the runner treats it as one: a `xenolith/` change that names no widget still gets the five-script
`WINDOW_SMOKE` (style, geometry, scale9, canvas, hotkey - 39 s, ~250 assertions, and between them they touch layout,
the stylesheet, hit-testing, the canvas and the hotkey registry), and a path that matches nothing at all widens the
plan to everything and says so.

## What the protocol does not cover

- **Other targets.** Windows, Android, macOS and wasm are verified by BUILDING for them, and `tests/com`, `tests/wwin`,
  `tests/wthread`, `tests/mtl`, `tests/auxui`, `tests/wasm` are their harnesses:
  [verifying on the right target](cross-target.md).
- **`tests/headless`** needs a GLES build (`GLES=1`) and is driven by its own `gles-clear-check.py`.
- **Times.** [Measuring a frame](measuring-frames.md): a release build, a quiet machine, and no polling for frames.

## The studio, in the same shape

`xenolith-studio` carries the same tool and the same tiers (`tests/run-checks.py` there: `fast`, `suite <stand>`,
`console`, `full`) with its own measured table in its `docs/agents/test-protocol.md`. A change that spans both
repositories owes the gate on both, and the engine's first.
