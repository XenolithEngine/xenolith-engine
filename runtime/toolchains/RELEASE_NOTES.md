# Xenolith Toolchains — `sdk-v0beta3`

Universal, self-contained C/C++ toolchains for building **Xenolith Runtime** and
projects based on it, on **Linux, Windows and macOS**, cross-compiling to every
supported platform without installing a vendor SDK (Windows SDK, Android SDK,
macOS/iOS SDK) on the build machine.

> **Beta.** The toolchains work, compile and are mostly tested. Every package in
> this release — *including the Windows and macOS hosts* — was assembled on
> **x86_64 Linux**; other build hosts have not been validated for full builds.

This release publishes two sets of binary packages:

* **Hosts** — a complete clang/LLVM toolchain (compiler, linkers, debugger,
  shader tooling) that *runs* on a given platform.
* **Targets** — a complete sysroot (system headers, libc where the target has
  one, compiler-rt, bundled libraries) that you *build for*.

You pick one host (matching the machine you compile on) and one or more targets
(the platforms you ship to). All packages are `.tar.xz` with a detached GnuPG
signature (`.tar.xz.sig`), and every package carries a `release` file holding
the SDK tag (`sdk-v0beta3`).

Release assets are named `host_<id>.tar.xz` and `target_<id>.tar.xz`. The prefix
is not cosmetic: a GitHub release has one flat asset namespace, and the host and
the target for the same triplet are otherwise both called `<triplet>.tar.xz`.
Inside the archive the top-level directory is still the bare `<id>`.

---

## What changed since `sdk-v0beta2`

### LLVM 22

The whole toolchain moved from **21.1.8 to 22.1.8** — clang, lld, lldb,
compiler-rt, libc++/libc++abi/libunwind and the resource dir (`lib/clang/22`).
The local patch set was rebased and grew from 11 to **13 patches**: four
wine-LLDB patches (one new), six `sprt-windows` patches, plus non-`__ulock`,
wasm libunwind and no-delayload.

### Third-party sources are pinned and verified

`src.mk` was rewritten from ad-hoc download recipes into declarative per-library
blocks, and every dependency now has to clear three checks before it is allowed
into `src/` — a failure stops the build instead of leaving a half-broken tree:

* the transfer actually succeeded, so a 404 page or a truncated file no longer
  reaches `tar` and fails several steps later with an unrelated message;
* `_SHA256` matches — the expected value lives in our git history, not on the
  server the file came from;
* `_SIG` verifies against `keys/<_KEY>.asc`, for the fourteen upstreams that
  publish a detached OpenPGP signature. That is what makes writing a `_SHA256`
  pin trustworthy at the one moment it matters — when somebody bumps a version.

Sources fetched with git are pinned by `_COMMIT` as well as by `_TAG`, because a
tag is a mutable pointer: if upstream re-tags, the clone is refused rather than
silently building something else. `make src-pins` re-resolves every tag to the
commit it points at today, which is how a version bump is prepared. Where an
upstream publishes nothing to verify against, the block says so and why that is
acceptable.

### libzip is gone

Dropped, not bumped. `stappler_zip` now reads and writes ZIP archives with its
own code (zlib is the only dependency left), so the library, its nine per-target
builds and the cmake feature-probe shims that carried it onto wasm/NuttX/Embox
are no longer part of any sysroot. Anything that linked `-lzip` from these
toolchains must vendor its own copy. bzip2, xz/liblzma and zstd stay: FreeType,
libtiff and curl still want them.

### ICU4C and libuidna are no longer built into the sysroots

The runtime's own Unicode 17.0 tables — case mapping, folding, collation and IDN
— replace them, so no target ships `libicuuc.a` or `libidn2.a` any more. The
icu4c *sources* are still downloaded: they are the UCD and conformance data the
table generators read. A real `libicuuc` can still be produced on demand for a
project that wants one:

```sh
make -C target-linux icu SP_ARCH=x86_64 SP_TARGET=x86_64-unknown-linux-gnu
```

Together with the libzip removal this takes `share/licenses` from 49 entries to
**48** — the icu4c and libuidna licenses stay, because their sources are still
inputs to the build.

### NuttX and Embox

`target-nuttx` (`aarch64-nuttx-none-elf`, wired into the top-level Makefile as
`target-nuttx`) and `target-embox` land in the tree as build targets, alongside
the matching runtime platform support. Neither is **published** as a package in
this release — build them from the tree.

### Everything else

* Dependency refresh across the board — see the manifest below. Notably
  **FreeType 2.14.3 carries a CVE-2026-50811 backport** (`TT_Get_Var_Design`
  bounds), and the giflib CVE-2026-26740 / CVE-2026-23868 backports are still
  applied.
* Vulkan SDK **1.4.357.0** (was 1.4.350.0), glslang 16.4.0; MoltenVK **1.4.2**.
* A glibc target linking fix.
* `make release-check` / `make release-export` in `runtime/toolchains/Makefile`
  stage the prefixed release assets described above and verify up front that
  every package of the release set was built and carries the right tag.

