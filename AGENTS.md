# AGENTS.md — Building & testing Xenolith/Stappler projects

Guidance for AI agents working in this monorepo. It tells you how to **create** a
project and how to **build and test** it for the four primary targets: Linux/glibc,
Windows, Android (NDK), and macOS.

Read this before running any build. Prefer these patterns over inventing your own
`clang`/`cmake` invocations. The engine's build machinery lives under
`make/universal.mk`, but **agents drive builds through `xenolith-cli`**
whenever it is installed — that CLI configures the SDK toolchains and then
invokes make for you. Raw `make` is only the fallback when the CLI is missing
(see §0).

Whether a freshly built binary can be **run** depends on having the matching OS:
a Linux binary runs on Linux, a Windows `.exe` runs on Windows or under Wine, an
Android `.so` runs on an Android device/emulator, a macOS `.app` runs on macOS.
Building (compile-verification) always works from any supported host; running may
require the target platform.

---

## 0. Golden rules (read first)

- **Build with `xenolith-cli` when available; `make` is the fallback.**
  Check once per session: `command -v xenolith-cli`. If it is on
  `PATH`, **always** build through it — do **not** call `make` / `gmake` /
  `xlmake` directly. Only if the CLI is absent may you fall back to absolute
  `make -C <abs-path>` (the rest of this document documents that fallback and
  the underlying make system the CLI drives).
  ```sh
  # Preferred (CLI present) — engine checkout as STAPPLER_ROOT:
  xenolith-cli build <abs-proj-path> --engine <abs-engine-root> \
      [--target <triple>] [--release] [--run]

  # Fallback ONLY when `command -v xenolith-cli` fails:
  make -C <abs-proj-path> STAPPLER_TARGET=<triple> -j8
  ```
  Inside this monorepo, pass `--engine` (or `$XENOLITH_ENGINE`) pointing at the
  engine root so the live tree is used instead of a baked snapshot. Outside the
  monorepo (scaffolded apps with an installed SDK) omit `--engine`. Never
  hand-roll a compiler command line to "test a build".
- **One build system underneath.** Every project is a small `Makefile` that sets
  `LOCAL_*` variables and `include`s `make/universal.mk`. Project makefiles
  contain **no build rules**. The CLI (or raw `make` as fallback) is what
  drives them so flags, includes, modules and the toolchain match.
- **Every build is a cross-compile.** The target is the triple
  `STAPPLER_TARGET=<arch>-<vendor>-<os>[-<env>][+sprt]` (CLI: `--target <triple>`).
  With no target the host is auto-detected from `uname` (e.g.
  `x86_64-unknown-linux-gnu`); passing the matching triple explicitly is
  equivalent and recommended for reproducibility.
- **A successful build is `exit 0`.** Check the exit status. For CLI test apps a
  clean run also prints `N checks, 0 failures`.
- **`.cpp`/`.c` are compile units (SCU); `.cc` files are `#include`-only
  subunits** and are *never* compiled standalone. Do not `clang -c foo.cc` to
  "check" it — you get false-positive errors. Build the `.cpp`/`.c` SCU that
  `#include`s it (driven by `SP_SOURCE_FILES_PATTERN`).
- **Some code is not built on the Linux host.** `runtime/libc_impl`, Android-only
  code, and macOS-only code compile to *nothing* on a native Linux build. To
  verify changes there you **must** build the matching target (see §3 and §6).
  A green native build does **not** prove those files even compiled.
- **Shell cwd can drift between calls.** Always pass **absolute** project (and
  `--engine`) paths to the CLI / `make -C`; never rely on `cd`.
- **App code cannot use POSIX sockets** (the runtime libc lacks them). Use the
  OpenSSL BIO socket API instead.
- **Before writing code, read the code style reference** —
  [docs/usage/codestyle/index.adoc](docs/usage/codestyle/index.adoc) (summary in
  §9 below). This document covers building; that one covers how the sources are
  written.

---

## 1. The build system in one screen

```
your Makefile
  └── include make/universal.mk          entry point, host/target dispatch
        ├── make/host.mk                  native / cross (clang) build rules
        ├── make/android-ndk.mk           Android NDK (ndk-build) rules
        └── make/general/compile.mk       the machinery (flags, modules, shaders, …)
```

`make/universal.mk` behaves two ways:

- **No `STAPPLER_TARGET` and no `STAPPLER_BUILD`** → it is a *front-end*: it
  re-invokes itself with defaults and exposes convenience goals (`host`,
  `host-release`, `android`, `clean`, …).
- **`STAPPLER_TARGET` set** → it does the real build directly (default goal
  `all`).

### Front-end goals (run from a project dir, no target set)

| Goal | Meaning |
|---|---|
| `host` *(default)* | native debug build |
| `host-release` | native optimized build (`-O3 -DNDEBUG`) |
| `host-debug-clean` / `host-release-clean` | clean that build |
| `host-coverage` / `host-report` | coverage build / lcov HTML report |
| `host-install` | build + run the `install` step |
| `android` / `android-release` | export + build **all** Android ABIs via the NDK |
| `android-export` | only stage the NDK project (Android.mk, shaders, config) |
| `clean` / `install` | aliases for `host-debug-clean` / `host-install` |

### Command-line switches

