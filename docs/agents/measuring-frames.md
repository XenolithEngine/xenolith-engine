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
| `XL_SOFT_BUDGET=N` | what the wall-clock frame was spent on, split into named stages | an environment variable, software backend only |
| `XL_APP_ACCOUNT=N` | what the app thread's half of that frame was spent on | an environment variable; needs the `XL_FRAME_ACCOUNT=1` build flag |
| `XL_FRAME_TIMELINE=N` | a closed account of the whole frame, both halves and the hand-offs between them | an environment variable; needs the `XL_FRAME_ACCOUNT=1` build flag |
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

## The frame budget (`XL_SOFT_BUDGET=N`)

Software backend only, and the question one level above the rasterizer profile: not *how fast did
the rasterizer run* but *how much of the frame was the rasterizer at all*. Reports every N frames,
running averages over the whole run, same grammar as `XL_SOFT_PROFILE` (unset or `0` is off,
unparseable is 60).

```
soft::budget frames= period=…us/frame (… fps)
  wait=…us …%  vertex=…us …%  record=…us …%  clear=…us …%
  raster=…us …%  present=…us …%  other=…us …%
```

Everything the software backend does to a frame happens on the loop thread in a fixed order, so the
frame really is a sum of stages:

| stage | where | scales with |
|---|---|---|
| `wait` | previous present → first thing the render half does | the app thread's update and scene visit. `preStartFrame = false` on this backend, so nothing overlaps it |
| `vertex` | `VertexAttachmentHandle::loadVertexes` | the **scene** — every command is walked even on a frame that repaints a cursor |
| `record` | `recordSubpass` | the scene: vertex stage per vertex, material/texture resolution, glyph runs |
| `clear` | the attachment load op | the **damage** |
| `raster` | `drawTiled` — the same span `XL_SOFT_PROFILE` times | the damage, and the overdraw inside it |
| `present` | `Swapchain::present` | the damage, plus whatever the window system charges |
| `other` | `period` minus all of the above | nothing — it is the residual |

Two things the split is for:

- **Damage tracking only shrinks the bottom half of the table.** `clear`, `raster` and `present`
  follow the damage; `wait`, `vertex` and `record` follow the scene and do not care that the frame
  repainted twelve percent of the screen. A scene that grows makes frames more expensive even when
  the picture barely moves, and this is the instrument that shows it.
- **`raster` is where an ISA kernel, a tile size or a worker count can help, and nothing else is.**
  Measured on qemu-armv8a, 640×480, debug, SMP=4, steady kiosk frame: `wait` 50%, `raster` 24%,
  `vertex` 8.6%, `record` 8.2%, `other` 6.2%, `present` 2.8%, `clear` 0.3%. Halving the rasterizer
  there buys 12% of the frame. That is the number to have before optimizing anything.

`other` should stay small. It is large when frames reach present without passing through the stages
— the damage tracker skipping the pass entirely is the ordinary cause — and the report is then
describing frames it did not measure. It is also large for the first few reports of a run, before
the stage counters and the period have covered the same frames; it is clamped at zero rather than
allowed to wrap, so a run that has not settled reads as `other=0`, not as a huge number.

`wait` says where the app thread's half of the frame went, not what it went on. When `wait`
dominates, this instrument has said all it can and the next question is `XL_FRAME_ACCOUNT=1`.

### Turning it on where there is no shell

The counters are guarded by one relaxed load in five places per frame, so the instrument stays in a
shipping build. It has to: an RTOS board that boots the app as `CONFIG_INIT_ENTRYPOINT` has no
shell to set an environment variable from, and the only way a number reaches its log is for the app
to ask for it itself. `board/nuttx-qemu/apps/hello/hello_main.c` does exactly that, with
`setenv(…, 0)` so a shell can still override it where there is one.

## The app account (`XL_APP_ACCOUNT=N`)

The other half of the frame budget, and the answer to a `wait=` that turned out to be the largest
stage. Compiled in only under `XL_FRAME_ACCOUNT=1`; reported only when the variable names an
interval. Running averages over the whole run, same grammar and same interval as the budget, so the
two logs are read against each other — as averages of the same run, never as a pair of lines about
the same frame.

```
app::account frames= appHalf=…us (update=…us visit=…us) spawned/frame=…
  defer: work=…us wait=…us count/frame=… waited/frame=…
  vertexPlan: write=…us span=…us (walk: damage=…us plan=…us)
```

| | what it is |
|---|---|
| `update` | everything `acquireFrame` does before posting the visit: scheduler, actions, input, the application's own `update()` |
| `visit` | the scene graph walk that builds the command list |
| `spawned` | deferred tesselation tasks **started** by that visit |
| `defer` | the consuming side, off `DrawStat`. `work` is summed across workers and may exceed the frame; `wait` is one thread standing still and is always inside it. **Never add them** |
| `vertexPlan` | the vertex stage's own phases — the render half, not the app thread; `damage` and `plan` are nested inside the command walk |

**`spawned/frame` is the number to look at first, and a steady frame must report zero.** Anything
else means something re-tesselates every frame, and that is a property of the scene, not a cost of
the renderer.

`appHalf` well below the budget's `wait=` is not a contradiction: `wait` runs from the previous
present to the first thing the render half does, and the app thread working is only part of what is
in there. The remainder is scheduling latency between the two threads.

### The instrument that was half the frame

