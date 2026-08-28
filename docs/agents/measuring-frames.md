# Measuring a frame

*What a frame costs, what part of it is deferred work, and the facts about driving a headless window
that make the difference between a number and a wrong number.*

*Part of the [build & test guide](../../AGENTS.md).*

There are two instruments in this repository and they answer for different halves. Neither is on by
default, and that is deliberate: **a build that measures itself is not the build that ships**, and
the frame path is the last place an unconditional clock read belongs.

| instrument | answers | how it is turned on |
|---|---|---|
| `XL_FRAME_ACCOUNT=1` | what a whole frame cost, split into the visit, the render half, and deferred work vs waiting for it | a build flag, passed to the project makefile |
| `XL_SOFT_PROFILE=N` | what the software rasterizer cost, per frame | an environment variable, software backend only |
| `XL_FONT_CACHE_LOG=1` | every batch of glyphs that actually reaches the atlas | an environment variable |
| `DrawStat::pixelsFilled` | the same fill number, live on screen | always on, shown by the FPS overlay when a backend fills it |

## The frame account (`XL_FRAME_ACCOUNT=1`)

Compiled out entirely unless the flag is set. `LOCAL_CXXFLAGS` in a project makefile reaches the
module sources, which is why a flag set in the project reaches the engine — and it has to, because
half of what is measured lives here. See `tests/graph/Makefile` in the studio repository for the
pattern:

```make
LOCAL_CXXFLAGS := $(if $(filter 1,$(XL_FRAME_ACCOUNT)),-DXL_FRAME_ACCOUNT=1)
```

What it adds:

- `Director::getLastAppFrameTime()` — the app half of ONE frame, exactly, not the twenty-frame
  moving average `getDirectorFrameTime()` reports. Covers `acquireFrame` whole: the update, the
  scene visit, and everything a node does inside it. **The account is closed after the visit, not
  where `acquireFrame` returns** — the visit is deliberately posted to a later turn of the loop
  ("break current stack frame"), so a clock taken at the bottom covers the update and the posting
  and nothing else. Measured before this was noticed: 800 ns for a frame that walked three hundred
  nodes, which is the shape of a measurement that ended too early.
- `Director::getLastDeferredSpawned()` — deferred tasks STARTED by that visit. The producing side's
  answer to "did this frame tesselate", and the only honest way to tell an initializing frame from
  a steady one.
- `DrawStat::deferredWorkTime` / `deferredWaitTime` / `deferredCount` / `deferredWaited` — the
  consuming side, carried back on the channel `pushDrawStat` already uses.
- `DrawStat::frameOrder` and `FrameTimingInfo::lastFrameOrder` — **which frame each half is about**.

### The two deferred numbers are two categories and may never be added

- **Work** is stamped inside the task, on a worker thread, by `DeferredVertexResult::setWorkTime`,
  and summed across tasks. It **may exceed the frame** it belongs to, because several run at once.
- **Wait** is measured around `acquireResult` on the thread that stands still, and is **always part
  of the frame**.

Work far above wait is deferral doing its job. The two roughly equal means deferral bought nothing —
which is what a single large sprite looks like, since there is nothing to overlap with.

The work is **taken, not read** (`takeWorkTime`). A sprite whose content did not change re-pushes
the same result every frame and the consumer takes it again each time; a plain getter would report
the tesselation cost forever and refute the very claim the account exists to make.

### Both halves of a frame do not arrive together

The visit and the vertex stage are known as soon as the frame is built; the render half only when
the frame COMPLETES — and **the next frame starts when the previous one is ready, not when it
completes**. A reader that takes both as they come is describing two different frames. Measured, and
consistently: the two numbers were one apart on every frame of a run. File each half under the frame
it names, and count a record only once both are in.

A frame that is INVALIDATED never reaches the timing block at all, so a record waiting for its
completion waits forever. Retire any pending frame older than the newest completion, or the run does
not terminate.

## Three facts about driving a headless window

Each was measured, each contradicts something that was written down here first, and each kills an
obvious way of driving frames.

| asked | frames produced |
|---|---|
| `frame` with `count: 10`, one call | **2** |
| five separate `frame` calls, each awaited | **5** |
| `screenshot` alone | **0** |

