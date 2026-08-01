# Cross-built Windows host toolchain

This directory cross-builds the `x86_64-pc-windows-msvc` host toolchain — clang, lld, lldb,
glslang, SPIR-V tools — from a Linux host, against sprt rather than against the MSVC CRT and
the Windows SDK. It is driven by `Makefile` + `stage0.mk`.

The first half of this file describes how the build fits together and what sprt's part in it
is; the second describes LLVM's own test suites, which are off by default.

## How the build works with sprt

### The layers

```
toolchains/hosts/<host-id>/            native (Linux) clang, lld, llvm-lib — the cross compiler
  └─ target-windows/intermediate/<triple>+dll/   the target sysroot: sprt, compiler-rt, headers
       └─ host-windows-msvc/cross/build/llvm_stage0/     LLVM built against that sysroot
            └─ .../cross/sysroot-windows/<triple>/       stage0 install prefix
                 └─ toolchains/hosts/<triple>/           the released Windows toolchain
```

`make -C runtime/toolchains host-cross-x86_64-pc-windows-msvc` walks that chain. It first
builds `target-x86_64-pc-windows-msvc+dll` (see below for the suffix), then runs `make -C
host-windows-msvc/cross out`, which builds stage0 and assembles `hosts/<triple>` from it —
`release`, `host.mk`, `bin/` with the tools and `sprt.dll`, and `xlmake.exe`.

The compiler doing the cross-compiling is the *native* host toolchain in
`toolchains/hosts/$(HOST_ID)`: `stage0.mk` points `STAGE0_CC`/`STAGE0_CXX` at its
`bin/clang-cl`, `STAGE0_LLD` at `lld-link`, and so on. Nothing Microsoft-authored takes part
at any point.

### sprt in place of the CRT and the SDK

* **libc, libm, the C++ runtime and libc++** are sprt. Headers come out of the source tree,
  not out of a sysroot copy: `runtime/include`, `runtime/include_libc`,
  `runtime/include/sprt/wrappers/windows` (+`/casemap`) are put on `INCLUDE` by `stage0.mk`,
  and the C++ ones (`runtime/include_libc/cxx`, `runtime/libcxx/include`) are passed as `-I`
  in `STAGE0_CXX_INCLUDE_PATH`. The `-I` is deliberate: clang-cl keeps `INCLUDE` *behind* its
  own builtin header directory and no flag reorders it, so a libc++ header placed there would
  lose to the builtin one.
* **The Win32 API surface** is described by the `.def` files in
  `runtime/include/sprt/wrappers/windows/def/` (kernel32, kernelbase, ntdll, …).
  `target-windows/init-target.mk` turns each into an import library with `llvm-lib /def:`,
  merges them into `usr/lib/import.lib`, and merges them again together with the runtime
  archive into `usr/lib/sprt.lib`. Adding an export to a `.def` therefore does not reach
  consumers until that merged `sprt.lib` is rebuilt — the link line names only `sprt.lib`,
  never the per-DLL import libs.
* **compiler-rt builtins** are cross-built inside the target (`target-windows/compiler_rt.mk`)
  and installed from the intermediate sysroot.

### How anything links against sprt

Every generated toolchain file (`target-windows/init-target.mk`'s `toolchain.cmake`, and
`host.cmake`/`hostcxx.cmake` in `stage0.mk`) says the same three things:

* `CMAKE_MSVC_RUNTIME_LIBRARY=Sprt`, defined as `-Xclang --dependent-lib=sprt` — the CRT
  selection every MSVC-flavoured project makes gets redirected to sprt;
* `CMAKE_C/CXX_STANDARD_LIBRARIES = /NODEFAULTLIB sprt.lib` — nothing else is on the link
  line, and the MSVC default libs the driver would otherwise emit are suppressed;
* the target triple, the sysroot, and `-D_WIN32_WINNT=0x0A00`.