Measured on the NuttX kiosk, qemu-armv8a, 640×480, steady state, `Scene2d`'s FPS overlay on → off:

| | overlay on | overlay off |
|---|---|---|
| period | 38 583 µs (25.9 fps) | 18 352 µs (54.5 fps) |
| `visit` | 5 684 µs | 1 951 µs |
| `spawned/frame` | 1.41 | 0.019 |
| `raster` | 9 310 µs | 206 µs |
| `record` | 3 105 µs | 29 µs |

`Scene2d` shows the overlay in any build that is not NDEBUG. Its label restates the frame rate and
the pixel counts every frame, so `setString` differs every frame, the label re-tesselates, and a
white `Layer` the size of the text is repainted on top — which is most of the damage the rasterizer
is handed. **A frame measured with the overlay on is measuring the overlay too.** Turn it off
before drawing any conclusion about the scene; on the NuttX kiosk that is `XL_KIOSK_FPS=0`.

## The frame timeline (`XL_FRAME_TIMELINE=N`)

The instrument that answers a gap the other two cannot, because the gap belongs to neither of them.

The budget measures the render half and lands the rest in `wait`; the app account says what the app
thread did. On raspberrypi-4b `wait` was 12.9 ms of a 19.5 ms frame and the app thread worked for
1 ms of it. **The other 11.9 ms was what happens between the two.**

Six marks on the frame's own path, each bucket the interval *ending* at its mark. They close on
themselves — the six sum to the period — which is what makes a missing cost impossible to hide.

| bucket | span | what it is |
|---|---|---|
| `render` | VertexStart → Presented | the render half; the backend budget splits this one further |
| `postPresent` | Presented → Scheduled | engine bookkeeping after a present, plus the deliberate wait for the present window when a target frame interval is set |
| `toApp` | Scheduled → AcquireStart | **getting from the loop thread to the app thread** |
| `update` | AcquireStart → VisitStart | scheduler, actions, input, plus the "break current stack frame" hop |
| `visit` | VisitStart → VisitEnd | the scene graph walk |
| `toLoop` | VisitEnd → VertexStart | back to the loop thread, frame graph setup included |

**Three of the six are thread hand-offs, and a hand-off is not free** — which is what this
instrument was built to find, and what it found on its first run.

On raspberrypi-4b, 2400 frames: `toApp` **47.5%** of an 18.6 ms frame — 8.8 ms for one
`performOnAppThread` (`AppWindow::acquireFrameData`) to be picked up by a thread that was idle for
most of the frame. Together with `toLoop` that was 53.7% of the frame spent on two threads waiting
to notice each other, against 45.8% of actual work.

It was a bug, in `SPEvent-nuttx.cc`. `spinWait` had a guard for a *stopped* clock — after 64
`sched_yield()` calls with no visible time passing it gave up and slept the whole remaining
timeout, deaf to `_wakeupReq`. On this board the clock is not stopped, it is **coarse**: 64 yields
take ~150 µs and `CLOCK_MONOTONIC` advances once a millisecond, so the guard fired on every wait
and every wait became a deaf 16 ms sleep. 8.8 ms is the average latency of a post landing at a
random point in one. Three fixes: pick the clock by measured resolution (the same probe the account
clock uses), sleep in one-tick slices rechecking the flag between them, and drain the wakeup
*before* firing handles rather than after so a signal arriving mid-iteration is not cleared unserved.

After: `toApp` 28.7% → **5.7%** on qemu-armv8a, frame 42.0 ms → 30.9 ms, and `render` became the
largest bucket at 65% — which is where a renderer's frame is supposed to spend itself.

**The lesson generalises past this one bug.** A hand-off costs whatever the waiting side's wake-up
path costs, and on an RTOS that path is easy to get wrong in a way no correctness test notices: the
frames still render, in order, with the right pixels. Only a closed account of the period shows it.

**The marks assume frames do not overlap.** They are recorded in sequence from three threads, and
concurrent frames would interleave them and make every bucket meaningless. The software
presentation engine sets `preStartFrame = false`, so this holds there; a backend that starts a
frame early must not turn this on.

## The clock all three instruments read

`core::getAccountClock()`, and it is not `nanoclock(Monotonic)` — the difference is not academic.

On a tickless desktop CLOCK_MONOTONIC is the right source and resolves to nanoseconds. On an RTOS
it need not be: NuttX with `CONFIG_USEC_PER_TICK=1000` and no `CONFIG_SCHED_TICKLESS` advances it
once a millisecond, and every phase measured here is shorter than that. Measured on raspberrypi-4b,
600 frames: `update`, `span`, `damage` and `plan` all reported **exactly 0.0**, and every total was
an exact multiple of 1000 µs — the signature of a quantized clock, not of free work. The frame
budget showed microsecond detail on the same run, because `Time::now()` reads CLOCK_REALTIME and on
that build it is the finer of the two.

So the source is chosen by **measuring** it, not by name. `clock_getres` cannot be trusted for this
— NuttX answers it with the tick period for both clocks even when one is finer — so the probe reads
each clock until it changes and takes the step, once, at first use. Monotonic wins ties; realtime is
taken only when measurably better, and the boards where it wins have no RTC and nothing to step it.

**Every account site reads this one clock**, because the numbers are compared and subtracted across
modules and two sources of different resolution would produce differences that are neither. The
resolution is printed in the app account and the timeline (`clock= res=`): a number below the
clock's own step is not a measurement, and a reader must be able to see that without knowing the
board.

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
