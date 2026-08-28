# The build system in one screen

*One screen: goals, switches, where output lands, and what a project makefile sets.*

*Part of the [build & test guide](../../AGENTS.md).*

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
| `-jN` | parallel build (**except Android-NDK** — see [per-platform detail, 3.3](platforms.md)) |
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