| Switch | Effect |
|---|---|
| `STAPPLER_TARGET=<triple>` | build for that target instead of the host |
| `RELEASE=1` | optimized build (else debug: `-g -DDEBUG`) |
| `COVERAGE=1` | coverage instrumentation |
| `verbose=1` | print every command + toolchain/module trace (use this to debug config) |
| `-jN` | parallel build (**except Android-NDK** — see §3.3) |
| `ASAN=1` | AddressSanitizer (supported by the test makefiles) |
| `STAPPLER_BUILD_ROOT=` | path to `make/` (defaulted relative to the makefile) |
| `STAPPLER_HOST_FILE=` / `STAPPLER_TARGET_FILE=` | override toolchain selection |

### Output layout

```
$(LOCAL_OUTDIR)/$(STAPPLER_TARGET)/$(BUILD_TYPE)/cc/<artifact>
# LOCAL_OUTDIR defaults to ./stappler-build ; BUILD_TYPE ∈ {debug,release,coverage}
```

- Linux: bare executable — `…/x86_64-unknown-linux-gnu/debug/cc/testapp`
- Windows: `…/x86_64-pc-windows-msvc/debug/cc/testapp.exe` (+ `.pdb`)
- macOS: a bundle — `…/aarch64-apple-macosx/debug/cc/testapp.app/Contents/MacOS/testapp` (+ `.dSYM`)
- Android-NDK: per-ABI `.so` — `…/unknown-ndk-linux-android/debug/ndk-build/obj/local/<abi>/lib<name>.so`

`compile_commands.json` is written to the repo root after a build (for clangd).

---

## 2. Quick reference: build + run/verify per target

Replace `<engine>` with the absolute path to this repo and `tests/window` with
your project. Success = `exit 0`. **Use the CLI column when
`xenolith-cli` is installed**; the `make` column is fallback-only (§0).

| Target | Triple | Build (CLI — preferred) | Build (`make` fallback) | Run | Notes |
|---|---|---|---|---|---|
| **Linux glibc x86_64** *(reference)* | `x86_64-unknown-linux-gnu` | `xenolith-cli build tests/window --engine <engine> --target x86_64-unknown-linux-gnu` | `make -C tests/window STAPPLER_TARGET=x86_64-unknown-linux-gnu -j8` | run the binary on Linux | omit `--target` for native host; CLI apps self-verify, GUI apps need display + Vulkan (§3.1) |
| **Windows x86_64** | `x86_64-pc-windows-msvc` | `xenolith-cli build tests/window --engine <engine> --target x86_64-pc-windows-msvc` | `make -C tests/window STAPPLER_TARGET=x86_64-pc-windows-msvc -j8` | on Windows, or under Wine | only way to exercise `runtime/libc_impl` (§6) |
| **Android (all ABIs)** | `unknown-ndk-linux-android` | `xenolith-cli build tests/window --engine <engine> --target unknown-ndk-linux-android -j1` | `env -u MAKEFLAGS -u MFLAGS make -C tests/window STAPPLER_TARGET=unknown-ndk-linux-android` | on an Android device/emulator | **no nested `-j`**, clear `MAKEFLAGS` if using make fallback; full APK via Gradle; see §3.3 |
| **macOS x86_64 / arm64** | `aarch64-apple-macosx` *(or `x86_64-…`, `…+sprt`)* | `xenolith-cli build tests/window --engine <engine> --target aarch64-apple-macosx` | `make -C tests/window STAPPLER_TARGET=aarch64-apple-macosx -j8` | on macOS | compile/build verification when no Mac is available |

Add `--release` / `RELEASE=1` for optimized builds; add `--run` on the CLI to
launch a native artifact after a successful build.

Additional target triples build the same way (full cross-compile); running them
requires the matching OS/emulator: `aarch64-unknown-linux-gnu`,
`riscv64-unknown-linux-gnu`, `x86_64-unknown-linux-musl`,
`aarch64-pc-windows-msvc` (now a full target with its own sysroot — see §3.2;
no emulator on an x86_64 Linux host, so build-verify only there).

---

## 3. Per-platform detail

### 3.1 Linux / glibc (reference platform)

- Triple `x86_64-unknown-linux-gnu` (or `aarch64-unknown-linux-gnu`). This is also
  the auto-detected host triple, so omitting `--target` / bare `make` builds the
  host equivalently.
- Build with the explicit triple (recommended):
  ```sh
  # preferred:
  xenolith-cli build tests/runtime --engine <abs-engine-root> \
      --target x86_64-unknown-linux-gnu    # add --release for optimized
  # fallback if CLI missing:
  make -C tests/runtime STAPPLER_TARGET=x86_64-unknown-linux-gnu -j8
  ```
- Run the produced binary (path includes the triple and build type):
  ```sh
  tests/runtime/stappler-build/x86_64-unknown-linux-gnu/debug/cc/runtimetest
  # expect exit 0 and "N checks, 0 failures"
  ```
- A Linux binary runs on a Linux system with the matching libc (glibc for this
  triple; for `…-linux-musl` you need a musl system).
- **CLI test apps** (`tests/runtime`, `tests/stappler`) run and self-check
  directly — the primary way to verify on Linux. `tests/libc` also runs on the
  host, but verifies by **diffing host vs Windows output** (see §5), not by a
  `0 failures` self-check.
- **Graphical apps** (`tests/window` → `testapp`) **build** without a display but
  to actually open a window need a display server (XCB/Wayland) and a Vulkan ICD.
  `…/testapp --help` works without a GUI; a full run needs a graphical session.
  In a headless environment, treat a clean build as the verification signal.
