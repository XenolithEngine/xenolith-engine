# "Not built on the host" — verify platform code on the right target

*Verifying platform code that the host cannot build or run.*

*Part of the [build & test guide](../../AGENTS.md).*

A native Linux build silently skips code for other platforms. To prove such code
even compiles, build its target. Match the change to the verification:

| You changed… | Verify with |
|---|---|
| `runtime/libc_impl/*` (freestanding Windows libc, `windows/*`, `builtin_*` SCUs) or the `runtime_libc_wrapper` substitute functions | **`tests/libc` + win32 cross-build + Wine** ([per-platform detail, 3.2](platforms.md), [the test projects](test-projects.md)). Linux build links `runtime_libc_wrapper` instead and touches none of `libc_impl`. |
| Android-only code (`runtime/window/android/*`, dispatch `*-alooper*`, JNI/unicode) | **Android NDK target** ([per-platform detail, 3.3](platforms.md)): `STAPPLER_TARGET=unknown-ndk-linux-android` |
| macOS-only code (`runtime/window/macos/*.mm`, darwin dispatch/clock/lock) | **macOS cross-compile** ([per-platform detail, 3.4](platforms.md)): `STAPPLER_TARGET=x86_64-apple-macosx` (compile-verify when no Mac is available) |
| arm64 Windows code / shared headers | **full cross-build** ([per-platform detail, 3.2](platforms.md)): `STAPPLER_TARGET=aarch64-pc-windows-msvc` (build-verify only — no emulator on Linux). For a quick header/SCU check, host clang `--target=aarch64-pc-windows-msvc` compile-only. |
| Linux/glibc, the runtime umbrella, stappler/xenolith app code | native build + run the relevant CLI test ([the test projects](test-projects.md)) |

If a rebuild reports "nothing to do" after you edited a file that *should* be in
scope, you are probably either (a) on a target that excludes that file, or
(b) hitting a stale object cache — `touch` the SCU and rebuild.
