# Xenolith Engine

**Xenolith** is a cross-platform SDK for building modern C and C++ applications: a Vulkan-based
graphics engine, a custom system runtime (libc/C++ standard library/pthread) and a set of
application libraries, all built with a single build system without relying on platform SDKs.

The SDK is organized as a monorepository and consists of three main layers:

* **Xenolith Runtime** (`runtime/`) — an umbrella libc implementation, a full C++ standard library
  (a freestanding minimal one plus a complete libc++ port) and a custom pthread. It provides a
  single POSIX-compatible interface on top of the platform libc, and on some platforms a fully
  custom implementation.
* **Stappler utilities** (`stappler/`) — application libraries: data and serialization, database
  access, cryptography, networking, raster and vector graphics, typography, documents.
* **Xenolith Engine** (`xenolith/`) — a graphics and compute engine, Vulkan first, with WebGPU,
  Metal, OpenGL ES and software backends: the core part of the project.

The SDK also includes its own GNU Make build system (`make/`) and self-contained toolchains
(built from `runtime/toolchains/`, or installed from the binary releases into `toolchains/`) that
allow building the project without third-party SDKs.

## Project principles

* **No barriers.** The SDK requires no platform SDKs: Windows builds work without UCRT and the
  Windows SDK, Android without the NDK, macOS without the macOS SDK (the `+open` sysroot). This
  removes the related technical and licensing restrictions.
* **For C and C++.** A convenient and full-featured working environment specifically for these
  languages; support for other languages is limited to the dynamic-library level.
* **Unified API.** As far as technically possible, every supported platform gets the same interface
  — without massive configure scripts or platform-specific branches in the code.
* **Unified build.** All build tools ship as part of the SDK; the user simply builds the code with a
  fixed version of GNU Make.
* **Compatibility.** Any code built as a dynamic library must work with the SDK on supported
  platforms.

See `docs/articles/ru/general/project.adoc` for details.

## Getting started

Install the SDK command line tool (see [CLI releases](docs/usage/cli-releases.adoc) for version
pinning and mirrors):

```sh
curl -fsSL https://raw.githubusercontent.com/XenolithEngine/xenolith-engine/master/install.sh | sh
```

```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/XenolithEngine/xenolith-engine/master/install.ps1 | iex
```

```sh
xenolith-cli install          # engine + native toolchains
xenolith-cli new XenoApp && xenolith-cli build XenoApp --run
```

## Platform support

| Platform | Architectures | Notes |
|----------|---------------|-------|
| Linux | x86_64, arm64, riscv64, loongarch64 | glibc and musl; loongarch64 needs Linux 6.1 and glibc 2.36, and has no `stappler_wasm` |
| Android | all ABIs (arm64, armv7, x86, x86_64) | no Google Services; builds with and without the NDK |
| Windows | x86_64, arm64 | custom libc, no UCRT or Windows SDK; |
| macOS | x86_64, arm64 | against the Xcode SDK, or SDK-free with the `+open` sysroot |
| WebAssembly | wasm32 | browser and Node.js; custom libc, WebGPU |
| iOS | arm64 (+ simulator) | in development — target sysroots and initial runtime support |
| NuttX, Embox | arm64 | draft — hosted on the RTOS's own libc |

## Xenolith Runtime

A system runtime that provides a single POSIX-compatible interface between user code and the
platform libc. It operates in two modes:

* **Umbrella** — forwards calls to the system libc (Linux glibc/musl, Android Bionic, macOS
  libSystem, the NuttX and Embox libc).
* **Custom** — fully replaces the system libc where needed: the `libc_impl` module on Windows (with
  the mimalloc allocator, without UCRT) and on WebAssembly.

Components:

* `core/` — base primitives: a custom **pthread**, **SPRT** synchronization primitives (built on
  `futex` on Linux, `WaitOnAddress` on Windows, `os_sync_wait_on_address` on macOS,
  `memory.atomic.wait` on WebAssembly, with priority inheritance support), safe `setjmp/longjmp` with stack unwinding, dynamic library loading,
  Unicode, memory pools, time, and path handling.
* `libc_impl/` — a standalone libc implementation (Windows, WebAssembly) and the mimalloc
  allocator.
* `libc_wrapper/` — the umbrella layer that forwards to the platform libc.
* `musl-adapters/` — adapted `math`, `string`, `stdlib`, `complex` and `regex` (TRE) code from musl
  (the external `musl-libc` submodule).
