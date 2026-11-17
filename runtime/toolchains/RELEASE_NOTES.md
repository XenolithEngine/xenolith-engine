# Xenolith Toolchains — `sdk-v0rc2`

Universal, self-contained C/C++ toolchains for building **Xenolith Runtime** and
projects based on it, on **Linux, Windows and macOS**, cross-compiling to every
supported platform without installing a vendor SDK (Windows SDK, Android SDK,
macOS/iOS SDK) on the build machine.

This release publishes two sets of binary packages:

* **Hosts** — a complete clang/LLVM toolchain (compiler, linkers, debugger,
  shader tooling) that *runs* on a given platform.
* **Targets** — a complete sysroot (system headers, libc where the target has
  one, compiler-rt, bundled libraries) that you *build for*.

You pick one host (matching the machine you compile on) and one or more targets
(the platforms you ship to). Packages are staged as unsigned `.tar.xz` archives
with a `SHA256SUMS-sdk-v0rc2` manifest; detached GnuPG signatures (`.tar.xz.sig`)
are added when publishing. Every package carries a `release` file holding
the SDK tag (`sdk-v0rc2`).

Release assets are named `host_<id>.tar.xz` and `target_<id>.tar.xz`. The prefix
is not cosmetic: a GitHub release has one flat asset namespace, and the host and
the target for the same triplet are otherwise both called `<triplet>.tar.xz`.
Inside the archive the top-level directory is still the bare `<id>`.

---

## What changed since `sdk-v0rc1`

### Target libraries and trusted certificates

The regular target dependency set is refreshed: zlib 1.4, Brotli 1.2.1,
libjpeg-turbo 3.2.1, libpng 1.6.59, FreeType 2.15.0, HarfBuzz 15.1.0,
SQLite 3.54.1, libffi 3.8.1, nghttp3 1.19.0, ngtcp2 1.26.0 and curl 8.23.0.
E2K retains its MCST libffi 3.3 port. Vulkan headers, loader, utility libraries,
SPIRV-Headers, SPIRV-Tools and glslang move to Vulkan SDK 1.4.363.0;
ValidationLayers uses the 1.4.363.1 hotfix.

OpenSSL 3.5.9 includes five upstream security backports: CVE-2026-88822,
CVE-2026-88823, CVE-2026-95691, CVE-2026-95692 and CVE-2026-107496.
Expat 2.10.0 includes the upstream CVE-2026-124317 backport and its control-flow
prerequisite. FreeType's former CVE-2026-50811 backport is now included upstream.
This does not mean that every known upstream Expat issue is resolved.

Both the toolchain curl bundle and the runtime's embedded CA bundle use the
2026-11-05 Mozilla snapshot (118 certificates). The three Russian Trusted CA
certificates are preserved byte for byte, bringing the bundle to 121 certificates.

The compiler/bootstrap versions (LLVM 22.1.8, GCC 15.2.0 and the system libc
sources) are unchanged. Xenolith OS, RTOS targets and Mesa are outside this
release refresh and are not included in its package set.

### Build, runtime and packaging fixes

* Native host exports declare their outputs before CMake configures them, and
  host-variable initialization works with GNU Make 4.3.
* Shader tools link SPIRV-Tools statically; cross-host packages export the tools
  and libraries needed to run independently of the build machine.
* FreeType/HarfBuzz bootstrap and static linking are ordered explicitly. Windows
  builds use zlib 1.4's `z.lib` archive name.
* musl host installs preserve dependency timestamp ordering so repeated exports
  do not spuriously rebuild zlib, libxml2 and LLVM.
* The Windows runtime keeps `argv` NULL-terminated and alive through process
  exit, including `atexit` handlers. This also fixes SPIRV-Tools startup.
* wasm `libsprt.a` includes the freestanding memory/string primitives. wasm curl
  uses the supported wakeup configuration, and dependency tests respect the
  libraries available on each target.
* Apple `+open` sysroots use the generated runtime ABI descriptions for their
  libc and framework declarations.

## How this release was checked

* Clean builds produced 12 hosts and 29 target exports: 24 packaged targets,
  three E2K sysroots and two Windows DLL helper sysroots. OS/RTOS and Mesa were
  excluded. The release contains 36 archives (12 hosts + 24 targets).