Consequence worth remembering: **the stage0 clang cannot link an ad-hoc program**.
`clang hello.c -o hello.exe` fails on `libcmt.lib`/`oldnames.lib`, because the driver emits the
MSVC default libraries and there is no `.cfg` in `bin/` redirecting the target to sprt. CMake
builds escape this only through the toolchain file above.

### Static runtime vs the `+dll` variant

`SPRT_TOOLCHAIN_SHARED` selects how the runtime is delivered, and the two arrangements cannot
share a sysroot directory — `usr/lib/sprt.lib` is the runtime *archive* in one and sprt.dll's
*import library* in the other, same path and silently incompatible. Hence the `+dll` directory
suffix; the compiler triple stays bare.

| | static (`SPRT_TOOLCHAIN_SHARED=0`, the target default) | `+dll` (`=1`) |
|---|---|---|
| `usr/lib/sprt.lib` | runtime archive merged with the Win32 import libs | sprt.dll's import library + the startup stub |
| image contents | each image carries its own runtime state | one owner: heap, stdio, TLS, atexit live in the DLL |
| consumer flags | — | `-DSPRT_SHARED_RUNTIME` |
| what is linkable by name | the whole libc surface | only the annotated surface plus `SPRT_ABI_EXPORTS` |

This directory pins `+dll` unconditionally, and not as a preference: the Windows-hosted
toolchain links DLLs of its own — liblldb, LTO, the `DynamicLibrary` unit tests — and
`_DllMainCRTStartup` exists only in the shared startup stub, while the static runtime carries
`mainCRTStartup` instead. Those targets do not link at all against a static sprt.

Two details of the shared mode leak into everything built here:

* Parts of the runtime are per-image on PE and can never be imported — the `.CRT$X*`
  initializer sections, the TLS directory, the `/GS` cookie. They live in
  `runtime/libc_impl/app/windows/{app,exe}_startup.cpp`, which are compiled as *consumer* code
  and baked into `sprt.lib` as archive members, the same way msvcrt.lib carries the MSVC
  startup stubs. Nobody has to add a file by hand.
* Windows resolves imports from the directory of the running image and has no rpath
  equivalent, so `sprt.dll` has to travel next to every executable. It is staged into the
  intermediate sysroot's `usr/bin`, copied into the released `bin/` by this `Makefile`, and —
  when tests are enabled — into the build tree as well (see below).

Two smaller consequences, both of which have cost debugging time before:

* **Link-based feature probes mis-detect under a shared runtime.** `check_function_exists`
  links a call with no header in scope; sprt reaches libc through header umbrellas forwarding
  to `__sprt_*`, and sprt.dll exports only that side, so the plain name is not a symbol.
  `check_symbol_exists`, which compiles against the header, is unaffected — that asymmetry is
  the tell. This is one reason the *target* sysroot stays static: it hands out a
  self-contained archive in which the whole libc surface is linkable by plain name.
* **sprt does not implement the MSVC C++ exception ABI.** `_CxxThrowException` calls
  `terminate()`; any `throw` aborts, static or shared. LLVM is built with `LLVM_ENABLE_EH=Off`
  accordingly.

### Order inside a target build

`target-windows/Makefile` runs the arch recipe in passes, and the order is load-bearing only
from an empty intermediate directory: `toolchain` (the toolchain file, whose recipe also
plants the `host` symlink, plus the Win32 import libraries) → `compiler-rt` → the rest of
`init-target.mk` (simde, sprt, the merged `sprt.lib`) → the third-party libraries → install.
Linking `sprt.dll` needs `clang_rt.builtins`, which needs what the first pass lays down; that
circularity is what the passes exist for. The `+dll` recipe skips the third-party pass — its
only consumer, this directory, does not read any of those libraries.

`install-target.mk` then copies the intermediate sysroot into `toolchains/targets/<triple>`.
A static export deliberately drops `sprt.lib` (consumers rebuild the runtime from source and
would otherwise link a stale copy by accident); a `+dll` export must keep it, since an import
library cannot be regenerated without the DLL.

### What stage0 does with it