* `src/` — high-level utilities: UUID, hashing, compression, URL parsing, IDN (IDNA2008), Unicode
  case mapping and collation, task dispatch, filesystem, geometry with SIMD.
* `window/` — the windowing subsystem: X11/Wayland, Windows, macOS, iOS, Android, WebAssembly,
  Embox, NuttX and a headless mode.
* `libcxx/` — the **C++ standard library** (`runtime_libcxx` module): a full port of LLVM's libc++,
  retargeted onto the runtime's own libc (see below).
* `include/sprt/` — SPRT headers, including `sprt/cxx/` — a minimal **freestanding** standard-library
  analogue (see below).

### C++ standard library

The runtime provides C++ support in two tiers:

* **`sprt/cxx/`** — a minimal, **freestanding** standard-library analogue living in `namespace sprt`
  (containers, type traits, atomics, `optional`/`variant`/`tuple`, mutexes, …). It has no hosted
  dependencies and is what the runtime itself — including the libc and the low-level primitives — is
  built with, so the lower layers never need a C++ standard library to exist yet.
* **`runtime/libcxx/`** — a **full port of LLVM's libc++** (the `runtime_libcxx` module), providing
  the real hosted `std::` for application code. The vendored libc++ tree is retargeted onto the
  runtime's own libc through the overlay in `runtime/include_libc/cxx`, which supplies
  `__config_site` and points every libc++ OS / threading / allocation hook at the sprt primitives.
  For ABI isolation the port lives in the versioned namespace `std::__sprt`, so it never clashes
  with a foreign system libc++ on ABI-shared targets. Conformance is measured with the **upstream
  libc++ test suite** (`tests/libcxx/`), run against the port on Linux, Windows (via wine) and
  WebAssembly (via Node.js).

## Graphics engine (Xenolith Engine)

Full-featured windowed applications built on Vulkan, WebGPU, Metal, OpenGL ES or a software
renderer.

* On-demand rendering (saves power while idle).
* Vector graphics and icons (including the Material Design icon set) — crisp at any pixel density.
* Pixel-perfect typography (FreeType + HarfBuzz), variable fonts, GPU glyph atlas.
* A UI toolkit (`xenolith_renderer_ui`): flexbox/grid layout, CSS styling, forms, menus, docking,
  popups and sub-windows, Markdown views; Rich Text and HTML rendering.
* Fast and responsive animation system.

The engine architecture is built around a **render graph** and per-frame execution with a pull-based
frame model (the `Presentation Engine` requests a frame from the scene). The boundary between scene
logic (the "client") and GPU execution (the "server") is factored out into a dedicated
`RenderSession` entity — which also became the foundation for remote rendering (see below).

The primary backend is Vulkan (`xenolith/backend/vk/`), with custom queue, material and mesh
compilers and its own device memory allocator. The other backends live next to it in
`xenolith/backend/`:

* `webgpu` — WebGPU: `wgpu-native` on native targets, the browser's `navigator.gpu` on WebAssembly.
* `mtl` — Metal, Darwin only.
* `gles` — OpenGL ES; EGL and GLES are loaded at runtime, nothing is linked.
* `soft` — a CPU renderer on top of the `stappler_raster` rasterizer.

Main modules:

* `xenolith_core` — the core: render graph, frames, presentation engine, materials.
* `xenolith_backend_vk`, `xenolith_backend_webgpu`, `xenolith_backend_mtl`, `xenolith_backend_gles`,
  `xenolith_backend_soft` — the backends; a non-Vulkan backend is paired with its
  `xenolith_renderer_basic2d_<backend>` module.
* `xenolith_application` — the application framework: threading model, context, windows, scene
  director.
* `xenolith_renderer_basic2d` / `ui` / `pug` / `richtext` — 2D rendering, the UI toolkit
  (flexbox/grid layout, CSS styling, widgets), pug templates, Rich Text.
* `xenolith_renderer_compositor` — a window compositor (Vulkan and software variants).
* `xenolith_remote` — remote rendering (see below).
* `xenolith_font` — typography.
* `xenolith_resources_assets` / `storage` / `network` — resources, local storage, networking.

### Remote rendering (experimental)

`xenolith_remote` (`xenolith/remote/`) — client-server rendering: a "thin" client updates the scene
graph and sends commands, while a server process does the GPU work. The protocol works end to
end — the client brings up a scene, receives frames, sends input, fonts and screenshots, with one
connection serving several windows — but is still experimental.