* `check-release.py --tag sdk-v0rc2` checks all packaged hosts and targets;
  the additional five target exports are checked separately. Archive validation
  compares file lists, paths, links, modes, sizes and every regular file's contents
  against the exported directories. SHA-256 checks cover all 36 archives.
* All eight Linux hosts pass eight tool startup probes each, natively or under
  QEMU. Six x86-64 Windows host version probes pass under Wine. Windows ARM64 and
  macOS packages are checked structurally; they were not run on this Linux host.
* Linux, wasm32 and wasm64 functional suites pass, including dependency,
  Stappler, runtime, libc and bundle checks. wasm64 also passes the memory64
  checks, including bundle reads above 4 GiB.
* Windows/Wine dependency checks pass. Four runtime cases (`libc_at`,
  `libc_setjmp`, `runtime_dispatch`, `runtime_address_wait`) still fail or time out;
  Stappler retains the same 13 failures as the original installed SDK. These
  baseline failures are not counted as successful tests.
* The clean-build full engine gate passed before release labeling (74 jobs,
  130,188,333 checks). Labeling runs encountered intermittent `flowtest`
  `parallel-threads` and font-persistence failures; both pass on isolated reruns.
  Golden references are unchanged. This refresh does not repeat the previous
  release's all-project/all-target build matrix.

## Packages

### Hosts (12)

Each host package ships clang 22 with the LLVM tool suite (`clang`, `clang++`,
`clang-cl`, `lld`/`ld.lld`/`ld64.lld`/`lld-link`/`wasm-ld`, `lldb`, `llvm-ar`,
`llvm-objcopy`, `llvm-nm`, `clang-format`, …), the bundled shader tools
`glslang` and the `spirv-*` suite, and the **`xlmake`** build driver
(`xlmake.exe` on Windows). The `lldb` builds carry patches for debugging
Wine-hosted binaries.

| Package | Runs on |
|---|---|
| `x86_64-unknown-linux-gnu`   | Linux / glibc, x86-64 |
| `aarch64-unknown-linux-gnu`  | Linux / glibc, ARM64 |
| `riscv64-unknown-linux-gnu`  | Linux / glibc, RISC-V 64 |
| `loongarch64-unknown-linux-gnu` | Linux / glibc 2.36+, LoongArch 64 |
| `x86_64-unknown-linux-musl`  | Linux / musl, x86-64 |
| `aarch64-unknown-linux-musl` | Linux / musl, ARM64 |
| `riscv64-unknown-linux-musl` | Linux / musl, RISC-V 64 |
| `loongarch64-unknown-linux-musl` | Linux / musl, LoongArch 64 |
| `x86_64-pc-windows-msvc`     | Windows, x86-64 |
| `aarch64-pc-windows-msvc`    | Windows, ARM64 |
| `x86_64-apple-macosx`        | macOS, Intel |
| `aarch64-apple-macosx`       | macOS, Apple Silicon |

Notes:

* Linux and macOS hosts also ship **GNU Make 4.3** as `bin/make`. The Windows
  hosts do not (use `xlmake.exe`), and carry a reduced tool set overall — the
  compiler, the linkers, `lldb`, `glslang`, `spirv-link`, `xlmake` and the
  `llvm-*` tools the build needs.
* The Windows hosts require **`sprt.dll` to sit next to the executables** —
  Windows resolves imports from the directory of the running image and has no
  rpath equivalent. It is inside `bin/`; keep it there.
* The macOS `lldb` is built with `LLDB_USE_SYSTEM_DEBUGSERVER` — `debugserver`
  needs `task_for_pid` entitlements and Apple code signing, so it is not shipped.

### Targets (24)