- **Rendering is on demand.** With nothing dirty the engine stops producing
  frames, so a window nobody is touching freezes: scheduled actions do not tick,
  timed phases never fire, and a state change made off-screen is only laid out
  once something (a mouse move over the window) wakes the loop. **Anything meant
  to verify reactivity or interactivity has to hold the loop open itself** —
  `runAction(Rc<RenderContinuously>::create())` (`XLAction.h`) draws nothing and
  damages nothing, it only keeps the frames coming. `tests/window` does this for
  every test in `TestLayout::init()`; an app of your own does not. See the
  `gui-debug` skill.

### 3.2 Windows (cross-compiled; runs on Windows or under Wine)

- Triple `x86_64-pc-windows-msvc`. Cross-built with clang + LLD; **does not use
  MSVCRT/UCRT or the Windows SDK** — it links the engine's own freestanding libc
  (`runtime/libc_impl`, mimalloc) and binds to system DLLs via a vendored
  `import.lib`.
- Build (CLI preferred; `make` only if CLI missing):
  ```sh
  xenolith-cli build <proj> --engine <abs-engine-root> --target x86_64-pc-windows-msvc
  # fallback: make -C <proj> STAPPLER_TARGET=x86_64-pc-windows-msvc -j8
  ```
- Artifact: `…/x86_64-pc-windows-msvc/debug/cc/<exe>.exe` (+ `.pdb`).
- Run/verify: on Windows directly, or — from a non-Windows host — under Wine if it
  is installed:
  ```sh
  WINEDEBUG=-all wine tests/runtime/stappler-build/x86_64-pc-windows-msvc/debug/cc/runtimetest.exe
  ```
  A clean CLI-test-app run is `exit 0` + `N checks, 0 failures`.
- **This is the only way to exercise `runtime/libc_impl`** — those sources are
  skipped entirely by the Linux host build (§6). The dedicated harness for it is
  `tests/libc` (§5), which builds the same sources for the host and for this
  Windows target and **diffs the two outputs** for behavioural identity. On
  Windows `wchar_t` is 16-bit (`== char16_t`), so surrogate-pair code paths only
  get exercised on this target.
- **arm64 Windows** (`aarch64-pc-windows-msvc`) is now a **full target with its
  own sysroot** (and a supported host) — build it like x86_64:
  ```sh
  xenolith-cli build <proj> --engine <abs-engine-root> --target aarch64-pc-windows-msvc
  # fallback: make -C <proj> STAPPLER_TARGET=aarch64-pc-windows-msvc -j8
  ```
  There is still **no emulator** to *run* arm64-Windows binaries on an x86_64
  Linux host (the Wine flow does not apply to execution), so from Linux this is
  build/compile verification only. For a quick header/SCU check **without** a full
  build, you can still compile-only with host clang — compile the `.cpp` SCUs (not
  `.cc`):
  ```sh
  clang --target=aarch64-pc-windows-msvc -std=c++20 -ffreestanding -nostdinc -fexceptions \
    -D__SPRT_WIN_USE_IMPORT_LIB=0 -D_WIN32_WINNT=0x0A00 \
    -Iruntime/include -Iruntime/include_libc -Iruntime/libc_impl/include \
    -c runtime/libc_impl/src/<unit>.cpp -o /tmp/x.o
  ```
  `-D__SPRT_WIN_USE_IMPORT_LIB=0` is required (no arm64 import libs). Diff against
  `--target=x86_64-pc-windows-msvc` to catch shared-header regressions.

### 3.3 Android (NDK)

- Triple `unknown-ndk-linux-android`; builds **all four ABIs** (`armeabi-v7a`,
  `arm64-v8a`, `x86`, `x86_64`) into `lib<name>.so` via `ndk-build`.
- **Build Android via CLI with `-j1`** (nested `ndk-build` hates a parent
  jobserver). Make fallback still works if the CLI is missing:
  ```sh
  xenolith-cli build tests/window --engine <abs-engine-root> \
      --target unknown-ndk-linux-android -j1
  # fallback: env -u MAKEFLAGS -u MFLAGS make -C tests/window STAPPLER_TARGET=unknown-ndk-linux-android
  ```
  This compiles every ABI. It expects the NDK project to have been staged once by
  the `android-export` goal (which `tests/window` wires into its build); if it has
  not, run `android-export` first (next bullet) or use the `make android`
  front-end below, which does both.
- **Front-end convenience goal** (stages the NDK project via `android-export`,
  then builds): `make -C tests/window android` (or `android-release`). `make
  -C tests/window android-export` only stages the project (Android.mk, shaders,
  generated config) without compiling.
- Artifacts: `…/unknown-ndk-linux-android/debug/ndk-build/obj/local/<abi>/lib<name>.so`.
  Run by installing the produced APK on an Android device/emulator (the APK is
  assembled by Gradle / Android Studio from `proj.android/`, not by `make`).
- **Gotchas (these are real and recurring):**
  - **Do not pass `-j`/`-jN`.** The nested `ndk-build` dies with
    `invalid --jobserver-auth string 'fifo:...'`. Clear the jobserver instead:
    `MAKEFLAGS=` / `env -u MAKEFLAGS -u MFLAGS …`; let ndk-build parallelize itself.
  - **"nothing to do" after an edit:** ndk-build caches objects under
    `…/unknown-ndk-linux-android/debug/ndk-build/obj`. `touch` the changed
    `.cpp`/SCU to force a recompile.
  - **`Android.mk.tmp: Permission denied` (exit 126) at `android-export`:** a
    stale tmp from an interrupted/`-j` run. Fix:
    `rm -f <proj>/stappler-build/unknown-ndk-linux-android/Android.mk{,.tmp}` and rerun.
  - The build resolves the NDK from `$ANDROID_NDK_ROOT`, falling back to
    `~/Android/Ndk`.
