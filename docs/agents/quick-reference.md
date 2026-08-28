# Quick reference: build + run/verify per target

*Build and run/verify, per target, in one table.*

*Part of the [build & test guide](../../AGENTS.md).*

Replace `<engine>` with the absolute path to this repo and `tests/window` with
your project. Success = `exit 0`. **Use the CLI column when
`xenolith-cli` is installed**; the `make` column is fallback-only ([Golden rules](golden-rules.md)).

| Target | Triple | Build (CLI — preferred) | Build (`make` fallback) | Run | Notes |
|---|---|---|---|---|---|
| **Linux glibc x86_64** *(reference)* | `x86_64-unknown-linux-gnu` | `xenolith-cli build tests/window --engine <engine> --target x86_64-unknown-linux-gnu` | `make -C tests/window STAPPLER_TARGET=x86_64-unknown-linux-gnu -j8` | run the binary on Linux | omit `--target` for native host; CLI apps self-verify, GUI apps need display + Vulkan ([per-platform detail, 3.1](platforms.md)) |
| **Windows x86_64** | `x86_64-pc-windows-msvc` | `xenolith-cli build tests/window --engine <engine> --target x86_64-pc-windows-msvc` | `make -C tests/window STAPPLER_TARGET=x86_64-pc-windows-msvc -j8` | on Windows, or under Wine | only way to exercise `runtime/libc_impl` ([verifying on the right target](cross-target.md)) |
| **Android (all ABIs)** | `unknown-ndk-linux-android` | `xenolith-cli build tests/window --engine <engine> --target unknown-ndk-linux-android -j1` | `env -u MAKEFLAGS -u MFLAGS make -C tests/window STAPPLER_TARGET=unknown-ndk-linux-android` | on an Android device/emulator | **no nested `-j`**, clear `MAKEFLAGS` if using make fallback; full APK via Gradle; see [per-platform detail, 3.3](platforms.md) |
| **macOS x86_64 / arm64** | `aarch64-apple-macosx` *(or `x86_64-…`, `…+sprt`)* | `xenolith-cli build tests/window --engine <engine> --target aarch64-apple-macosx` | `make -C tests/window STAPPLER_TARGET=aarch64-apple-macosx -j8` | on macOS | compile/build verification when no Mac is available |

Add `--release` / `RELEASE=1` for optimized builds; add `--run` on the CLI to
launch a native artifact after a successful build.

Additional target triples build the same way (full cross-compile); running them
requires the matching OS/emulator: `aarch64-unknown-linux-gnu`,
`riscv64-unknown-linux-gnu`, `x86_64-unknown-linux-musl`,
`aarch64-pc-windows-msvc` (now a full target with its own sysroot — see [per-platform detail, 3.2](platforms.md);
no emulator on an x86_64 Linux host, so build-verify only there).