| Package | Builds for |
|---|---|
| `x86_64-unknown-linux-gnu`            | Linux / glibc, x86-64 |
| `aarch64-unknown-linux-gnu`           | Linux / glibc, ARM64 |
| `riscv64-unknown-linux-gnu`           | Linux / glibc, RISC-V 64 |
| `loongarch64-unknown-linux-gnu`       | Linux / glibc 2.36+, LoongArch 64 |
| `x86_64-unknown-linux-musl`           | Linux / musl, x86-64 |
| `aarch64-unknown-linux-musl`          | Linux / musl, ARM64 |
| `riscv64-unknown-linux-musl`          | Linux / musl, RISC-V 64 |
| `loongarch64-unknown-linux-musl`      | Linux / musl, LoongArch 64 |
| `aarch64-unknown-linux-android`       | Android, arm64-v8a |
| `armv7a-unknown-linux-androideabi`    | Android, armeabi-v7a |
| `i686-unknown-linux-android`          | Android, x86 |
| `x86_64-unknown-linux-android`        | Android, x86-64 |
| `unknown-ndk-linux-android`           | Android, bridge sysroot for use with an installed Android NDK |
| `x86_64-apple-macosx`                 | macOS, Intel (against a real Apple SDK) |
| `aarch64-apple-macosx`                | macOS, Apple Silicon (against a real Apple SDK) |
| `x86_64-apple-macosx+open`            | macOS, Intel — Xcode-SDK-free sysroot |
| `aarch64-apple-macosx+open`           | macOS, Apple Silicon — Xcode-SDK-free sysroot |
| `aarch64-apple-ios`                   | iOS device, ARM64 |
| `aarch64-apple-ios-simulator`         | iOS Simulator, Apple Silicon |
| `x86_64-apple-ios-simulator`          | iOS Simulator, Intel |
| `x86_64-pc-windows-msvc`              | Windows, x86-64 |
| `aarch64-pc-windows-msvc`             | Windows, ARM64 |
| `wasm32-unknown-unknown`              | WebAssembly, 32-bit |
| `wasm64-unknown-unknown`              | WebAssembly, 64-bit (memory64) |

Notes:

* **`unknown-ndk-linux-android`** is a thin bridge sysroot: it ships the Xenolith
  libc shim/headers for all four ABIs but compiles through an externally
  installed Android NDK (`ndk-build` / `$NDK`), rather than the bundled clang.
  The four concrete `*-linux-android(eabi)` targets are self-contained, use the
  bundled clang and target **API level 24**.
* **Apple targets other than `+open`** require the matching Apple SDKs. On a
  macOS host they are located automatically via `xcrun`; when cross-building they
  must be supplied as `MacOSX.sdk` / `iPhoneOS.sdk` / `iPhoneSimulator.sdk`
  (Apple SDK license applies — these are **not** redistributed here).
* On iOS the Vulkan validation layer is shipped as
  `VkLayer_khronos_validation.framework` (upstream forces a framework there)
  instead of a flat `.dylib`.
* The Windows `+dll` sysroot variants (`*-pc-windows-msvc+dll`, built around
  `sprt.dll` instead of the static runtime archive) exist as build targets and
  are what the Windows host toolchain is produced from, but they are **not
  published** as packages. E2K sysroots and OS/RTOS targets are also outside
  the published package set.

### What a target sysroot actually contains

| Target family | libc in the sysroot | C++ runtime bits | Vulkan runtime |
|---|---|---|---|
| `*-unknown-linux-gnu` | glibc **2.33** (riscv64: **2.35**, loongarch64: **2.36**) + Linux 5.10 LTS UAPI headers (loongarch64: 6.1 LTS) | `include_libc/c++/v1`, `libc++abi.a`, `libunwind.a` | headers only — system loader |
| `*-unknown-linux-musl` | musl **1.2.6** (pinned upstream) | same | headers only — system loader |
| `*-linux-android(eabi)` | bionic stubs + headers, API 24 | same | headers only — system loader |
| `*-apple-macosx`, `*-apple-ios*` | the Apple SDK's (not redistributed) | provided by the platform | **`libvulkan.dylib` + MoltenVK + validation layer** bundled |
| `*-apple-macosx+open` | apple-oss headers in `include_libc` + generated `.tbd` link stubs, `System/Library/Frameworks/` | `libc++.tbd`/`libc++abi.tbd` stubs, apple-oss headers | same as above |
| `*-pc-windows-msvc` | **none** — sprt is built from the engine sources; the sysroot supplies the Win32 import libraries (`usr/lib/import.lib`) | none — sprt provides libc++ | headers only — system loader |
| `wasm32/64-unknown-unknown` | **none** — `libsprt.a` ships prebuilt; the engine build system rebuilds sprt from source | `libc++abi.a`, `libunwind.a`, `c++` headers | n/a |

