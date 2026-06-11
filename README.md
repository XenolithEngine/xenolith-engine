# Xenolith Engine

**Xenolith** is a cross-platform SDK for building modern C and C++ applications: a Vulkan-based
graphics engine, a custom system runtime (libc/STL/pthread) and a set of application libraries,
all built with a single build system without relying on platform SDKs.

The SDK is organized as a monorepository and consists of three main layers:

* **Xenolith Runtime** (`runtime/`) — an umbrella libc implementation, a partial C++ STL and a
  custom pthread. It provides a single POSIX-compatible interface on top of the platform libc, and
  on some platforms a fully custom implementation.
* **Stappler utilities** (`stappler/`) — application libraries: data and serialization, database
  access, cryptography, networking, raster and vector graphics, typography, documents.
* **Xenolith Engine** (`xenolith/`) — a Vulkan-based graphics and compute engine: the core part of
  the project.

The SDK also includes its own GNU Make build system (`make/`) and self-contained toolchains
(`runtime/toolchains/`) that allow building the project without third-party SDKs.

## Project principles

* **No barriers.** The SDK requires no platform SDKs: Windows builds work without UCRT and the
  Windows SDK, Android without the NDK, macOS without the macOS SDK. This removes the related
  technical and licensing restrictions.
* **For C and C++.** A convenient and full-featured working environment specifically for these
  languages; support for other languages is limited to the dynamic-library level.
* **Unified API.** As far as technically possible, every supported platform gets the same interface
  — without massive configure scripts or platform-specific branches in the code.
* **Unified build.** All build tools ship as part of the SDK; the user simply builds the code with a
  fixed version of GNU Make.
* **Compatibility.** Any code built as a dynamic library must work with the SDK on supported
  platforms.

See `docs/articles/ru/general/project.adoc` for details.

## Platform support

| Platform | Architectures | Notes |
|----------|---------------|-------|
| Linux | x86_64, arm64 | glibc and musl |
| Android | all ABIs | no Google Services; builds with and without the NDK |
| Windows | x86_64 | custom libc, no UCRT or Windows SDK |
| macOS | x86_64, arm64 | including builds with Xenolith Runtime (`+sprt`) |
| iOS | — | in development |

## Xenolith Runtime

A system runtime that provides a single POSIX-compatible interface between user code and the
platform libc. It operates in two modes:

* **Umbrella** — forwards calls to the system libc (Linux glibc/musl, Android Bionic, macOS
  libSystem).
* **Custom** — fully replaces the system libc where needed (Windows: the `libc_impl` module with the
  mimalloc allocator, without UCRT).

Components:

* `core/` — base primitives: a custom **pthread**, **SPRT** synchronization primitives (built on
  `futex` on Linux, `WaitOnAddress` on Windows, `os_sync_wait_on_address` on macOS, with priority
  inheritance support), safe `setjmp/longjmp` with stack unwinding, dynamic library loading,
  Unicode, memory pools, time, and path handling.
* `libc_impl/` — a standalone libc implementation (Windows) and the mimalloc allocator.
* `libc_wrapper/` — the umbrella layer that forwards to the platform libc.
* `musl-adapters/` — adapted `math`/`string` functions from musl (the external `musl-libc`
  submodule).
* `src/` — high-level utilities: UUID, hashing, compression, URL parsing, IDN (IDNA2008), task
  dispatch, filesystem, geometry with SIMD, and the windowing subsystem.
* `include/sprt/` — SPRT headers, including a partial **C++ STL** implementation (libc++/libstdc++
  are deliberately not used, for ABI isolation).

## Graphics engine (Xenolith Engine)

Full-featured windowed applications built on Vulkan.

* On-demand rendering (saves power while idle).
* Vector graphics and icons — crisp at any pixel density.
* Pixel-perfect typography (FreeType + HarfBuzz), variable fonts, GPU glyph atlas.
* Material Design widgets, Rich Text, HTML rendering.
* Fast and responsive animation system.

The engine architecture is built around a **render graph** and per-frame execution with a pull-based
frame model (the `Presentation Engine` requests a frame from the scene). The boundary between scene
logic (the "client") and GPU execution (the "server") is factored out into a dedicated
`RenderSession` entity — which also became the foundation for remote rendering (see below). The
backend is Vulkan (`xenolith/backend/vk/`), with custom queue, material and mesh compilers and VMA
integration.

Main modules:

* `xenolith_core` — the core: render graph, frames, presentation engine, materials.
* `xenolith_backend_vk` — the Vulkan backend.
* `xenolith_application` — the application framework: threading model, context, windows, scene
  director.
* `xenolith_renderer_basic2d` / `material2d` / `richtext` / `simpleui` — 2D rendering, Material
  Design, Rich Text, a simple UI toolkit (flexbox).
* `xenolith_font` — typography.
* `xenolith_resources_assets` / `storage` / `network` — resources, local storage, networking.