`stage0.mk` builds, in order, zlib and libxml2, then LLVM (clang/lld/lldb + compiler-rt),
then Vulkan/SPIR-V headers, SPIR-V Tools and glslang, installing everything into
`sysroot-windows/<triple>`. Two generated CMake toolchain files are used, and the difference
matters: `host.cmake` for the C-only projects, `hostcxx.cmake` for everything C++ — only the
latter carries the libc++ include paths. `INCLUDE` and `LIB` are exported by `stage0.mk`
itself, with `LIB` pointing at the `+dll` intermediate sysroot; running `ninja` by hand in a
build directory without them fails in ways that read as libc++ bugs rather than as missing
paths.

Both toolchain-file rules depend on `stage0.mk`, so editing flags here regenerates them.
Sub-projects must ask for LTO with `CMAKE_INTERPROCEDURAL_OPTIMIZATION=On` and never with a
`-DCMAKE_CXX_FLAGS` cache entry: that *replaces* what CMake derives from the toolchain file's
`CMAKE_CXX_FLAGS_INIT`, which is where the libc++ include paths live.

## The LLVM test suites

They are off by default. Why they are here at all: they are the broadest exercise of sprt as
a libc available — tens
of thousands of process launches through sprt's stdio, filesystem, temp files and pipes,
plus the googletest unit suites, which take their `GTEST_OS_WINDOWS` path and so cover a
slice of the MSVC/Win32 surface — the `-A` entry points (`GetCommandLineA`,
`CreateProcessA`, `GetTempFileNameA` with `STARTUPINFOA`), `CRITICAL_SECTION` as a real
`struct _RTL_CRITICAL_SECTION`, `_stricmp`/`_strnicmp`, `_CrtSetDbgFlag` with the
`_CRTDBG_*` flags, `DebugBreak`, `<sys/timeb.h>`.

(Steering googletest onto its POSIX backend, the way libc++ is steered off
`_LIBCPP_WIN32API`, would need a patch to the vendored tree: `gtest-port-arch.h` defines
`GTEST_OS_WINDOWS` unconditionally under `_WIN32` and the sources test it with `#ifdef`.)

### Enabling them

```sh
# from runtime/toolchains/host-windows-msvc/cross
make stage0 STAGE0_TESTS=1
```

`STAGE0_TESTS` defaults to `0`. Turning it on adds, to the stage0 clang configure line:

* `LLVM_INCLUDE_TESTS` / `LLVM_BUILD_TESTS` — one switch covers both halves,
  `llvm/CMakeLists.txt` adds `test/` and `unittests/` under it, so the lit trees and the
  googletest binaries arrive together;
* `CLANG_INCLUDE_TESTS` — a separate option that has to be asked for explicitly, or
  `check-clang` is absent from the generated tree (it does not follow `LLVM_INCLUDE_TESTS`
  once the cache holds `OFF`);
* `LLDB_INCLUDE_TESTS` — likewise separate. It brings in `lldb/unittests` and `lldb/test`;
  the latter's API half is driven by `dotest.py` and needs `LLDB_ENABLE_PYTHON`, which this
  build keeps off, so what actually becomes runnable is the unit tests and the Shell suite.

It also makes the build take noticeably longer and the tree noticeably larger — that, and
the fact that a released toolchain never needs them, is why the default is off.

Two patches in `replacements/llvm/21.1.8-sprt-windows/` are part of this and are applied
regardless of the switch:

* `0004-lit-Make-the-suites-usable-when-cross-testing-under-wine.patch` — see
  [Cross-testing under wine](#cross-testing-under-wine);
* `0005-clang-Do-not-require-clang-repl-for-the-test-suites.patch` — `clang/test` declares a
  hard dependency on `clang-repl`, which this build switches off
  (`CLANG_TOOL_CLANG_REPL_BUILD=Off`), and configuring fails outright without the patch.
  clang-repl is the only target in the tree that asks the linker to re-export the MSVC C++
  runtime ABI by name (`?nothrow@std@@`, `?__type_info_root_node@@`, the operator
  new/delete set) for its JIT. sprt is not the MSVC CRT and keeps those in its own inline
  namespace (`__sprt`), so satisfying that list would mean inventing duplicate objects
  under the canonical names. An interactive C++ REPL is not part of a cross toolchain;
  everything else links without it.

### Cross-testing under wine

The suites run the cross-built Windows `.exe` directly on the Linux host: a `binfmt_misc`
handler for MZ images makes them executable and `llvm-lit` is host Python. No emulator
wrapper is configured or needed.

What `STAGE0_TESTS=1` does to the build tree after `cmake --build`, before install:

**`STAGE0_LIT_TOOL_ALIASES`** — extensionless symlinks in `build/llvm_stage0/bin`. lit
addresses the tools it drives by their extensionless names (`FileCheck`, `llvm-config`) and
leaves appending `.exe` to the OS, which a POSIX host does not do. There is no single place
to fix it: `lit.util.which` only consults `PATHEXT` when `os.pathsep` is `;`, and other call
sites join a name onto `llvm_tools_dir` and spawn it directly. Untreated, every suite aborts
with `Did not find FileCheck in .../bin`. The symlinks supply what `CreateProcess` would
supply implicitly on Windows and cover every lookup path at once.

`bin/` only. The unit-test binaries are found by the GoogleTest format instead (patch
`0004`): they must keep their real names, because `CommandLineTest` asserts that `argv[0]`
ends in `.exe`.

**`STAGE0_LIT_RUNTIME_STAGING`** — copies `sprt.dll` into `build/llvm_stage0/bin` and
symlinks it next to every `*Tests.exe`. Against the `+dll` sysroot every image here imports
`sprt.dll`, `clang.exe` included, and only the *released* `bin/` gets the runtime staged
into it (see the cross `Makefile`), never the build tree. Windows resolves imports from the
directory of the running image and has no rpath equivalent; `PATH` is not an option either,
because lit hands the inferior a POSIX `PATH`, which wine does not use to resolve imports.
A symlink beside the binary is what the loader actually looks at.

Both failure modes this prevents are misleading:

* a unit-test directory without the DLL reports as
  `failed_to_discover_tests_from_gtest` for the whole binary, which reads like a test bug;
* `bin/` without the DLL means `clang -print-file-name=include` answers with nothing and lit
  aborts the entire suite with `Couldn't find the include dir for Clang` rather than
  reporting a load failure. **If you ever see that message, or an absurd failure rate,
  check `build/llvm_stage0/bin/sprt.dll` before anything else** — every result is void
  without it.

What patch `0004` covers, none of which can be decided from the host lit runs on:

* GoogleTest format matches `*Tests.exe` regardless of host — otherwise `test/Unit`
  discovers nothing at all;
* `WINEDEBUG` / `XDG_RUNTIME_DIR` / `LANG` / `LC_ALL` pass through lit's scrubbed
  environment, in both `lit/llvm/config.py` and `test/Unit/lit.cfg.py` (the Unit suite
  builds no `LLVMConfig`, so it needs its own copy). Without `LANG`/`LC_ALL` wine's unix
  codepage is not UTF-8 and non-ASCII filenames cannot be encoded on the host filesystem;
* the builtin-header path is normalized to forward slashes — the shell that re-parses a RUN
  line runs `shlex` in posix mode and eats backslashes, so `%clang_cc1` would lose them;
* a per-test empty file is substituted for a literal `/dev/null`. lit already does this, but
  gates it on `kAvoidDevNull = kIsWindows` — a property of the host, not of the build under
  test. ~740 clang tests pass `/dev/null` as an output path and the string reaches the
  Windows inferior verbatim. The flag has to travel on `test.config` and not on the
  `TestRunner` module global: configs are read in the parent, tests run in worker
  processes, and Python's default start method on Linux has been `forkserver` since 3.14,
  so a module-level mutation is silently lost.

### Running the suites

```sh
cd build/llvm_stage0
WINEDEBUG=-all ./bin/llvm-lit -sv --timeout=300 test          # check-llvm
WINEDEBUG=-all ./bin/llvm-lit -sv --timeout=300 tools/clang/test
```

or through the generated targets (`ninja -C build/llvm_stage0 check-llvm check-llvm-unit
check-lld`).

* `WINEDEBUG=-all` is not optional in practice — wine writes `fixme:`/`semi-stub:` lines to
  the inferior's stderr, and tests that assert on stderr exactly (a trailing `CHECK-NOT`, or
  `2>&1 | count 0`) fail on that noise. It is passed through by patch `0004`, but it still
  has to be in the environment.