- **`count` is a flag, not a queue.** `PresentationEngine::setReadyForNextFrame` sets a `bool` and
  clears it when a frame is scheduled; the inspector's `frame` command calls it in a loop, so the
  extra calls are dropped. `tests/parity/benchclient.py` says the same thing in its header.
- **A capture renders nothing.** In a headless window `captureScreenshot` reads back the last
  PRESENTED image (`XLSoftPresentation.cc`, `XLVkHeadlessPresentation.cc`) and returns. The rule
  recorded across several subtasks — "a capture is what forces the visit" — is an artifact of
  latency: the round trip merely lasts long enough for the flag `frame` had already set to be acted
  on. It is still a correct way to WAIT; it is not the cause.
- **A socket command is carried out by the app thread**, the same thread the frame needs. A client
  polling "has it happened yet" competes with what it is waiting for. Measured: at eight thousand
  nodes a 20 Hz poll of a command that touches the scene stopped frames happening at all.

The consequence for any harness: **do not poll, and do not capture to force a frame.** Drive one
frame, then wait for evidence that it happened — a counter incremented during the visit — or better,
let the run live inside the scene and answer once when it is done. The studio repository's
`tests/graph/frame-bench.py` and its `canvas.frame-bench` command are the worked example, and they
assert all three facts at the start of every run so that a change here is caught before any table is
printed.

## The rasterizer profile (`XL_SOFT_PROFILE=N`)

Software backend only; there is no equivalent hook on the Vulkan path. Reports every N frames on the
loop thread, around the whole fork-and-join: `kernels= threads= frames= regions/frame= tiles/frame=
px/frame= us/frame= Mpx/s=`. Driven by `tests/parity/bench.sh`, which is worth reading for its own
rules:

- `XL_SOFT_FORCE_FULL_REDRAW=1` is not optional — with damage tracking on, a static scene skips its
  frames and every kernel set measures the same zero.
- The script reads `frames=` and `kernels=` back **from the engine** rather than trusting what it
  asked for, because a set that silently fell back would produce a real measurement of the wrong
  thing.
- **Never print a number under a label that was not run.**

### The fill line

A second line follows each report and answers a different question — not how fast the rasterizer
ran, but how much it was asked to do:

```
fill: surface/frame= damage/frame= filled/frame= (span= glyph= rect=) damage/surface= filled/damage=
```

Three quantities that must not be confused:

| | what it is |
|---|---|
| `surface` | the window. Fixed. |
| `damage` | what the tracker handed the rasterizer. Equal to `surface` means the damage protocol narrowed nothing, whatever the reason — that is what `damage/surface=1` reports. |
| `filled` | pixels the kernels actually wrote, counted at `writeSpan`, `blitGlyph` and `fillRect` and nowhere else. Writes issued, so a pixel covered by two commands counts twice. |

So `damage/surface` answers *"is this a full repaint"* and `filled/damage` answers *"and how much
work goes on inside whatever it repaints"* — the overdraw. They move independently: a frame can
repaint 10% of the screen and still burn ten times that region in overlapping commands.

`span=`/`glyph=`/`rect=` split `filled` by kernel, which is what says where the work went. `rect=`
equal to `damage=` is the attachment clear covering each damaged region exactly once, as it should.

The counters are only touched when `XL_SOFT_PROFILE` is on: `draw` and `fillRect` take the stats
block as an optional out-param and skip it when null, and `drawTiled` merges one add per worker at
the end of its run rather than one per tile, so no contended cache line lands in the pixel loops.

**Each counter must mirror its kernel's own clip exactly.** `blitGlyph` walks the glyph box
intersected with the scissor, *not* the scissor — counting the scissor instead over-reports by
roughly the glyph count per frame, which on a text-heavy scene reads as a 100x overdraw that is not
there. A metric of the wrong thing is worse than no metric; whoever changes a kernel's clip changes
its counter in the same edit.

## Rules for any timing run

- **Release.** A debug build does not scale the numbers, it reorders them.
- **A quiet machine**, and pin it (`taskset`). A working desktop drifts several percent between
  runs — more than most changes are worth.
- **Counts beside times.** A count is a claim that reproduces on any machine; a millisecond is a
  fact about one. A change that halves a time without moving any count saved it somewhere the
  account does not name.