Not every target carries every bundled library. The full dependency suite —
libxml2, expat, libffi, WAMR, libbacktrace, wayland, libdrm — is a Linux-target
thing (the LoongArch targets lack WAMR). Apple targets ship libxml2, WAMR and libbacktrace but no
expat/libffi/wayland/libdrm. Android targets ship WAMR and libbacktrace.
**Windows and wasm carry the smallest set**: no libxml2, expat, libffi, WAMR or
libbacktrace. Every target's `libcurl` is the OpenSSL build
(`libcurl-openssl.a`), with HTTP/3.

Every packaged target ships `share/licenses`, and every target except the
`unknown-ndk-linux-android` bridge ships `lib/clang` — the compiler-rt resource
dir, with sanitizers where the platform supports them.

The Windows target export deliberately **drops `sprt.lib`**: consumers rebuild
the runtime from source and would otherwise link a stale copy by accident.

---

## Bundled component versions

The compiler and every library are built from pinned upstream sources, each with
a `_SHA256` (and, where upstream publishes one, an OpenPGP signature) checked at
download time. This is the complete manifest shipped in this release.

### Compiler & toolchain
| Component | Version |
|---|---|
| LLVM / Clang / LLD / LLDB | 22.1.8 (`llvmorg-22.1.8`, with the existing platform patches) |
| libc++ / libc++abi / libunwind / compiler-rt | 22.1.8 (from LLVM) |
| GNU Make | 4.3 (not present on Windows hosts) |
| xlmake (build driver) | 1.1 |
| SIMDe | pinned `f3e8262` |
| libbacktrace | pinned `6f8310e` |
| binutils / GCC (glibc bootstrap only, not shipped) | 2.46.0 / 15.2.0 |

### System libc sources
| Component | Version |
|---|---|
| glibc (Linux targets) | 2.33 — riscv64: 2.35, loongarch64: 2.36 |
| musl | 1.2.6 (`runtime/musl-libc` submodule, `v1.2.6-62`: v1.2.6 + upstream fixes) |
| Linux UAPI headers | 5.10.258 (LTS) — loongarch64: 6.1.189 (LTS) |
| Android API level | 24 |
| macOS / iOS deployment target | 14.5 / 17.4 |

### Vulkan / shaders (Vulkan SDK 1.4.363.0)
| Component | Version |
|---|---|
| Vulkan-Headers | `vulkan-sdk-1.4.363.0` |
| Vulkan-Loader | `vulkan-sdk-1.4.363.0` |
| Vulkan-ValidationLayers | `vulkan-sdk-1.4.363.1` |
| Vulkan-Utility-Libraries | `vulkan-sdk-1.4.363.0` |
| SPIRV-Headers | `vulkan-sdk-1.4.363.0` |
| SPIRV-Tools | `vulkan-sdk-1.4.363.0` |
| glslang | `vulkan-sdk-1.4.363.0` (16.6.0) |
| MoltenVK (Apple) | 1.4.2 |

### Compression & archive
| Component | Version |
|---|---|
| zlib | 1.4 |
| bzip2 | 1.0.8 |
| xz / liblzma | 5.8.4 |
| zstd | 1.5.7 |
| brotli | 1.2.1 (+ LoongArch build fix) |

### Image
| Component | Version |
|---|---|
| libjpeg-turbo | 3.2.1 (no SIMD on LoongArch) |
| libpng | 1.6.59 |
| giflib | 6.1.3 (+ upstream fixes: CVE-2026-26740, integer overflows, `GifUnionColorMap` NULL dereference) |
| libwebp | 1.6.0 |
| libtiff | 4.7.2 (CVE-2026-12912 + CVE-2026-4775 fixed upstream) |

### Text, fonts & i18n
| Component | Version |
|---|---|
| FreeType | 2.15.0 |
| HarfBuzz | 15.1.0 |
| SheenBidi | 3.0.0 (Unicode 17.0) |
| libxml2 | 2.15.4 |
| expat | 2.10.0 (+ upstream CVE-2026-124317 and prerequisite patches) |
| ICU4C (build-time only — UCD & conformance data, not shipped) | 78.3 |

