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

## Rules for any timing run

- **Release.** A debug build does not scale the numbers, it reorders them.
- **A quiet machine**, and pin it (`taskset`). A working desktop drifts several percent between
  runs — more than most changes are worth.
- **Counts beside times.** A count is a claim that reproduces on any machine; a millisecond is a
  fact about one. A change that halves a time without moving any count saved it somewhere the
  account does not name.
- **Report what was not measured.** A size that timed out is a finding about that size, not a row to
  leave out.