- A project enables Android via `LOCAL_ANDROID_MK`, `LOCAL_APPLICATION_MK`,
  `LOCAL_ANDROID_TARGET`, `LOCAL_ANDROID_PLATFORM` (default `android-24`) and a
  `prepare-android`/`android-export` rule that rsyncs `resources/` into the APK
  assets. The Gradle/Android Studio project lives under `proj.android/`.

### 3.4 macOS (cross-compiled; runs only on macOS)

- Triples `x86_64-apple-macosx`, `aarch64-apple-macosx`, optionally `+sprt`
  (`aarch64-apple-macosx+sprt`) to use the toolchain with the integrated Xenolith
  Runtime (then the macOS SDK is not needed).
- Build (CLI preferred; `make` only if CLI missing):
  ```sh
  xenolith-cli build <proj> --engine <abs-engine-root> --target aarch64-apple-macosx
  # fallback: make -C <proj> STAPPLER_TARGET=aarch64-apple-macosx -j8
  ```
- Artifact: a `name.app` bundle (`…/cc/<name>.app/Contents/MacOS/<name>`, with a
  `.dSYM` for debug builds).
- **macOS binaries run only on macOS.** From a non-macOS host this target is
  build / compile verification only — to actually run, copy the bundle to a Mac or
  use the Xcode project in `proj.macos/`.
- Use this target to verify macOS-only sources (window/macos `.mm`, darwin
  dispatch/clock/lock) actually compile (§6). On macOS, `errno`/exceptions
  behavior differs from Linux — do not assume Linux values.
- License note: before using Apple targets **without** `+sprt`, review the macOS
  SDK license (Apple-hardware restrictions).

---

## 4. Creating a new project

### 4.1 Layout

```
myproject/
  Makefile
  main.cpp            # the translation unit with main()
  src/...             # other sources (compiled recursively)
  shaders/...         # optional GLSL → SPIR-V
  resources/...       # optional bundled assets
  proj.android/       # optional, for the Android/Gradle build
  proj.macos/         # optional, for Xcode
```

### 4.2 Makefile template

```make
# Rebuild everything if this makefile changes
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# Path to make/ (relative to this makefile). ../../make for a tests/* layout.
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# What to build: one of LOCAL_EXECUTABLE / LOCAL_LIBRARY / LOCAL_WASM_MODULE
LOCAL_EXECUTABLE := myapp

# Module catalogs to read, then the modules to use (deps resolved transitively)
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	runtime \
	xenolith_application \
	xenolith_renderer_ui \
	xenolith_backend_vk

# Sources
LOCAL_MAIN          := main.cpp     # TU with main(), compiled separately
LOCAL_SRCS_DIRS     := src          # scanned recursively for .c/.cpp/.mm
LOCAL_INCLUDES_OBJS := src          # added to the include path
LOCAL_SHADERS_DIR   := shaders      # optional

# App identity (baked into the binary / bundle)
APPCONFIG_APP_NAME    := MyApp
APPCONFIG_BUNDLE_NAME := org.stappler.MyApp

include $(STAPPLER_BUILD_ROOT)/universal.mk
```

Then build with the CLI (preferred) or `make` fallback (§0):
```sh
xenolith-cli build myproject --engine <abs-engine-root>   # add --release / --target …
# fallback: make -C myproject -j8
```

### 4.3 Key `LOCAL_*` variables

| Variable | Meaning |
|---|---|
| `LOCAL_EXECUTABLE` | build an executable with this name |
| `LOCAL_LIBRARY` / `LOCAL_VERSION` | build a library (versioned). `LOCAL_BUILD_STATIC` / `LOCAL_BUILD_SHARED` (default `1`/`1`) choose archive/.so; `LOCAL_BUILD_SHARED=2/3` for standalone/live-reload module |
| `LOCAL_WASM_MODULE` | build a WebAssembly component |
| `LOCAL_MAIN` | TU holding `main()` (compiled apart, so sources are reusable for tests) |
| `LOCAL_MODULES` / `LOCAL_MODULES_OPTIONAL` | direct module deps (transitive resolved) / include-if-present |
| `LOCAL_MODULES_PATHS` | the `*-modules.mk` catalogs that define modules |
| `LOCAL_SRCS_DIRS` / `LOCAL_SRCS_OBJS` | source dirs (recursive) / individual sources |
| `LOCAL_INCLUDES_DIRS` / `LOCAL_INCLUDES_OBJS` | header dirs (recursive) / direct include dirs |
| `LOCAL_SHADERS_DIR` / `LOCAL_SHADERS_INCLUDE` | GLSL dirs / shader include dirs |
| `LOCAL_CFLAGS` / `LOCAL_CXXFLAGS` / `LOCAL_LDFLAGS` / `LOCAL_LIBS` | extra flags / libs (`-lfoo`, `-l:libfoo.a`) |
| `LOCAL_PRIVATE_INCLUDE_PCH` | precompiled header(s), e.g. `SPCommon.h` |
| `LOCAL_OUTDIR` / `LOCAL_INSTALL_DIR` | build output root / install destination |

### 4.4 `APPCONFIG_*` (executables get a generated config header)