### Crypto & network
| Component | Version |
|---|---|
| OpenSSL (LTS) | 3.5.9 (+ five upstream security patches listed above) |
| openssl-gost-engine | 3.0.3 |
| nghttp3 | 1.19.0 |
| ngtcp2 | 1.26.0 |
| libcurl | 8.23.0 (OpenSSL, with HTTP/3) |
| CA bundle | `cacert-2026-11-05.pem` + Russian Trusted CA (Root / Sub / Sub-2024) |

### Database & runtime
| Component | Version |
|---|---|
| SQLite | 3.54.1 (amalgamation 3540100) |
| WAMR (wasm-micro-runtime) | 2.4.5 |
| libffi | 3.8.1 (E2K: MCST 3.3) |

### Linux windowing / system
| Component | Version |
|---|---|
| wayland (client/cursor/egl, built) | 1.26.0 |
| wayland-protocols | 1.49 |
| plasma-wayland-protocols | 1.21.0 |
| libdrm | 2.4.134 |
| DBus / XCB / XKB / X11 | essential headers only (libraries not bundled) |

### Windows
| Component | Version |
|---|---|
| xwin (legacy Windows-native bootstrap only) | 0.10.0 |

> The published Windows host and target packages contain **no MSVC CRT and no
> Windows SDK**: the toolchain runs on sprt and the Win32 surface is described by
> the runtime's own `.def` files. `xwin` remains in the tree for the legacy
> Windows-native bootstrap; nothing Microsoft-licensed is redistributed.

---

## How to use

### 1. Download & verify

```sh
# Pick a host matching your machine and the target(s) you ship to:
#   host_<host-id>.tar.xz       + .sig
#   target_<target-id>.tar.xz   + .sig

# Verify the detached GnuPG signature, then extract
gpg --verify host_x86_64-unknown-linux-gnu.tar.xz.sig host_x86_64-unknown-linux-gnu.tar.xz
tar xJf host_x86_64-unknown-linux-gnu.tar.xz     # unpacks into x86_64-unknown-linux-gnu/
```

The `host_` / `target_` prefix is part of the asset name only — the directory
inside the archive is the bare toolchain id, which is what the build system
looks for.

Each host package extracts to `bin/ lib/ share/ host.mk release`; each target to
`usr/ lib/ share/ target.mk release` plus `include_libc/` where the target
carries libc headers.

### 2. Use with the Xenolith / Stappler build system (recommended)

Place the extracted packages where the build system looks for them:

```
<root>/toolchains/hosts/<host-id>/      # contains host.mk
<root>/toolchains/targets/<target-id>/  # contains target.mk
```

(`<root>/runtime/toolchains/...` is also searched.) Then select host and target on the
make command line — using either GNU `make` or the bundled `xlmake` (see below),
which is a drop-in replacement:

```sh
make    STAPPLER_HOST=x86_64-unknown-linux-gnu   # GNU make
xlmake  STAPPLER_HOST=x86_64-unknown-linux-gnu   # bundled driver, same makefiles
```

The build system auto-includes `hosts/$(STAPPLER_HOST)/host.mk` and
`targets/$(STAPPLER_TARGET)/target.mk`. `host.mk` exports `HOST_CC`/`HOST_CXX`/
`HOST_AR`/`HOST_GLSLANG`/`HOST_SPIRV_LINK` and host include flags; `target.mk`
exports `TARGET_SYSROOT`/`TARGET_NAME`/`TARGET_SYSTEM` and the
`--target`/`--sysroot`/`-resource-dir` flags. If `STAPPLER_TARGET` is omitted it
defaults to `STAPPLER_HOST` (native build). Use custom locations with
`STAPPLER_HOST_FILE` / `STAPPLER_TARGET_FILE`.

### 3. Use clang directly (any build system)

The toolchain is a standard clang cross-compiler: the **host** provides clang
and its own resource headers, the **target** provides the sysroot and
compiler-rt resource dir. For example, compiling on x86-64 glibc for the
x86-64 musl target:

```sh
HOST=/path/to/hosts/x86_64-unknown-linux-gnu
SYSROOT=/path/to/targets/x86_64-unknown-linux-musl

$HOST/bin/clang \
    --target=x86_64-unknown-linux-musl \
    --sysroot=$SYSROOT \
    -resource-dir $SYSROOT/lib/clang \
    -idirafter $HOST/lib/clang/22/include \
    main.c -o main
```