* **Transport** — selected by the address scheme: QUIC (the default, via OpenSSL, ALPN `xlremote`;
  the server uses an ephemeral self-signed P-256 certificate), Unix domain sockets (`unix:`),
  shared-memory rings between processes of one machine (`shm:`) and an in-process pair for tests
  (`mem:`).
* **Protocol** — custom (magic `XLRP`, version 4), bearer-key authentication (64 bytes,
  constant-time comparison), an X11-like handshake in which the server describes its gAPI, OS and
  window subsystem.
* **Serialization** — CBOR: the compiled render graph and resources are encoded into a blob; GPU
  objects are addressed by id on the server, with corresponding GPU-less "thin" handles on the
  client.
* **Compression** — LZ4 with a dictionary negotiated during the handshake; frame data sets can be
  sent by reference into a per-session frame data cache.

## Stappler utilities

Application libraries on top of Xenolith Runtime:

* **Data** (`stappler_data`) — the dynamic `Value` container, JSON / CBOR / Serenity serialization.
* **Databases** (`stappler_db`, `stappler_sql`) — PostgreSQL and SQLite; an object (Firebase-like)
  interface with schemes and fields, role-based access control, full-text search
  (`stappler_search`), virtual, computed and automatic fields, file and image fields.
* **Cryptography** (`stappler_crypto`) — OpenSSL (with GOST engine), GnuTLS and mbedTLS backends;
  RSA/ECDSA/GOST, symmetric ciphers, JWT.
* **Networking** (`stappler_network`) — HTTP/2 and HTTP/3 over cURL.
* **Graphics** — images (`stappler_bitmap`: PNG/JPEG/WebP/GIF), a CPU rasterizer with SIMD kernels
  (`stappler_raster`), vector (`stappler_vg`) with tessellation (`stappler_tess`), fonts
  (`stappler_font`).
* **Documents** — `stappler_document` (HTML/EPUB), `stappler_markdown`, `stappler_layout` (layout
  engine), `stappler_pug` (templates).
* **Other** — `stappler_zip` (self-contained ZIP reader/writer, zlib the only dependency),
  `stappler_filesystem`, `stappler_git` (a Git Smart HTTP v2 client), `stappler_makefile` (the
  GNU-make-compatible engine behind `xlmake`), `stappler_wasm` (WebAssembly guest code via WAMR),
`stappler_vstore` (a relocatable 32-bit addressed arena with a write barrier and an undo journal),
`stappler_flow_value`, `stappler_flow`, `stappler_flow_ops` and `stappler_flow_codegen` (typed records over that
arena, an execution graph with an interpreter and a resumable machine, its standard operations, and a generator that
writes a graph out as a C++ unit).

The memory management model is based on two interfaces: memory pools (`mem_pool`) and standard
allocation (`mem_std`).

## Build system

A custom modular build system on GNU Make (`make/`). Every build is treated as a cross-compilation
by default; the entry point is `make/universal.mk`.

* Modules are pulled in via `LOCAL_MODULES` and `*-modules.mk` catalogs; dependencies are resolved
  transitively.
* The target platform is set by the `STAPPLER_TARGET` triple (for example,
  `x86_64-pc-windows-msvc`, `unknown-ndk-linux-android`, `x86_64-apple-macosx`,
  `x86_64-unknown-linux-gnu`, `wasm32-unknown-unknown`); a `+variant` suffix selects a sysroot
  flavor (`x86_64-pc-windows-msvc+dll`, `aarch64-apple-macosx+open`).