| Variable | Meaning |
|---|---|
| `APPCONFIG_APP_NAME` | human-readable name (default = `LOCAL_EXECUTABLE`) |
| `APPCONFIG_BUNDLE_NAME` | reverse-DNS id, e.g. `org.stappler.MyApp` (used for macOS bundle id, Windows AppContainer name) |
| `APPCONFIG_APP_PATH_COMMON` | resource/sandbox mode. **Don't set on macOS** — non-default values break app activation and the window never maps (leave it unset, like `tests/window`). Linux: `>0` = use XDG dirs, `0` = self-contained next to exe. Windows: `1` AppData, `2` AppContainer paths, `3` run inside an AppContainer |
| `APPCONFIG_VERSION_API` / `_REV` / `_BUILD` / `_VARIANT` | version components |
| `APPCONFIG_STRINGS` / `APPCONFIG_VALUES` | extra string / numeric defines |

### 4.5 Available modules

You list only direct deps; the resolver walks `_DEPENDS_ON` transitively and
aborts with `Module not found: <name>`. The final ordered set prints at configure
time as `Enabled modules: …`.

- **runtime** (`runtime/runtime.mk`): `runtime` (the umbrella — what apps use),
  `runtime_core`, `runtime_libc_wrapper`, `runtime_libc_impl`, `runtime_musl_libc`,
  `runtime_malloc`, `runtime_window`.
- **stappler** (`stappler/stappler-modules.mk`): `stappler_core`, `stappler_data`,
  `stappler_filesystem`, `stappler_bitmap`, `stappler_crypto`, `stappler_db`,
  `stappler_sql`, `stappler_search`, `stappler_network`, `stappler_font`,
  `stappler_vg`, `stappler_tess`, `stappler_zip`, `stappler_wasm`,
  `stappler_makefile`, `stappler_pug`, `stappler_document`, `stappler_layout`,
  `stappler_brotli_lib`.
- **xenolith** (`xenolith/xenolith-modules.mk`): `xenolith_core`,
  `xenolith_application`, `xenolith_backend_vk`, `xenolith_font`,
  `xenolith_renderer_basic2d` (+ `_shaders`), `xenolith_renderer_ui`,
  `xenolith_renderer_pug`, `xenolith_renderer_richtext`,
  `xenolith_resources_assets`, `xenolith_resources_network`,
  `xenolith_resources_storage`, `xenolith_remote`.

Typical sets: a GUI app → `runtime xenolith_application xenolith_renderer_basic2d
xenolith_backend_vk`; a non-graphical Stappler app → `runtime stappler_core` plus
what you need.

---

## 5. The ready-made test projects (use these to verify changes)

| Project | Artifact | Modules / what it exercises | Kind |
|---|---|---|---|
| `tests/runtime` | `runtimetest` | `runtime_libc_wrapper` + `runtime` — the Xenolith Runtime (libc/STL/pthread) | CLI, self-checking |
| `tests/libc` | `libctest` | the internal libc implementation — `runtime/libc_impl` **and** the `runtime_libc_wrapper` wrappers (including the substitute/replacement functions the wrappers supply when a function is missing on the platform). Built for the host **and** `x86_64-pc-windows-msvc`; `compare.sh` diffs the two for behavioural identity | CLI, host-vs-Windows diff |
| `tests/stappler` | `stapplertest` | the `stappler_*` app modules (core/data/bitmap/crypto/db/document/font/vg/pug/makefile/layout/network) — **fast smoke build** | CLI |
| `tests/window` | `testapp` | full xenolith GUI stack (`xenolith_application` + `renderer_ui` + `backend_vk` + `resources_assets`); transitively compiles the stappler modules | GUI |

**Which to use:**
- Changed a `stappler/` module → build `tests/window` (preferred — full stack) or
  `tests/stappler` (faster smoke). Drive either through the CLI (§0 / §2).
- Changed the runtime (`runtime`/`runtime_core`/wrapper) → `tests/runtime`; for
  the libc wrappers themselves also run `tests/libc`.
- Changed `runtime/libc_impl` (or the libc wrappers) → `tests/libc` (its
  `compare.sh` cross-builds for Windows and diffs against the host) **or**
  `tests/runtime`; either way the Windows cross-build is what actually compiles
  `libc_impl` (§3.2, §6).

A clean CLI-test-app verify (native):
```sh
xenolith-cli build tests/runtime --engine <abs-engine-root> \
  && tests/runtime/stappler-build/x86_64-unknown-linux-gnu/debug/cc/runtimetest
# fallback if CLI missing: make -C tests/runtime -j8 && <same binary>
# expect exit 0 and "N checks, 0 failures"
```

---

## 6. "Not built on the host" — verify platform code on the right target

A native Linux build silently skips code for other platforms. To prove such code
even compiles, build its target. Match the change to the verification:

| You changed… | Verify with |
|---|---|
| `runtime/libc_impl/*` (freestanding Windows libc, `windows/*`, `builtin_*` SCUs) or the `runtime_libc_wrapper` substitute functions | **`tests/libc` + win32 cross-build + Wine** (§3.2, §5). Linux build links `runtime_libc_wrapper` instead and touches none of `libc_impl`. |
| Android-only code (`runtime/window/android/*`, dispatch `*-alooper*`, JNI/unicode) | **Android NDK target** (§3.3): `STAPPLER_TARGET=unknown-ndk-linux-android` |
| macOS-only code (`runtime/window/macos/*.mm`, darwin dispatch/clock/lock) | **macOS cross-compile** (§3.4): `STAPPLER_TARGET=x86_64-apple-macosx` (compile-verify when no Mac is available) |
| arm64 Windows code / shared headers | **full cross-build** (§3.2): `STAPPLER_TARGET=aarch64-pc-windows-msvc` (build-verify only — no emulator on Linux). For a quick header/SCU check, host clang `--target=aarch64-pc-windows-msvc` compile-only. |
| Linux/glibc, the runtime umbrella, stappler/xenolith app code | native build + run the relevant CLI test (§5) |

