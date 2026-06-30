---
name: xenolith-build
description: >-
  Build & compile-verify a Xenolith/Stappler project for any target (Linux,
  Windows, Android-NDK, macOS) through the repo's make-based build system. Use
  when asked to build, compile-check, or verify a change in this monorepo —
  including "does this still compile", "build tests/window", "check the Windows
  build", or after editing runtime / stappler / xenolith sources. Picks the
  right test project and target for the code that changed, and reports exit
  status as the pass/fail signal.
---

# Building a Xenolith / Stappler project

This monorepo has **one** make-based build system. Never hand-roll a `clang`/
`cmake` command line to "test a build" — always drive `make` so flags, includes,
modules and the toolchain match. Authoritative reference: `AGENTS.md` at the repo
root (read it if anything below is unclear).

## Golden rules

- **A successful build is `exit 0`.** For CLI test apps a clean run also prints
  `N checks, 0 failures`. Always check the exit status — do not eyeball logs.
- **Every build is a cross-compile**, selected by `STAPPLER_TARGET=<triple>`.
  With no target the host triple is auto-detected.
  Pass the triple explicitly for reproducibility.
- **Use absolute `make -C <abs-path>`.** The shell cwd drifts between calls; never
  rely on `cd`.
- **`.cpp`/`.c` are compile units; `.cc` files are `#include`-only subunits.**
  Never `clang -c foo.cc` to check it — build the `.cpp` SCU that includes it.
- **A green native (Linux) build does NOT cover** `runtime/libc_impl`, Android-only,
  or macOS-only code — those are skipped on the host. Verify them on the matching
  target (see "Platform-specific code" below).

## Step 1 — pick the project to build

If the user named a project (`tests/window`, `tests/stappler`, their own dir),
use it. Otherwise pick by what changed:

| You changed… | Build this |
|---|---|
| a `stappler/` module | `tests/window` (full stack, preferred) or `tests/stappler` (faster smoke) |
| the runtime umbrella / `runtime_core` / the libc wrappers | `tests/runtime`; for the wrappers themselves also `tests/libc` |
| `runtime/libc_impl/*` or the wrapper substitute functions | `tests/libc` (host-vs-Windows diff, see Step 3) + the Windows cross-build |
| xenolith GUI / renderer / backend code | `tests/window` |

## Step 2 — build for the target

Default native debug build:

```sh
make -C <abs-proj-path> STAPPLER_TARGET=<native-triple-for-clang> -j8
```

Add `RELEASE=1` for an optimized build, `verbose=1` to debug a configure/module/
toolchain problem.

| Target | Triple | Notes |
|---|---|---|
| Linux glibc x86_64 *(reference)* | `x86_64-unknown-linux-gnu` | bare `make` is equivalent |
| Windows x86_64 | `x86_64-pc-windows-msvc` | only way to exercise `runtime/libc_impl`; run under Wine |
| Android (all ABIs) | `unknown-ndk-linux-android` | **no `-j`**, clear `MAKEFLAGS` — see below |
| macOS x86_64 / arm64 | `aarch64-apple-macosx` (`…+sprt` to skip the macOS SDK) | compile-verify when no Mac |
| Windows arm64 | `aarch64-pc-windows-msvc` | full target + sysroot, builds like x86_64; no emulator on Linux → build-verify only |

Other triples build the same full cross-compile; running needs the matching OS:
`aarch64-unknown-linux-gnu`, `riscv64-unknown-linux-gnu`,
`x86_64-unknown-linux-musl`.

### Android quirks (these are real and recurring)

```sh
env -u MAKEFLAGS -u MFLAGS make -C <abs-proj-path> STAPPLER_TARGET=unknown-ndk-linux-android
```

- **Never pass `-j`/`-jN`** — the nested `ndk-build` dies on the make-4.4 fifo
  jobserver. Clear `MAKEFLAGS` instead; ndk-build parallelizes itself.
- **"nothing to do" after an edit** → `touch` the changed `.cpp`/SCU; ndk-build
  caches objects.
