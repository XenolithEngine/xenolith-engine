# AGENTS.md — Building & testing Xenolith/Stappler projects

Guidance for AI agents working in this monorepo. It tells you how to **create** a
project and how to **build and test** it for the four primary targets: Linux/glibc,
Windows, Android (NDK), and macOS.

Read this before running any build. Prefer these patterns over inventing your own
`clang`/`cmake` invocations. The engine's build machinery lives under
`make/universal.mk`, but **agents drive builds through `xenolith-cli`**
whenever it is installed — that CLI configures the SDK toolchains and then
invokes make for you. Raw `make` is only the fallback when the CLI is missing.

Whether a freshly built binary can be **run** depends on having the matching OS:
a Linux binary runs on Linux, a Windows `.exe` runs on Windows or under Wine, an
Android `.so` runs on an Android device/emulator, a macOS `.app` runs on macOS.
Building (compile-verification) always works from any supported host; running may
require the target platform.

---

## The chapters

| Chapter | Read it before |
|---|---|
| [Golden rules](docs/agents/golden-rules.md) | anything else — these are the ones a build breaks silently without |
| [The build system in one screen](docs/agents/build-system.md) | passing a flag, or looking for where the output landed |
| [Quick reference](docs/agents/quick-reference.md) | building or running for a specific target |
| [Per-platform detail](docs/agents/platforms.md) | touching Windows, Android or macOS |
| [Creating a new project](docs/agents/new-project.md) | adding a project makefile |
| [The ready-made test projects](docs/agents/test-projects.md) | verifying a change — start here rather than writing a new harness |
| [Verifying on the right target](docs/agents/cross-target.md) | changing code the host cannot build or run |
| [Toolchains & tools](docs/agents/toolchains.md) | an SDK, sysroot or toolchain question |
| [Common pitfalls](docs/agents/pitfalls.md) | asking why something does not build |
| [Code style](docs/agents/code-style.md) | writing a line of C++ |
| [Measuring a frame](docs/agents/measuring-frames.md) | quoting any number that is a time rather than a count |

The rest of the documentation is under [docs/](docs/): `api/`, `usage/`, `design/`, `articles/`
and `platforms/`.

---

## Five rules that are cheaper to read here than to rediscover

Each is stated in full in the chapter named beside it.

**Build through `xenolith-cli` when it is on `PATH`; raw `make -C <abs-path>` only as the fallback.
Never hand-roll a compiler command line to "test a build".**
→ [Golden rules](docs/agents/golden-rules.md)

**A `.cc` file is an include-only subunit and is never compiled on its own** — a module's compile
unit is its `*.scu.cpp`. The dependency on a subunit is **not tracked**, so editing only a `.cc`
leaves `make` with nothing to do and you then run the old binary. Touch a header it includes and
check that the link line actually appears. → [Code style](docs/agents/code-style.md),
[Common pitfalls](docs/agents/pitfalls.md)

**Every build is a cross-compile**, and output lands under
`$(LOCAL_OUTDIR)/$(STAPPLER_TARGET)/$(BUILD_TYPE)/cc/` — so two builds of one project differ by
directory rather than overwriting each other.
→ [The build system in one screen](docs/agents/build-system.md)

**Re-pinning a golden is a decision to record in the commit message, not a way to make a run
green.** → [The ready-made test projects](docs/agents/test-projects.md)

**Times need a release build, a quiet machine, and no polling for frames.** A debug build does not
scale timings, it reorders them; a client that polls the app thread competes with the frame it is
waiting for — measured, that stopped frames happening at all.
→ [Measuring a frame](docs/agents/measuring-frames.md)