---

## Packages

### Hosts (10)

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
| `x86_64-unknown-linux-musl`  | Linux / musl, x86-64 |
| `aarch64-unknown-linux-musl` | Linux / musl, ARM64 |
| `riscv64-unknown-linux-musl` | Linux / musl, RISC-V 64 |
| `x86_64-pc-windows-msvc`     | Windows, x86-64 |
| `aarch64-pc-windows-msvc`    | Windows, ARM64 |
| `x86_64-apple-macosx`        | macOS, Intel |
| `aarch64-apple-macosx`       | macOS, Apple Silicon |

Notes:

* Linux and macOS hosts also ship **GNU Make 4.3** as `bin/make`. The Windows
  hosts do not (use `xlmake.exe`), and carry a reduced tool set overall — the
  compiler, the linkers, `lldb`, `glslang`, `spirv-link`, `xlmake` and the
  `llvm-*` tools the build needs, with no `share/licenses` tree.
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
| `x86_64-unknown-linux-musl`           | Linux / musl, x86-64 |
| `aarch64-unknown-linux-musl`          | Linux / musl, ARM64 |
| `riscv64-unknown-linux-musl`          | Linux / musl, RISC-V 64 |
| `x86_64-xenolithos-linux-gnu`         | Xenolith OS device, x86-64 |
| `aarch64-xenolithos-linux-gnu`        | Xenolith OS device, ARM64 |
| `riscv64-xenolithos-linux-gnu`        | Xenolith OS device, RISC-V 64 |
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
  published** as packages. The same holds for the NuttX and Embox targets.

### What a target sysroot actually contains

| Target family | libc in the sysroot | C++ runtime bits | Vulkan runtime |
|---|---|---|---|
| `*-unknown-linux-gnu` | glibc **2.33** (riscv64: **2.35**) + Linux 5.10 LTS UAPI headers | `include_libc/c++/v1`, `libc++abi.a`, `libunwind.a` | headers only — system loader |
| `*-unknown-linux-musl` | musl **1.2.6** (pinned upstream) | same | headers only — system loader |
| `*-xenolithos-linux-gnu` | glibc **2.39** + device `runtime/rootfs` | same | **`libvulkan.so` 1.4.357** bundled (no OS to provide it), GPU driver applied as an overlay by `xenolith-os` |
| `*-linux-android(eabi)` | bionic stubs + headers, API 24 | same | headers only — system loader |
| `*-apple-macosx`, `*-apple-ios*` | the Apple SDK's (not redistributed) | provided by the platform | **`libvulkan.dylib` + MoltenVK + validation layer** bundled |
| `*-apple-macosx+open` | apple-oss headers in `include_libc` + generated `.tbd` link stubs, `System/Library/Frameworks/` | `libc++.tbd`/`libc++abi.tbd` stubs, apple-oss headers | same as above |
| `*-pc-windows-msvc` | **none** — sprt is built from the engine sources; the sysroot supplies the Win32 import libraries (`usr/lib/import.lib`) | none — sprt provides libc++ | headers only — system loader |
| `wasm32-unknown-unknown` | **none** — sprt is built from the engine sources | `libc++abi.a`, `libunwind.a`, `c++` headers | n/a |

Not every target carries every bundled library. The full dependency suite —
libxml2, expat, libffi, WAMR, libbacktrace, wayland, libdrm, both `libcurl`
variants — is a Linux-target thing. Apple targets ship libxml2, WAMR,
libbacktrace and both curl variants but no expat/libffi/wayland/libdrm. Android
targets ship WAMR, libbacktrace and both curl variants. **Windows and wasm32
carry the smallest set**: no libxml2, expat, libffi, WAMR or libbacktrace, and
only the OpenSSL `libcurl` variant.

Every target ships `share/licenses` (48 entries), and every target except the
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
| LLVM / Clang / LLD / LLDB | 22.1.8 (`llvmorg-22.1.8`, + 13 patches: wine-LLDB ×4, non-`__ulock`, wasm libunwind, no-delayload, sprt-windows ×6) |
| libc++ / libc++abi / libunwind / compiler-rt | 22.1.8 (from LLVM) |
| GNU Make | 4.3 (not present on Windows hosts) |
| xlmake (build driver) | 1.1 |
| SIMDe | pinned `f3e8262` |
| libbacktrace | pinned `6f8310e` |
| binutils / GCC (glibc bootstrap only, not shipped) | 2.46.0 / 15.2.0 |

### System libc sources
| Component | Version |
|---|---|
| glibc (Linux targets) | 2.33 — riscv64: 2.35 |
| glibc (Xenolith OS targets) | 2.39 |
| musl | 1.2.6 (`runtime/musl-libc` submodule, v1.2.6 + upstream fixes) |
| Linux UAPI headers | 5.10.258 (LTS) |
| Android API level | 24 |
| macOS / iOS deployment target | 14.5 / 17.4 |