If a rebuild reports "nothing to do" after you edited a file that *should* be in
scope, you are probably either (a) on a target that excludes that file, or
(b) hitting a stale object cache — `touch` the SCU and rebuild.

---

## 7. Toolchains & tools

- **GNU Make 4.1+** is required. macOS ships an older `make` — use `gmake` there.
- The SDK ships its **own clang/LLD/lldb toolchain**, selected automatically per
  host triple (the host half) and per target triple (the target half). The build
  resolves each half by searching **two locations in order** (see
  `make/utils/defaults.mk`):
  1. `<root>/toolchains/hosts/<host>/host.mk` and
     `<root>/toolchains/targets/<target>/target.mk` — **externally installed
     toolchains from the binary releases** (checked first).
  2. if not found there, `<root>/runtime/toolchains/hosts/<host>/host.mk` and
     `<root>/runtime/toolchains/targets/<target>/target.mk` — **toolchains built
     from source on this machine** (the in-repo `runtime/toolchains` build).

  A release install therefore takes precedence over a local source build of the
  same triple; remove/rename the `toolchains/...` copy to force the source build.
  No platform SDK is needed (Windows without UCRT/Windows SDK; macOS without the
  macOS SDK when using a `+sprt` target). Override the whole search with
  `STAPPLER_HOST_FILE=` / `STAPPLER_TARGET_FILE=` (an explicit file wins over both
  locations).
- **`xlmake`** (`utils/xlmake/`) is the project's GNU-make-compatible build driver
  (also a drop-in for the VSCode Makefile Tools extension); the `make` invocations
  in this document work with either.
- To **run** a binary that is not native to the build machine you need the
  matching runner installed: **Wine** for a Windows `.exe`, an **Android
  device/emulator** (plus Gradle to assemble the APK) for Android, and **macOS**
  for a `.app` bundle. Building (compile-verification) never requires these.

---

## 8. Common pitfalls checklist

- [ ] Built via `xenolith-cli` when it is on `PATH` (raw `make` only as
      fallback — §0). Inside this monorepo, passed `--engine <abs-engine-root>`.
- [ ] Used absolute project / `--engine` paths (cwd drifts between calls).
- [ ] Did **not** try to compile a `.cc` subunit standalone (build its `.cpp` SCU).
- [ ] For Android: no `-j`, cleared `MAKEFLAGS`, `touch`ed edited sources, cleared
      a stale `Android.mk.tmp` if export failed with exit 126.
- [ ] To verify Windows/Android/macOS code, built the **matching target** — a
      green native build does not cover them.
- [ ] Treated a clean **build** as the signal when no runner is available
      (GUI/macOS/Android); ran the **binary** for native CLI tests and via Wine.
- [ ] Ran with `verbose=1` when a configure/module/toolchain problem is unclear.

---

## 9. Code style

Full reference: **[docs/usage/codestyle/index.adoc](docs/usage/codestyle/index.adoc)**
— one article per topic. Read the relevant article before creating a file, a
header, a platform branch, or an allocation. The essentials:

- `.cpp`/`.c` = compile units (SCU); `.cc` = `#include`-only subunits
  ([units](docs/usage/codestyle/sources/units-and-files.adoc)).
- MIT license block, path-derived include guard (`XENOLITH_..._H_`, no `#pragma
  once`), `namespace STAPPLER_VERSIONIZED stappler::…` (runtime: `namespace
  sprt`), `SP_PUBLIC` / `SPRT_API` on exported entities, includes never sorted
  ([file layout](docs/usage/codestyle/sources/file-layout.adoc)).
- Types `PascalCase`, functions `camelCase`, members `_camelCase`, file statics
  `s_name`; virtual hooks are `handleXxx()`, not `onXxx()`; files are `SP*` /
  `XL*` / lowercase-in-`sprt`, aggregators `*.scu.cpp`
  ([naming](docs/usage/codestyle/sources/naming.adoc)).
- Platform tests are `#if SPRT_WINDOWS` / `SPRT_APPLE` / … — `#if`, not `#ifdef`,
  never raw `_WIN32`; arch via `__SPRT_ARCH_ID == __SPRT_ARCH_ID_*`
  ([platform guards](docs/usage/codestyle/platform/platform-guards.adoc)).
- Ref-counted objects: `Rc<T>::create()` + `virtual bool init(...)`, never bare
  `new`. Pool-allocated types must derive from `AllocPool`; `new (pool) T` on
  anything else is a silent corruption, and aggregate `Type{value}` initializes
  the base class ([memory](docs/usage/codestyle/core/memory-and-ownership.adoc)).
- Layout is [.clang-format](.clang-format)'s job: tabs (4), continuation 8,
  column limit 100, `Node *node`, attached braces
  ([formatting](docs/usage/codestyle/sources/formatting.adoc)).