- **`Android.mk.tmp: Permission denied` (exit 126)** → stale tmp from an
  interrupted/`-j` run: `rm -f <proj>/stappler-build/unknown-ndk-linux-android/Android.mk{,.tmp}`
  and rerun.
- `make -C <proj> android` is the front-end that stages the NDK project, then builds.

## Step 3 — run / verify

- **Native CLI tests** (`tests/runtime`, `tests/stappler`): run the produced
  binary and expect `exit 0` + `N checks, 0 failures`:
  ```sh
  <proj>/stappler-build/x86_64-unknown-linux-gnu/debug/cc/<exe>
  ```
- **`tests/libc`** verifies the internal libc by **diffing host vs Windows
  output** — drive it with its own script, not a plain run; success is
  `ALL TESTS IDENTICAL`:
  ```sh
  tests/libc/compare.sh            # builds host + x86_64-pc-windows-msvc, diffs (needs wine)
  tests/libc/compare.sh --host-only   # skip the Windows/wine half
  ```
- **Windows `.exe`** (from a non-Windows host, if Wine is installed):
  ```sh
  WINEDEBUG=-all wine <proj>/stappler-build/x86_64-pc-windows-msvc/debug/cc/<exe>.exe
  ```
- **GUI apps** (`tests/window` → `testapp`) **build** headless but need a display
  + Vulkan ICD to open a window. With no display, treat a clean **build** as the
  verification signal (`testapp --help` works without a GUI).
- **macOS / Android** binaries run only on the matching OS/emulator; from Linux
  these targets are compile-verification only.

Artifact layout:
`<LOCAL_OUTDIR>/<STAPPLER_TARGET>/<debug|release|coverage>/cc/<artifact>`
(`LOCAL_OUTDIR` defaults to `./stappler-build`).

## Toolchain selection (host & target halves)

The build supplies its own clang/LLD toolchain — no platform SDK needed. It
resolves the host half (per host triple) and the target half (per target triple)
by searching **two locations, in order** (`make/utils/defaults.mk`):

1. `<root>/toolchains/{hosts,targets}/<triple>/{host,target}.mk` — **externally
   installed toolchains from the binary releases** (checked first).
2. fallback: `<root>/runtime/toolchains/{hosts,targets}/<triple>/{host,target}.mk`
   — **toolchains built from source on this machine** (the in-repo
   `runtime/toolchains` build).

So an installed release shadows a local source build of the same triple; remove
or rename the `toolchains/...` copy to force the source-built one. Override the
search entirely with `STAPPLER_HOST_FILE=` / `STAPPLER_TARGET_FILE=` (an explicit
file wins over both locations). Run with `verbose=1` to print which file was
picked.

## Platform-specific code — verify on the right target

| You changed… | Verify with |
|---|---|
| `runtime/libc_impl/*` (freestanding Windows libc, `windows/*`, `builtin_*` SCUs) | win32 cross-build + Wine |
| Android-only (`runtime/window/android/*`, `*-alooper*`, JNI/unicode) | `STAPPLER_TARGET=unknown-ndk-linux-android` |
| macOS-only (`runtime/window/macos/*.mm`, darwin dispatch/clock/lock) | `STAPPLER_TARGET=x86_64-apple-macosx` |

If a rebuild says "nothing to do" after a real edit, you are either on a target
that excludes that file, or hitting a stale object cache — `touch` the SCU and
rebuild.

## Defining a project (for reference)

A project is a small `Makefile` that sets `LOCAL_*` variables and `include`s
`make/universal.mk`; it contains **no build rules**. Key knobs:
`LOCAL_EXECUTABLE`/`LOCAL_LIBRARY`, `LOCAL_MODULES` (+ `LOCAL_MODULES_PATHS`
catalogs `stappler/stappler-modules.mk`, `xenolith/xenolith-modules.mk`,
`runtime/runtime.mk`), `LOCAL_SRCS_DIRS`, `LOCAL_MAIN`, and `APPCONFIG_*` for app
identity. A typical GUI module set: `runtime xenolith_application
xenolith_renderer_basic2d xenolith_backend_vk`.
