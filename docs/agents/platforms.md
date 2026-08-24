# Per-platform detail

*Linux, Windows, Android and macOS in detail, including what each one needs to RUN.*

*Part of the [build & test guide](../../AGENTS.md).*

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
  host, but verifies by **diffing host vs Windows output** (see [the test projects](test-projects.md)), not by a
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
  skipped entirely by the Linux host build ([verifying on the right target](cross-target.md)). The dedicated harness for it is
  `tests/libc` ([the test projects](test-projects.md)), which builds the same sources for the host and for this
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
  dispatch/clock/lock) actually compile ([verifying on the right target](cross-target.md)). On macOS, `errno`/exceptions
  behavior differs from Linux — do not assume Linux values.
- License note: before using Apple targets **without** `+sprt`, review the macOS
  SDK license (Apple-hardware restrictions).