- `data::Value` is the boundary type (config, files, IPC, command line):
  `setValue(value, key)` takes the **value first**, a failed lookup returns the
  shared `Value::Null` — read-only memory, so assigning through it asserts in
  debug and faults if it gets past the guards — an indexed write past the end of
  an array takes the next free slot, `Value{5}` is the array `[5]`, and bytes
  survive CBOR but not JSON
  ([data::Value](docs/usage/codestyle/core/data-value.adoc), full guide:
  [docs/usage/data/value.adoc](docs/usage/data/value.adoc)).
- A key combination is a **named global hotkey**: register it once
  (`HotkeyRegistry::add("org.example.app.save", HotkeyCombo::parse("Ctrl+S"))`)
  and subscribe with `listener->addHotkey(id, cb, flags)`. Delivery is in
  `InputDispatcher`, **ahead of the ordinary key route**, so a subscriber needs no
  key mask and is **not hit-tested against the pointer**; return `true` to consume,
  `false` to pass it on. The keyboard owner is offered it first, then the normal
  walk order. `FocusedOnly` means "entitled to keys in this focus group", not
  `isFocused()`. A hotkey is the press only — something shown *while a key is held*
  still wants a recognizer. A combination may demand one side of a modifier
  (`"CtrlL+K"`), which every backend but wasm reports; and
  `HotkeyOptions::ReserveFromTextInput` makes the text-input processor decline it,
  so an `Alt`/`Super` chord survives a focused field — opt-in, because `Escape` and
  `Backspace` belong to the IME ([hotkeys](docs/usage/codestyle/scene/hotkeys.adoc)).
- A directory named in `LOCAL_EMBED_DIRS` / `MODULE_<X>_EMBED_DIRS` is compiled
  into the binary (BundleFS) and read back through **`FileCategory::Embedded`**,
  where the bundle's mount name is the directory's own name. `Embedded` is
  independent of `Bundled` (the on-disk app bundle) and is strictly read-only;
  compression is per-bundle, needs `stappler_data`, and only `xlmake` performs it
  ([embedded files](docs/usage/codestyle/core/embedded-files.adoc)).
- Windows are asked for with `Context::createWindow`, and what a window **is**
  travels with the request as `WindowInfo::appData` — never look one up by `id`,
  which the runtime may re-unique. Popups/dialogs/tooltips are `ui::SubWindow`
  (native subwindow or in-scene overlay); check `WindowCapabilities` before
  offering fullscreen or decoration controls. A scene reads where its window is
  with `getRenderServer()->getWindowGeometry()` (a content rect in the **logical**
  units `WindowInfo::rect` takes, so it can be handed straight back) and is told
  about changes by `Scene::handleWindowGeometryChanged`; always check
  `hasPosition` before saving an origin, and set
  `WindowCreationFlags::UsePosition` to ask for one back
  ([windows](docs/usage/codestyle/window/windows.adoc)).
- OS dialogs are an `Rc<sprt::window::DialogRequest>` you **keep** — it is the
  cancellation token; the callback is required, runs exactly once, and
  `Status::Declined` is the user cancelling, not a failure
  ([dialogs](docs/usage/codestyle/window/dialogs.adoc)).
- The clipboard is `Rc<ClipboardSession>` over the app thread, never the three
  `AppThread` calls directly. A payload is a `ClipboardOffer` — MIME types **in
  order of preference** plus eager or lazy bytes, the same object a drag carries;
  a read states a **preference list** matched by PREFIX and is answered **exactly
  once**, which the platforms do not do on their own (wayland drops an unoffered
  type in silence, the base controller answers twice). **`cancel()` when the
  reason for the read goes away** — a widget losing focus does it in two places,
  because the platform usually revokes input rather than going through `blur()`.
  An empty offer is refused (on Android that means *clear the clipboard*), a
  write is never a receipt, and policy — a masked field refusing to copy — stays
  with the widget ([clipboard](docs/usage/codestyle/window/clipboard.adoc)).
- Undo for text is `ui::TextHistory`, over `hist::CommandBus` from
  `SPCommandHistory.h`. It records at the ONE point where text changes — the IME
  **echo**, not a widget command, because the runtime's processor owns printable
  keys and a typed character never reaches `insertText`. It is **on** for
  `ui::TextView`/`ui::CodeEditor` and **off** for a plain `ui::TextInput`: a field
  commits into somebody's document, and `Ctrl+Z` there must take back the document
  edit, not the typing. A run of keystrokes is one entry until its idle window
  passes (`breakRun()` ends one on demand); a paste, a cut and a newline are each
  their own. A handler with nothing to undo answers **false**, so the chord reaches
  whoever is below — that is how an application arbitrates two histories.
- A form is one `ui::FormSystem` on the node it is rooted at, and that system
  **is** the focus group; fields are attached to the widgets (`ui::addFormField`,
  `addFormButton`, or `FormFieldSlots` for a widget of your own) and join the
  nearest form above them. The field name is the node's name (= its CSS id), a
  focus change is committed only on the next frame (`getPendingField()` is what
  was asked for), and the tab ring is document order — so give siblings distinct
  `ZOrder` ([forms](docs/usage/codestyle/ui/forms.adoc)).