* Support for compiling GLSL → SPIR-V shaders (glslang/spirv-link) and building WebAssembly
  (`wasm32-unknown-unknown` through the same toolchain and the runtime's own libc).
* Builds run under GNU Make 4.1+ or the bundled, drop-in **`xlmake`** driver (see below).

### Self-contained toolchains

The self-contained **toolchains** (`runtime/toolchains/`) are what make all of this possible. Built
around a custom LLVM, they ship every tool and library a build needs, so the build machine requires
nothing beyond the toolchain itself — no system compiler, no system libraries and no platform SDK. A
toolchain is delivered as two kinds of package that together cover the whole build:

* **Host** — the compiler that *runs* on the build machine: clang 22 / LLVM with the full tool suite
  (`clang`, `clang++`, `clang-cl`, the `lld` linkers, `lldb`, `llvm-ar` / `llvm-objcopy` /
  `llvm-nm`, …), the shader tools (`glslang`, `spirv-link`), GNU Make, and the **`xlmake`** build
  driver.
* **Target** — the sysroot you *build for*: libc (glibc / musl / a custom UCRT-free Windows libc /
  the Android bridge / Apple open-source headers and link stubs), libc++, compiler-rt, the system
  headers, and prebuilt versions of every third-party dependency (OpenSSL + GOST, curl, FreeType,
  HarfBuzz, SQLite, the Vulkan stack, image and compression libraries, …).

Because both halves are self-contained, Windows builds work without UCRT or the Windows SDK, Android
without the NDK, and macOS without the macOS SDK — removing the related technical and licensing
restrictions. The exceptions are the iOS targets and the stock `*-apple-macosx` sysroots, which
compile against the Apple SDK for licensing reasons (on macOS the SDK-free `*-apple-macosx+open`
sysroot, assembled from Apple's open-source releases, is the alternative), and the
`unknown-ndk-linux-android` bridge sysroot, which deliberately compiles through an externally
installed Android NDK.

The build looks for a toolchain in `toolchains/` (binary releases) first and in
`runtime/toolchains/` (built from source on this machine) second.

### Toolchain architectures

The toolchains build both LLVM/Clang **host** toolchains (the compiler that runs on a machine,
`runtime/toolchains/hosts/`) and **target** sysroots (what the SDK is compiled for,
`runtime/toolchains/targets/`).

**Host toolchains:**

| OS / libc | Architectures |
|-----------|---------------|
| Linux, glibc | x86_64, aarch64, riscv64, loongarch64 (`*-unknown-linux-gnu`) |
| Linux, musl | x86_64, aarch64, riscv64, loongarch64 (`*-unknown-linux-musl`) |
| Windows, MSVC ABI | x86_64, aarch64 (`*-pc-windows-msvc`) |
| macOS | x86_64, aarch64 (`*-apple-macosx`) |

**Target sysroots:**

| Platform | Architectures |
|----------|---------------|
| Linux, glibc | x86_64, aarch64, riscv64, loongarch64 (`*-unknown-linux-gnu`) |
| Linux, musl | x86_64, aarch64, riscv64, loongarch64 (`*-unknown-linux-musl`) |
| Android | arm64-v8a, armeabi-v7a, x86, x86_64; with and without the NDK (`unknown-ndk-linux-android`) |
| Windows, MSVC ABI | x86_64, aarch64; static runtime |
| macOS | x86_64, aarch64; Xcode SDK or SDK-free (`*-apple-macosx`, `*-apple-macosx+open`) |
| iOS / iOS Simulator | aarch64 (device), x86_64 + aarch64 (simulator) |
| WebAssembly | wasm32 (`wasm32-unknown-unknown`) |
| NuttX, Embox | aarch64 (`aarch64-nuttx-none-elf`, `aarch64-embox-none-elf`), draft |

> **iOS** is in development: the sysroots build, and the runtime has initial iOS support (including
> the window layer).

### `xlmake` — bundled build driver

Every host toolchain ships **`xlmake`** (`xlmake.exe` on Windows) in `bin/` — a GNU-make-compatible
makefile engine and build driver, *"like Ninja, but it's make."* It reads the same
GNU-make-style makefiles and runs recipes as child processes multiplexed through a single-threaded,
non-blocking build reactor, so the project (and any project using this build system) can be built
without an external `make` installed.

* **Drop-in for GNU Make.** It is 4.1-compatible and supports the usual flags (`-f`, `-C`, `-j[N]`,
  `-k`, `-n`, `-s`, `-B`, `-w`, …); GNU-make-oriented tooling such as the VSCode *Makefile Tools*
  extension works against it unmodified. It exposes `XLMAKE_VERSION` so makefiles can detect the
  engine.
* **Two modes.** *build* (default — resolve the dependency graph and run recipes) and *inspect*
  (`-i` / `--inspect` — print variables, recipes and prerequisites without running anything).

```sh
cd tests/window
xlmake -j8         # same makefiles as `make`, built by the bundled driver
```

## Dependencies

### Build

* GNU Make 4.1+ (or the bundled `xlmake` build driver)
* LLVM/Clang 22.1.8, the Vulkan headers and shader tools (glslang, SPIRV-Tools) — all shipped by
  the SDK toolchains

### Databases

PostgreSQL (12+) or SQLite (bundled).

### Key third-party components

Pinned versions are built as part of the toolchains:
OpenSSL 3.5.8 LTS (+ GOST engine 3.0.3), Vulkan SDK 1.4.357.0, MoltenVK 1.4.2 (Apple), WAMR 2.4.5,
FreeType 2.14.3, HarfBuzz 14.5.0, SheenBidi 3.0.0, SQLite 3.53.4, curl 8.22.0 (nghttp3 1.18.0,
ngtcp2 1.25.0), as well as zlib, zstd, brotli, xz, bzip2, libpng, libwebp, libjpeg-turbo, giflib,
libtiff, and others. Linux sysroots carry the Wayland, XCB, xkbcommon and D-Bus headers only — those
libraries are loaded at runtime.

ICU4C 78.3 is downloaded but not built: IDN, case mapping and collation are the runtime's own, and
ICU serves as the Unicode reference data for the table generators and conformance tests.

## Building and running

```sh
git clone <repo-url> xenolith-engine
cd xenolith-engine
git submodule update --init   # fetches musl-libc
```

Build an example (a graphical application). With the SDK installed, the `xenolith-cli` front-end
(`utils/installer/cli`) configures the toolchain and drives make; plain make works too:

```sh
xenolith-cli build $PWD/tests/window --engine $PWD [--target <triple>] [--release] [--run]

make -C tests/window                                  # build for the current host
make -C tests/window STAPPLER_TARGET=x86_64-pc-windows-msvc
```

Test applications live in the `tests/` directory (`window`, `stappler`, `runtime`, `remote`, …),
along with the conformance suites for the runtime's libc (`tests/libc`) and its libc++ port
(`tests/libcxx`). `tests/run-checks.py` picks and runs the checks a change can break
(`tests/run-checks.py full` runs all of them). Larger examples are in `examples/`.

## Project structure

```
runtime/   — Xenolith Runtime (libc, libc++ port, pthread, SPRT) and toolchains (runtime/toolchains)
stappler/  — application libraries (data, DB, crypto, networking, graphics, documents)
xenolith/  — graphics engine (core, backends, renderers, remote rendering)
make/      — GNU Make build system
toolchains/ — installed binary-release toolchains (looked up before runtime/toolchains)
tests/     — test applications and conformance suites
examples/  — example applications (window, os)
utils/     — tools: the installer and xenolith-cli, xlmake, headergen
docs/      — documentation (articles, platform and API references)
```

## First application

Project layout:

* `Makefile` — the project root Makefile
* `src/` — source code

### Makefile

```make
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# Path to the make/ directory within the SDK
STAPPLER_BUILD_ROOT ?= <path to xenolith-engine>/make

# Executable name
LOCAL_EXECUTABLE := testapp

# Module catalogs
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

# Modules used (dependencies are resolved automatically)
LOCAL_MODULES := \
	xenolith_application \
	xenolith_application_main \
	xenolith_renderer_ui \
	xenolith_backend_vk \
	xenolith_resources_assets

# Sources and shaders
LOCAL_SRCS_DIRS := src
LOCAL_INCLUDES_OBJS := src
LOCAL_SHADERS_DIR := shaders

include $(STAPPLER_BUILD_ROOT)/universal.mk
```

A complete working example of a graphical application is in `tests/window/`.

## Documentation

Documentation is located in the `docs/` directory:

* `docs/articles/ru/general/` — about the project, building, platform support
* `docs/platforms/` — Linux, Windows, macOS, Android, WebAssembly, NuttX and Embox specifics
* `docs/usage/` — the build system, working in an IDE, data, 2D and UI usage
* `docs/design/` — design notes (draw order, the node event pipeline, Unicode and IDN)
* `docs/api/runtime/` — runtime API reference
* `docs/agents/` — the build and test guide (see also `AGENTS.md`)

## Contact and signing key

Maintainer: Roman Katuntsev <sbkarr@stappler.org>

Releases and commits are signed with the following OpenPGP key:

```
Roman Katuntsev (Xenolith Project key) <sbkarr@stappler.org>
Key ID:      rsa4096/50243B02EB5F7F73
Fingerprint: D35F B371 7A2D CA13 E572  2EB8 5024 3B02 EB5F 7F73
```

Import it from a keyserver to verify signatures:

```sh
gpg --recv-keys D35FB3717A2DCA13E5722EB850243B02EB5F7F73
```

## License

See the [LICENSE](LICENSE) file.