Swap `--target` / `--sysroot` / `-resource-dir` to retarget. Android
`*-linux-android` targets work the same way with the bundled clang; the
`unknown-ndk-linux-android` sysroot is meant to be driven through an installed
Android NDK instead.

The Windows and wasm targets are the exception: they carry no system libc, so a
plain `clang hello.c` will not link against them. They are meant to be consumed
together with the engine's `runtime` module, which builds sprt from source; the
link line names `sprt.lib` (Windows) with `/NODEFAULTLIB`, and the MSVC default
libraries must be suppressed. The wasm targets do ship a prebuilt `libsprt.a`
for consumers outside the engine build system.

### 4. `xlmake` — bundled build driver

Every host package includes **`xlmake`** (version 1.1; `xlmake.exe` on Windows)
in `bin/` — a GNU-make-compatible makefile engine and build driver. *"It's like
Ninja, but it's make."* It reads GNU-make-style makefiles and runs recipes as
child processes multiplexed through a single-threaded, non-blocking build
reactor, so no external `make` is required to build Xenolith or your own
projects.

* **Two modes.** The first argument selects the mode: *build* (default — resolve
  the dependency graph and run recipes) or *inspect* (`-i`/`--inspect` — print
  variables, recipes and prerequisites without running anything; also `-p`
  `--print-data-base`).
* **Drop-in for GNU make.** It is 4.1-compatible and supports the usual flags
  (`-f`, `-C`, `-j[N]`, `-k`, `-n`, `-s`, `-B`, `-w`, …); GNU-make-oriented
  tooling such as the VSCode *Makefile Tools* extension works against it
  unmodified. It exposes `XLMAKE_VERSION` so makefiles can detect the engine.

```sh
xlmake -j8                 # build the default goal with 8 parallel recipes
xlmake -C path/to/project  # change directory first, like make -C
xlmake -i -V STAPPLER_HOST # inspect: print one variable's expanded value
```

---

## Russian CA & GOST support

`libcurl` in these toolchains ships a CA bundle that additionally includes the
Russian national CA authorities (Ministry of Digital Development Root / Sub /
Sub-2024). GOST ciphers can be loaded statically through the
`stappler_crypto` module via the bundled `openssl-gost-engine`.

## Known limitations

* **The `+open` framework headers are not complete framework headers.** They
  declare only what real code in this repository and the host projects use, with
  every constant and symbol validated against the real SDK.
* **sprt does not implement the MSVC C++ exception ABI**, so the Windows host
  toolchain is built with exceptions off; `throw` terminates.
* **The Windows host clang cannot link an ad-hoc program on its own**
  (`clang hello.c -o hello.exe` fails on `libcmt.lib`): the driver emits the MSVC
  default libraries and there is no `.cfg` in `bin/` redirecting the target to
  sprt. Builds driven by the engine's `target.mk` or a CMake toolchain file are
  unaffected.
* **The Windows and wasm targets omit libxml2, expat, libffi, WAMR and
  libbacktrace**; Android targets omit libxml2, expat and libffi; the LoongArch
  targets omit WAMR, so `stappler_wasm` is not available there.
* **The Vulkan backend does not build for wasm** (there is no Vulkan there); wasm
  applications use the WebGPU backend. The Metal backend does not build on the
  `+open` sysroots, which carry no Metal stub.
* **No target ships ICU or libidn2 any more.** Code that linked `-licuuc` or
  `-lidn2` from these toolchains must either build ICU itself
  (`make -C target-linux icu`) or move to the runtime's own Unicode API.
* **OS/RTOS targets are not part of this release refresh.** Build them from the tree.
* **Wine does not support the shipped Windows `xlmake` container path policy.**
  Its help probe returns without output; this is recorded separately from the six
  successful Windows host version probes.
* **Expat still has known upstream issues** beyond the security fixes described
  above; this release makes no claim that all Expat CVEs are closed.

## License

All build scripts and the distribution itself are licensed under **MIT** — no
GPL restrictions are implied on software that uses these toolchains. The bundled
DBus headers are licensed under `AFL-2.1 OR GPL-2.0`. The Microsoft Windows SDK
and Apple SDKs are subject to their respective vendor licenses and are not
redistributed here.