- **Report what was not measured.** A size that timed out is a finding about that size, not a row to
  leave out.

## The fill number on screen (`DrawStat::pixelsFilled`)

`XL_SOFT_PROFILE` is a running average printed to the log; the same quantity is also carried on
`DrawStat` so it can be read live:

- `DrawStat::pixelsTotal` — the target, in pixels.
- `DrawStat::pixelsFilled` — what the kernels wrote for that frame, the sum of the same three
  counters the `fill:` line breaks down.

Filled by `basic2d::soft::FlatPassHandle` and nothing else. A GPU backend leaves both at zero: the
nearest equivalent would be a fragment count out of a query pool, which is a different quantity, and
reporting one under this label would be a real measurement of the wrong thing. **Zero therefore
means "not measured here"**, which is why the FPS overlay prints the `Px:` line only when
`pixelsTotal` is non-zero instead of printing `0/0`.

The overlay shows it in the `Fps` and `Full` modes (F12 cycles them):

```
Px: 123780/307200 0.4029297x
```

Read the ratio as "this frame cost 0.40 of a full repaint". It is not bounded by 1: overlapping
commands inside a small damage region can push it well above one while the damage stays tiny.

## Glyph cache churn (`XL_FONT_CACHE_LOG=1`)

Reports every batch the font controller submits to the atlas. A batch carries the whole required
set and rebuilds the atlas image from scratch, plus a material recompile in every window that
samples it — so on a scene whose text does not grow there must be a handful during warm-up and then
none. A steady trickle means glyphs are being dropped and rendered again.

Pairs with the pre-existing `FontController: Removed:` line from `removeUnusedLayouts()`: this one
says a set was rendered, that one says a set was dropped. `XL_FONT_EVICT_ALWAYS=1` and
`XL_FONT_EVICT_THRESHOLD` drive the eviction side for a side-by-side.

A set is a candidate for dropping only when nothing holds it (`getReferenceCount() == 1`) and it is
not persistent, so a font size used by a label that is still on screen is never a candidate — not
even under `XL_FONT_EVICT_ALWAYS=1`. Reading a run therefore means checking both lines: no batches
after warm-up AND no removals.

## How the two thread pools are split

`sprt::window::config` sizes them from `thread::hardware_concurrency()`:

| CPUs | main (`mainThreadsCount`) | app (`appThreadsCount`) |
|---|---|---|
| 0 (unknown) or 1 | 0 | 0 |
| 2 | 2 | 1 |
| 4 | 3 | 1 |
| 8 | 5 | 3 |
| n | `n/2 + 1` | `n/2 - 1`, floor 1 |

Not an even split above one CPU, and deliberately. The frame is produced on the main side: the
software rasterizer fans its tiles out to the main pool and the submitting thread takes tiles too,
so `main + 1` is what bounds the fan-out. An even split left it one worker short on every even core
count — on four cores the rasterizer could only ever occupy three of them (`threads=3.0` in the
profile line, against `4.0` now). App-side work is latency-bound rather than throughput-bound and
does not scale the same way.

**Zero on a single CPU is a real answer, not a degenerate one.** A Looper with no workers runs
`performAsync` on its own thread queue, and `drawTiled` with one available thread walks the tiles
itself; both are supported modes, and on one core they are the right ones — a worker there is a
task with a stack that can only take the CPU away from the thread it is helping. Hosted RTOS
targets used to get this from a hard-coded zero in `XLContext`/`XLAppThread`; it is now a property
of the machine, so a single-core NuttX or Embox image keeps exactly the behaviour it had, and a
multi-core one gets workers with no special case. Embox is single-core on arm/aarch64 either way:
`embox.arch.smp` is implemented for x86 and riscv only, so those builds resolve to
`embox.arch.generic.nosmp` and `NCPU` is 1.

`hardware_concurrency()` returns 0 when it cannot tell, which lands in the same branch as one CPU.

Read the result back from `threads=` in the `XL_SOFT_PROFILE` line — it reports what the rasterizer
actually got, not what it asked for, and a pool that could not supply the workers looks identical
from the outside otherwise. Verified on qemu-armv8a: `CONFIG_SMP_NCPUS=4` gives `threads=4.0`, the
same image built without `CONFIG_SMP` gives `threads=1.0`.