### Remote rendering (experimental)

`xenolith_remote` (`xenolith/remote/`) — client-server rendering: a "thin" client updates the scene
graph and sends commands, while a server process does the GPU work. The subsystem is at an early
stage (protocol and object scaffolding).

* **Transport** — QUIC (via OpenSSL), ALPN `xlremote`; the server uses an ephemeral self-signed
  P-256 certificate.
* **Protocol** — custom (magic `XLR1`, version 1), bearer-key authentication (64 bytes,
  constant-time comparison), an X11-like handshake.
* **Serialization** — CBOR: the compiled render graph and resources are encoded into a blob; GPU
  objects are addressed by id on the server, with corresponding GPU-less "thin" handles on the
  client.
* **Compression** — LZ4 with a dictionary negotiated during the handshake.

## Stappler utilities

Application libraries on top of Xenolith Runtime:

* **Data** (`stappler_data`) — the dynamic `Value` container, JSON / CBOR / Serenity serialization.
* **Databases** (`stappler_db`, `stappler_sql`) — PostgreSQL and SQLite; an object (Firebase-like)
  interface with schemes and fields, role-based access control, full-text search
  (`stappler_search`), virtual, computed and automatic fields, file and image fields.
* **Cryptography** (`stappler_crypto`) — OpenSSL (with GOST engine), GnuTLS and mbedTLS backends;
  RSA/ECDSA/GOST, symmetric ciphers, JWT.
* **Networking** (`stappler_network`) — HTTP/2 and HTTP/3 over cURL.
* **Graphics** — raster (`stappler_bitmap`: PNG/JPEG/WebP/GIF), vector (`stappler_vg`) with
  tessellation (`stappler_tess`), fonts (`stappler_font`).
* **Documents** — `stappler_document` (HTML/EPUB), `stappler_layout` (layout engine), `stappler_pug`
  (templates).
* **Other** — `stappler_zip`, `stappler_filesystem`, `stappler_wasm` (WebAssembly guest code via
  WAMR).

The memory management model is based on two interfaces: memory pools (`mem_pool`) and standard
allocation (`mem_std`).

## Build system

A custom modular build system on GNU Make (`make/`). Every build is treated as a cross-compilation
by default; the entry point is `make/universal.mk`.

* Modules are pulled in via `LOCAL_MODULES` and `*-modules.mk` catalogs; dependencies are resolved
  transitively.
* The target platform is set by the `STAPPLER_TARGET` triple (for example,
  `x86_64-pc-windows-msvc`, `unknown-ndk-linux-android`, `x86_64-apple-macosx`,
  `x86_64-unknown-linux-gnu`).
* Support for compiling GLSL → SPIR-V shaders (glslang/spirv-link) and building WebAssembly
  (WASI SDK + wit-bindgen, the WAMR runtime).

The self-contained **toolchains** (`runtime/toolchains/`) are built with a custom LLVM and ship all
tools and dependencies, which is what makes it possible to build the project without platform SDKs.

## Dependencies

### Build

* GNU Make 4.1+
* LLVM/Clang 21.1.8 (shipped by the SDK toolchains)
* For Vulkan: Vulkan SDK headers and tools (glslang, spirv-tools)
* For WebAssembly: WASI SDK and wit-bindgen

### Databases

PostgreSQL (12+) or SQLite (bundled).

### Key third-party components

Pinned versions are built as part of the toolchains:
OpenSSL 3.5.5 LTS (+ GOST), mbedTLS 3.6.5 LTS, Vulkan SDK 1.4.328.1, WAMR 2.4.4, ICU4C 78.2,
FreeType 2.14.1, HarfBuzz 12.3.2, SQLite 3.53.1, curl 8.18.0 (nghttp3 1.15.0), as well as zlib,
zstd, brotli, libwebp, libjpeg-turbo, Wayland, and others.

## Building and running

```sh
git clone <repo-url> xenolith-engine
cd xenolith-engine
git submodule update --init   # fetches musl-libc
```

Build an example (a graphical application):

```sh
cd tests/window
make            # build for the current host
```

Test applications live in the `tests/` directory (`window`, `stappler`, `runtime`, `builtin`).

## Project structure

```
runtime/   — Xenolith Runtime (libc, STL, pthread, SPRT) and toolchains (runtime/toolchains)
stappler/  — application libraries (data, DB, crypto, networking, graphics, documents)
xenolith/  — Vulkan graphics engine (core, backend, renderers, remote rendering)
make/      — GNU Make build system
tests/     — test and example applications
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
	xenolith_renderer_simpleui \
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
* `docs/platforms/` — Linux, Windows, macOS, Android specifics
* `docs/usage/` — the build system and working in an IDE
* `docs/api/runtime/` — runtime API reference

## License

See the [LICENSE](LICENSE) file.
