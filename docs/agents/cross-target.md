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
| Fence polling and device-loss handling in `xenolith/core` / `xenolith/backend/vk` | **`tests/compute` on Linux and under Wine**: `make -C tests/compute STAPPLER_TARGET=x86_64-pc-windows-msvc -j8`, then `WINEDEBUG=-all wine tests/compute/stappler-build/x86_64-pc-windows-msvc/debug/cc/computetest.exe`. Linux runs both fence paths (`export/` and `polled/`); Windows has only the polled one, and the Wine run is what proves it builds and runs there. winevulkan forwards to the host driver. |
| Linux/glibc, the runtime umbrella, stappler/xenolith app code | native build + run the relevant CLI test ([the test projects](test-projects.md)) |
| Per-arch Linux runtime code (`linux_sprt/<arch>_sprt/*`, arch detection in `__sprt_def.h`) on aarch64, riscv64 or loongarch64 | **cross-build + qemu-user**: `make -C tests/libc STAPPLER_TARGET=<triple> -j8`, then `tests/libc/compare.sh --target <triple>` diffs every libc test against the host under `qemu-<arch>`. The script lays out a `qemu -L` root from the target sysroot. A `*-linux-gnu` target needs two more things the sysroot does not ship: the arch's `libgcc_s.so.1` (glibc's `pthread_exit`/`pthread_cancel` `dlopen()` it; build it with the GCC of `runtime/toolchains/target-linux/glibc`, `--enable-shared`, `make all-target-libgcc`) passed with `--qemu-lib <dir>`, and a compiled `C.UTF-8` for `multibyte`/`uchar` (glibc 2.35+ does not build it in): run the target's `localedef --no-archive -i C -f <unpacked UTF-8 charmap> <dir>/C.UTF-8` from `target-linux/glibc/sysroot-target-<triple>/bin` under qemu and pass `--qemu-locale <dir>`. Compare a musl target against `x86_64-unknown-linux-musl`, not the glibc host - the libcs format differently. Expected arch noise: `printf_float` prints `nan` where x86 prints `-nan` (x86's default NaN has the sign bit set). `runtimetest`: `qemu-<arch> -L <that root> -E LD_LIBRARY_PATH=/lib tests/runtime/stappler-build/<triple>/debug/cc/runtimetest`; io_uring falls back to epoll (qemu-user has no io_uring). |
| SIMD code: geometry backends (`runtime/include/sprt/runtime/geom/simd_*.h`), raster kernel sets (`stappler/raster/SPRasterKernels*.cc`), GOST 34.11 | **against the scalar reference, on the target**: `runtimetest runtime_geom_simd` holds every geometry backend built for the arch against `simd::scalar`; `stapplertest raster` every available kernel set (sse2/sse41/avx2, neon, lsx/lasx) against the scalar set byte for byte; `stapplertest gost3411` both `Gost3411Backend`s against the RFC 6986 examples and each other. For LoongArch run them under `qemu-loongarch64` (the default CPU has LSX and LASX; `-cpu la464,lasx=off` takes LASX away). A whole build without SIMD: `-DSP_GEOM_DEFAULT_SIMD=SP_GEOM_DEFAULT_SIMD_SCALAR`, `-DSP_GOST3411_SCALAR=1`, and `SP_RASTER_KERNELS=scalar` at run time. |

If a rebuild reports "nothing to do" after you edited a file that *should* be in
scope, you are probably either (a) on a target that excludes that file, or
(b) hitting a stale object cache — `touch` the SCU and rebuild.