### Vulkan / shaders (Vulkan SDK 1.4.357.0)
| Component | Version |
|---|---|
| Vulkan-Headers | `vulkan-sdk-1.4.357.0` |
| Vulkan-Loader | `vulkan-sdk-1.4.357.0` |
| Vulkan-ValidationLayers | `vulkan-sdk-1.4.357.0` |
| Vulkan-Utility-Libraries | `vulkan-sdk-1.4.357.0` |
| Vulkan-Tools (`vulkaninfo`, Xenolith OS rootfs only) | `vulkan-sdk-1.4.357.0` |
| SPIRV-Headers | `vulkan-sdk-1.4.357.0` |
| SPIRV-Tools | `vulkan-sdk-1.4.357.0` |
| glslang | `vulkan-sdk-1.4.357.0` (16.4.0) |
| MoltenVK (Apple) | 1.4.2 |

### Compression & archive
| Component | Version |
|---|---|
| zlib | 1.3.2 |
| bzip2 | 1.0.8 |
| xz / liblzma | 5.8.3 |
| zstd | 1.5.7 |
| brotli | 1.2.0 |

### Image
| Component | Version |
|---|---|
| libjpeg-turbo | 3.2.0 |
| libpng | 1.6.58 |
| giflib | 5.2.2 (+ backports: CVE-2026-26740, CVE-2026-23868) |
| libwebp | 1.6.0 |
| libtiff | 4.7.2 (CVE-2026-12912 + CVE-2026-4775 fixed upstream) |

### Text, fonts & i18n
| Component | Version |
|---|---|
| FreeType | 2.14.3 (+ backport: CVE-2026-50811) |
| HarfBuzz | 14.3.1 |
| SheenBidi | 3.0.0 (Unicode 17.0) |
| libxml2 | 2.15.3 |
| expat | 2.8.3 |
| ICU4C (build-time only — UCD & conformance data, not shipped) | 78.3 |

### Crypto & network
| Component | Version |
|---|---|
| OpenSSL (LTS) | 3.5.7 |
| openssl-gost-engine | 3.0.3 |
| MbedTLS (LTS) | 3.6.7 |
| nghttp3 | 1.18.0 |
| ngtcp2 | 1.25.0 |
| libcurl | 8.21.0 (MbedTLS **and** OpenSSL variants; HTTP/3 in the OpenSSL variant) |
| CA bundle | `cacert-2026-08-13.pem` + Russian Trusted CA (Root / Sub / Sub-2024) |

### Database & runtime
| Component | Version |
|---|---|
| SQLite | 3.53.4 (amalgamation 3530400) |
| WAMR (wasm-micro-runtime) | 2.4.5 |
| libffi | 3.8.0 |

### Linux windowing / system
| Component | Version |
|---|---|
| wayland (client/cursor/egl, built) | 1.25.0 |
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
carries libc headers. Xenolith OS targets additionally carry `target.ini` (meson
cross-file), `toolchain.cmake` and `runtime/rootfs`.

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

The Windows and wasm32 targets are the exception: they carry no libc, so a plain
`clang hello.c` will not link against them. They are meant to be consumed
together with the engine's `runtime` module, which builds sprt from source; the
link line names `sprt.lib` (Windows) with `/NODEFAULTLIB`, and the MSVC default
libraries must be suppressed.

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

* **`riscv64-xenolithos-linux-gnu` is built by name only** — there is no real
  board for it yet. x86-64 and ARM64 are the validated device architectures.
* **Xenolith OS targets carry no GPU driver.** The mesa driver differs per board
  (v3dv, panvk, venus, lavapipe) and is applied as an overlay by `xenolith-os`;
  lavapipe additionally needs a native LLVM for the target architecture, which
  cross-compilation does not provide.
* **`+sprt` Apple packages are not published**; the `+open` sysroots supersede
  them. The recipes are still in `target-apple/Makefile`.
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
* **`libcurl`'s MbedTLS variant is not built for Windows and wasm32**, and those
  two targets also omit libxml2, expat, libffi, WAMR and libbacktrace. Android
  targets omit libxml2, expat and libffi.
* **No target ships ICU or libidn2 any more.** Code that linked `-licuuc` or
  `-lidn2` from these toolchains must either build ICU itself
  (`make -C target-linux icu`) or move to the runtime's own Unicode API.
* **NuttX and Embox targets are not published** — build them from the tree.

## License

All build scripts and the distribution itself are licensed under **MIT** — no
GPL restrictions are implied on software that uses these toolchains. The bundled
DBus headers are licensed under `AFL-2.1 OR GPL-2.0`. The Microsoft Windows SDK
and Apple SDKs are subject to their respective vendor licenses and are not
redistributed here.
