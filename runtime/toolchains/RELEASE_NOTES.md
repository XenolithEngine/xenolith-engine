# Xenolith Toolchains — `sdk-v0beta1`

Universal, self-contained C/C++ toolchains for building **Xenolith Runtime** and
projects based on it, on **Linux, Windows and macOS**, cross-compiling to every
supported platform without installing a vendor SDK (Windows SDK, Android SDK,
macOS/iOS SDK) on the build machine.

> **Beta.** The toolchains work, compile and are mostly tested. Release packages
> are assembled on **x86_64 Linux**; other build hosts have not been validated
> for full builds. Apple hosts/targets are built on Apple hardware due to SDK
> licensing.

This release publishes two sets of binary packages:

* **Hosts** — a complete clang/LLVM toolchain (compiler, linkers, debugger,
  shader tooling) that *runs* on a given platform.
* **Targets** — a complete sysroot (libc, compiler-rt, system headers,
  bundled libraries) that you *build for*.

You pick one host (matching the machine you compile on) and one or more targets
(the platforms you ship to). All packages are `.tar.xz` with a detached GnuPG
signature (`.tar.xz.sig`).

---

## Packages

### Hosts (10)

Each host package ships clang 21 with the full LLVM tool suite (`clang`,
`clang++`, `clang-cl`, `lld`/`ld.lld`/`ld64.lld`/`lld-link`, `lldb`, `llvm-ar`,
`llvm-objcopy`, `llvm-nm`, …), the bundled shader tools `glslang` and
`spirv-link`, and the **`xlmake`** build driver (`xlmake.exe` on Windows). The
`lldb` builds carry patches for debugging Wine-hosted binaries.

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

### Targets (20)

| Package | Builds for |
|---|---|
| `x86_64-unknown-linux-gnu`            | Linux / glibc, x86-64 |
| `aarch64-unknown-linux-gnu`           | Linux / glibc, ARM64 |
| `riscv64-unknown-linux-gnu`           | Linux / glibc, RISC-V 64 |
| `x86_64-unknown-linux-musl`           | Linux / musl, x86-64 |
| `aarch64-unknown-linux-musl`          | Linux / musl, ARM64 |
| `riscv64-unknown-linux-musl`          | Linux / musl, RISC-V 64 |
| `aarch64-unknown-linux-android`       | Android, arm64-v8a |
| `armv7a-unknown-linux-androideabi`    | Android, armeabi-v7a |
| `i686-unknown-linux-android`          | Android, x86 |
| `x86_64-unknown-linux-android`        | Android, x86-64 |
| `unknown-ndk-linux-android`           | Android, bridge sysroot for use with an installed Android NDK |
| `x86_64-apple-macosx`                 | macOS, Intel |
| `aarch64-apple-macosx`                | macOS, Apple Silicon |
| `x86_64-apple-macosx+sprt`            | macOS, Intel — with integrated Xenolith Runtime (`libsprt`) |
| `aarch64-apple-macosx+sprt`           | macOS, Apple Silicon — with integrated Xenolith Runtime (`libsprt`) |
| `aarch64-apple-ios`                   | iOS device, ARM64 |
| `aarch64-apple-ios-simulator`         | iOS Simulator, Apple Silicon |
| `x86_64-apple-ios-simulator`          | iOS Simulator, Intel |
| `x86_64-pc-windows-msvc`              | Windows, x86-64 |
| `aarch64-pc-windows-msvc`             | Windows, ARM64 |

Notes:

* **`+sprt`** targets bundle the prebuilt Xenolith Runtime (`libsprt`) and its
  headers, so applications link the runtime directly from the sysroot. With a
  `+sprt` target you do **not** build the `runtime` module yourself.
* **`unknown-ndk-linux-android`** is a thin bridge sysroot: it ships the Xenolith
  libc shim/headers but compiles through an externally installed Android NDK
  (`ndk-build` / `$NDK`), rather than the bundled clang. The four concrete
  `*-linux-android(eabi)` targets are self-contained and use the bundled clang.
* Apple targets require the matching Apple SDKs. On a macOS host they are located
  automatically via `xcrun`; when cross-building they must be supplied as
  `MacOSX.sdk` / `iPhoneOS.sdk` / `iPhoneSimulator.sdk` (Apple SDK license
  applies — these are **not** redistributed in these packages).

---

## Bundled component versions

The compiler and every library are built from pinned upstream sources. This is
the complete manifest shipped in this release.