* `--timeout=300` is likewise not optional (psutil is installed): lit defaults to no
  per-test limit, and `AllClangUnitTests.exe` shard 11/48 hangs forever on
  `DirectoryWatcherTest.DeleteWatchedDir`, spinning at 99% CPU and stalling the whole run.
* Conversely: a timeout seen only under `-j16` and not on a lone re-run is CPU starvation
  under wine, not a hang. The two ThinLTO lld tests (`COFF/thinlto-archives.ll`,
  `MachO/thinlto-emit-imports.ll`) time out in a full batch and finish in ~11 s alone.

### Reading the results

Some failure classes are structural to cross-testing and are **not** sprt defects — an
MSVC-built clang tested the same way would show them too. As of the 2026-07-31 sweep
(868 failures: clang 560, llvm 249, lld 59):

| n | class | sprt bug? |
|---|---|---|
| 285 | POSIX path has no drive letter | no |
| 239 | a PE process cannot exec a host ELF | no |
| 110 | host-OS lit features (`system-linux` is added instead of `system-windows`) | no |
| 234 | unattributed — FileCheck/verify mismatches, 36 of them `llvm-cov` | unknown |

* **PE cannot exec ELF.** `not grep`, `not ls`, `not cmp` and anything spawning
  `/usr/bin/python3` fail with `unable to find X in PATH` or `Failed getting status for
  program: ERROR_INVALID_HANDLE`. The same `grep` run by lit's *internal* shell works,
  because that is host Python.
* **Drive letters.** POSIX absolute paths are "relative" to an `llvm::sys::path` compiled in
  windows style — `is_absolute` wants a drive letter. Confirmed for `@response` files
  (`@/home/...` fails in all four lld flavours and in `llvm-lib`, while a relative or
  `Z:\...` form works), suspected for the `llvm-cov` "source files must be specified"
  cluster. Not fixable in sprt; the harness would have to feed `Z:\` paths.
* **Host-OS features.** `LLVMConfig` derives `system-*` from `platform.system()` rather than
  from `config.host_triple`. Proven: changing that turns all 110 into 107 UNSUPPORTED + 3
  PASS. Not applied — same root cause as the `/dev/null` bug, one line in the same chain.
* **lldb-shell** is dominated by a real toolchain limitation rather than a test bug: those
  tests compile an inferior to debug, and the stage0 clang cannot link an ad-hoc program
  (the driver emits the MSVC default libs and there is no `.cfg` in `bin/` redirecting the
  target to sprt). The link fails, lldb prints nothing, FileCheck reports `'<stdin>' is
  empty`: 74 of 243 name `libcmt` directly, 111 show the empty-output symptom.

### Rebuilding after a test run

`rm -rf build/llvm_stage0` fails once the lit suites have run: tests that exercise write
failures leave directories behind with the write bit cleared on purpose (see
`llvm-ifs/fail-file-write.test`) and `rm` cannot descend into them. The `$(STAGE0_CLANG_CC)`
rule always wipes the tree, so every rebuild after testing would stop there with `Permission
denied`. It runs `chmod -R u+rwX` first for that reason; if you clean by hand, do the same.

To keep a tested tree and rebuild incrementally, run `ninja` in `build/llvm_stage0` directly
instead of the make target — but export `INCLUDE`/`LIB` the way `stage0.mk` sets them, or
compilation fails with `unresolved using declaration` on `mbstate_t`, which reads as a
libc++ bug rather than as a missing include path.
