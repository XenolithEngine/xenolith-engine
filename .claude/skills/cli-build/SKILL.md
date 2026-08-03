---
name: cli-build
description: >-
  Build Xenolith apps/projects through the `xenolith-cli` installer tool — the SDK
  command front-end. Use whenever asked to build, rebuild, run, or compile-check a
  Xenolith project or app in this repo (e.g. "build the installer", "build
  utils/installer", "rebuild the CLI", "build tests/window", "run the app",
  "does it compile"). Drives the toolchain's own make with STAPPLER_ROOT + PATH
  wired correctly — NEVER hand-roll `gmake`/`make`/`clang`/`cmake`; the CLI is
  the supported path once an SDK is installed.
---

# Building a Xenolith project via `xenolith-cli`

`xenolith-cli` is the installed SDK front-end (`/usr/local/bin/xenolith-cli`,
source at `utils/installer/cli`). It drives a project's `Makefile` through the
**toolchain's own GNU make** (the host toolchain's `bin/make`, NOT the host
OS `make`/`gmake`), with `STAPPLER_ROOT` (engine root), `PATH`, and the
toolchain-store→engine symlinks wired up. This is the only supported way to
build a project/app once the SDK is provisioned.

## Golden rules

- **Always use `xenolith-cli build`, never raw `gmake`/`make`/`clang`.** `gmake`
  only bootstraps the CLI itself; after that the CLI builds everything (it even
  builds itself — see self-build below).
- **A successful build ends with `built <path> for <triple> (<debug|release>)`.**
  Treat that line + exit 0 as the pass signal. Do not eyeball logs.
- **The engine must be present.** `xenolith-cli build` resolves `STAPPLER_ROOT`
  from: `--engine <path>` > `$XENOLITH_ENGINE` > the cloned default ref
  (`<data>/engines/<ref>`). If none is valid it errors — install/clone one first.
- **The host + target toolchains must be installed** in the shared store
  (`xenolith-cli install <triple>`, or `xenolith-cli install` to provision all).
  The CLI symlinks them into the engine tree at build time.
- **`.cpp`/`.c` are compile units; `.cc` files are `#include`-only subunits.**
  Don't try to compile a `.cc` alone — build the `.cpp` SCU that includes it.

## Build command

```
xenolith-cli build <project-path> [--engine <path>] [--target <triple>] [--release] [--run]
```

- `<project-path>` — a directory containing a `Makefile` (defaults to `.`).
- `--engine <path>` — engine root override (else resolved from env/installed ref).
  Use a local checkout (e.g. this repo) when iterating on engine code:
  `--engine /Users/vitaliyry/PET_PROJECTS/xenolith-engine`.
- `--target <triple>` — cross target (default = native host triple). Cross builds
  run `make install` (cannot `--run` on the host).
- `--release` — optimized (`-O2 -DNDEBUG`); else debug (`-g`).
- `--run` — after a native build, exec the produced binary (CLI apps: terminal;
  GUI apps: opens a window).

Output lands in `<project>/stappler-build/<target>/<debug|release>/cc/` — a bare
binary on Linux, a `.exe` on Windows, a `<name>.app` bundle on macOS.

## First time on a machine (provision the SDK)

```
xenolith-cli install                 # engine (git clone) + native host + native target (+sprt)
# or step by step:
xenolith-cli engine-install <ref>    # clone engine branch/tag (default: master; use 'stage'/'installer-cli')
xenolith-cli install <host-triple> --host
xenolith-cli install <host-triple> --target
xenolith-cli install <triple>        # one triple that is both host and target → installs both
```

Inspect state any time: `xenolith-cli detect` (native triple), `xenolith-cli list`
(catalogue + install status), `xenolith-cli paths`, `xenolith-cli state`,
`xenolith-cli verify`.

## Common build targets in this repo

| Build | Command |
|---|---|
| **GUI installer** (`xenolith-installer.app`) | `xenolith-cli build utils/installer --engine <engine>` |
| **The CLI itself** (`xenolith-cli`) | `xenolith-cli build utils/installer/cli --engine <engine>` |
| **Engine test apps** (`tests/window`, `tests/runtime`, …) | `xenolith-cli build tests/<name> --engine <engine>` |
| **A scaffolded project** | `xenolith-cli new MyApp && xenolith-cli build MyApp --run` |

After building a GUI app, launch it on macOS with `open <name>.app` (or add
`--run` to the build). Run the scene-inspector skill to debug the window.

## Self-build (the CLI builds the CLI)

The CLI is self-hosting — once bootstrapped, it rebuilds itself:

```
xenolith-cli build utils/installer/cli --engine <engine>
```

(Bootstrap from zero uses `gmake -C utils/installer/cli -j8` once; after that,
always prefer the CLI.)

## Engine iteration

To build against engine changes you're editing locally, point `--engine` at this
repo: the build reads `make/`, `stappler/`, `xenolith/` straight from there, with
the installed toolchains symlinked in. No re-clone needed.

## When NOT to use this skill

- Building a fresh toolchain/sysroot, or cross-compiling an SDK target that is
  not an app project — use the lower-level `xenolith-build` skill / `make` flow.
- The SDK isn't installed and you only need a compile check of engine sources on
  the host — a host `gmake` of a `tests/*` app is fine (see `xenolith-build`).