- Input atoms: `ui::TextInput` (text), `ui::NumberField` (a number — the range is
  **declared**, and typing past it is refused while dragging past it is clamped),
  `ui::Select` (a drop-down: a closed `Panel` plus a real menu surface, so the
  list's keyboard is `MenuSystem`'s and the closed control's arrows are its own),
  `ui::VectorField` (a row of `NumberField`s that is ONE form field — Tab walks
  its components and leaves only at the ends), `ui::ColorField` (a swatch and a
  hex line; the system colour dialog behind `isDialogSupported`, and a picker of
  its own where there is none), `ui::Chip` / `ui::ChipRow` (a row of chips that is
  ONE form field collecting an ARRAY of ids — the limit and the uniqueness are
  **declared**, so at the maximum the "+" is dead rather than refusing after the
  press, duplicates are allowed by default because an element chain repeats, and
  the wrapped height is reported through the measurement protocol),
  `ui::Checkbox`, `ui::Button`. All of them take their whole look from CSS through
  a per-type applier, and a refusal is the style class `invalid` — there is no
  `:invalid` pseudo-class in the subset.
- **A control's states are independent classes, and two of them carry a reason.** `invalid` is "what is written here is wrong"; `disabled` is the
  mechanical off, and tracks `:disabled`; `read-only` is readable and copyable but
  not editable; `locked` is "you may not write here at all, and here is why" —
  `ui::setEditLock(node, reason)` paints it, clears the `Enabled` bit, hangs the
  reason off a `ui::TooltipTarget` (only if the node has no hint of its own) and
  takes the field out of the form's tab ring. A locked control is also disabled,
  and the two compose: unlocking restores what the application last asked for, not
  "on". `ui::applyControlEnabled` is the **single** writer of the `Enabled` bit and
  of the `disabled` class — do not flip either by hand. `unavailable` is the fifth
  and narrowest: an ACTION the control offers cannot be performed (no system colour
  dialog on this platform, a dialog that failed). It is not `invalid` — the value is
  fine, the way in is missing — and a capability refusal must never borrow the
  validation channel.
- **A number can name its unit**, and a unit is a LABEL: `ui::NumberField::setUnit`
  and `ui::VectorField::setUnit` (one per row, not one per component) draw a word
  beside the number and inset the text viewport to make room. Nothing converts or
  validates against it, and it never enters `getText()` — `parse(format(v)) == v`
  has to keep holding.
- **A list of names is not a `MenuSource`.** `ui::Select` and `ui::ChipRow` take
  data (`SelectOption`); `ui::makeSelectOptions` builds that list from plain
  strings for the id==title case, in both the `StringView` and the `String`
  spelling because a `SpanView<StringView>` cannot be made from a `Vector<String>`.
- Two widgets are surfaces rather than atoms, and both exist because a list is
  the wrong shape for what they do. `ui::SearchPicker` is a query line over a
  virtualized result list (`ui::SearchSystem` + a `SearchSource`): a list of
  hundreds is not a menu, so the query line keeps focus and the arrows move a
  selection somewhere else. `ui::InlineEditor` edits over a **rectangle** rather
  than inside a node (`beginInlineEdit` / `beginInlineTextEdit`, or a
  `ui::InlineEditTarget` on the node itself): a virtualized row is destroyed by
  scrolling and by `invalidateSource()`, and a `ui::TextInput` holds the IME, so
  an editor parented into the cell would lose the typed text to a rebuild nobody
  asked for. It ends on Enter, on a press outside, on a scroll and on the anchor
  leaving the scene - all of them COMMITTING, and only Escape cancelling - and
  the commit is delivered at most once however many of those arrive together.
  A caller that supplies its OWN editor through `setFactory` must also supply
  `setCollectCallback`: this side is handed a node it cannot interpret, so
  without it the commit carries a Nil - and it did, silently, until the studio's
  control binder became the first caller to take that path. `collect` stays
  optional because a display has nothing to report, which is exactly what made
  the hole invisible.
- `ui::TableView` publishes its ROW GEOMETRY - `getRowRect`, `getCellRect`,
  `getRowIndexAt`, `getRowBoundaryAt`, shared with `ui::TreeView` through
  `ui::RowGeometrySource` - and it answers for a row that has no node, because
  only the nodes are virtualized: `rebuildRows()` commits one controller item per
  row with the height it resolved beforehand. Reordering builds on that:
  `setReorderCallback` asks `bool(from, to)` where `to` is the row's FINAL index,
  a false REFUSES and moves neither the order nor the selection, and on
  acceptance the selection follows the ROW rather than the index. The grip is a
  column the CALLER declares under `TableView::ReorderColumnKey` - the view fills
  that cell with an icon and a `DragSource` but never inserts the column itself,
  which would renumber every other column behind the caller's back. Keyboard
  equivalent is `EngineHotkeys::moveItemUp` / `moveItemDown` (Alt+Up / Alt+Down,
  registered `ReserveFromTextInput` because an Alt chord carries a keychar), gated
  on there being a selected row so that a table nobody picked in declines rather
  than swallows.
- A menu is `ui::MenuSource` (the model) plus one `ui::MenuSystem` on the node it
  is built into; a popup is that same pair inside a `ui::SubWindow`
  (`ui::openMenuForNode`). **One measurement decides everything** —
  `MenuSystem::measure` resolves the shared columns and every wrapped row height,
  and the same call sizes the popup surface before any node exists. The system
  owns its children's geometry, so the menu node must not carry a `LayoutSystem`;
  an item's accelerator is a named `HotkeyId` and `bindMenuHotkeys` subscribes a
  listener to all of them. The keyboard is a **mode**
  (`setKeyboardEnabled`), off by default and turned on by `openMenu` for the menu
  it builds: it installs an **`Exclusive` `FocusGroup`**, because key events are
  not hit-tested and without one the arrows of an open menu also reach everything
  else in the window ([menus](docs/usage/codestyle/ui/menus.adoc)).