### Compiler & toolchain
| Component | Version |
|---|---|
| LLVM / Clang / LLD / LLDB | 21.1.8 (`llvmorg-21.1.8`, + Wine-LLDB & non-`__ulock` patches) |
| libc++ / libc++abi / libunwind / compiler-rt | 21.1.8 (from LLVM) |
| GNU Make | 4.4.1 (not present on Windows) |
| xlmake (build driver) | 1.0 |
| SIMDe | pinned `f3e8262` |
| libbacktrace | pinned `549b81b` |

### Vulkan / shaders (Vulkan SDK 1.4.350.0)
| Component | Version |
|---|---|
| Vulkan-Headers | `vulkan-sdk-1.4.350.0` |
| Vulkan-Loader | `vulkan-sdk-1.4.350.0` |
| Vulkan-ValidationLayers | `vulkan-sdk-1.4.350.0` |
| Vulkan-Utility-Libraries | `vulkan-sdk-1.4.350.0` |
| SPIRV-Headers | `vulkan-sdk-1.4.350.0` |
| SPIRV-Tools | `vulkan-sdk-1.4.350.0` |
| glslang | `vulkan-sdk-1.4.350.0` |
| MoltenVK (Apple) | 1.4.1 |

### Compression & archive
| Component | Version |
|---|---|
| zlib | 1.3.2 |
| bzip2 | 1.0.8 |
| xz / liblzma | 5.8.3 |
| zstd | 1.5.7 |
| brotli | 1.2.0 |
| libzip | 1.11.4 |

### Image
| Component | Version |
|---|---|
| libjpeg-turbo | 3.1.4.1 |
| libpng | 1.6.58 |
| giflib | 5.2.2 (+ backports: CVE-2026-26740, CVE-2026-23868) |
| libwebp | 1.6.0 |
| libtiff | 4.7.1 (+ backport: CVE-2026-4775) |

### Text, fonts & i18n
| Component | Version |
|---|---|
| FreeType | 2.14.3 |
| HarfBuzz | 14.2.1 |
| ICU4C | 78.3 |
| libxml2 | 2.15.3 |

### Crypto & network
| Component | Version |
|---|---|
| OpenSSL (LTS) | 3.5.7 |
| openssl-gost-engine | 3.0.3 |
| MbedTLS (LTS) | 3.6.6 |
| nghttp3 | 1.16.0 |
| libcurl | 8.20.0 (MbedTLS **and** OpenSSL variants; HTTP/3 in the OpenSSL variant) |
| CA bundle | `cacert-2026-05-14.pem` + Russian Trusted CA (Root / Sub / Sub-2024) |

### Database & runtime
| Component | Version |
|---|---|
| SQLite | 3.53.2 (amalgamation 3530200) |
| WAMR (wasm-micro-runtime) | 2.4.4 |

### Linux windowing / system headers
| Component | Version |
|---|---|
| wayland-protocols | 1.49 |
| plasma-wayland-protocols | 1.21.0 |
| DBus / XCB / XKB / wayland-client | essential headers only (libraries not bundled) |

### Windows
| Component | Version |
|---|---|
| xwin (used to splat the MSVC CRT / Windows SDK headers & import libs) | 0.9.0 |

> The MSVC CRT and Windows SDK themselves are Microsoft-licensed and are fetched
> on demand via `xwin`; they are not redistributed in these packages.

---

## How to use

### 1. Download & verify

```sh
# Pick a host matching your machine and the target(s) you ship to:
#   hosts/<host-id>.tar.xz       + .sig
#   targets/<target-id>.tar.xz   + .sig

# Verify the detached GnuPG signature, then extract
gpg --verify x86_64-unknown-linux-gnu.tar.xz.sig x86_64-unknown-linux-gnu.tar.xz
tar xJf x86_64-unknown-linux-gnu.tar.xz
```

Each host package extracts to `bin/ lib/ share/ host.mk release`; each target to
`usr/ lib/ include_libc/ share/ target.mk release`. The `release` file holds the
SDK tag (`sdk-v0beta1`).

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
    -idirafter $HOST/lib/clang/21/include \
    main.c -o main
```

Swap `--target` / `--sysroot` / `-resource-dir` to retarget. Android `*-linux-android`
targets work the same way with the bundled clang; the `unknown-ndk-linux-android`
sysroot is meant to be driven through an installed Android NDK instead.

### 4. `xlmake` — bundled build driver

Every host package includes **`xlmake`** (version 1.0; `xlmake.exe` on Windows)
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

## License

All build scripts and the distribution itself are licensed under **MIT** — no
GPL restrictions are implied on software that uses these toolchains. The bundled
DBus headers are licensed under `AFL-2.1 OR GPL-2.0`. The Microsoft Windows SDK
and Apple SDKs are subject to their respective vendor licenses and are not
redistributed here.
