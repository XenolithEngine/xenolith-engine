# Creating a new project

*The makefile skeleton for a new project, field by field.*

*Part of the [build & test guide](../../AGENTS.md).*

### 4.1 Layout

```
myproject/
  Makefile
  main.cpp            # the translation unit with main()
  src/...             # other sources (compiled recursively)
  shaders/...         # optional GLSL → SPIR-V
  resources/...       # optional bundled assets
  proj.android/       # optional, for the Android/Gradle build
  proj.macos/         # optional, for Xcode
```

### 4.2 Makefile template

```make
# Rebuild everything if this makefile changes
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# Path to make/ (relative to this makefile). ../../make for a tests/* layout.
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# What to build: one of LOCAL_EXECUTABLE / LOCAL_LIBRARY / LOCAL_WASM_MODULE
LOCAL_EXECUTABLE := myapp

# Module catalogs to read, then the modules to use (deps resolved transitively)
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	runtime \
	xenolith_application \
	xenolith_renderer_ui \
	xenolith_backend_vk

# Sources
LOCAL_MAIN          := main.cpp     # TU with main(), compiled separately
LOCAL_SRCS_DIRS     := src          # scanned recursively for .c/.cpp/.mm
LOCAL_INCLUDES_OBJS := src          # added to the include path
LOCAL_SHADERS_DIR   := shaders      # optional

# App identity (baked into the binary / bundle)
APPCONFIG_APP_NAME    := MyApp
APPCONFIG_BUNDLE_NAME := org.stappler.MyApp

include $(STAPPLER_BUILD_ROOT)/universal.mk
```

Then build with the CLI (preferred) or `make` fallback ([Golden rules](golden-rules.md)):
```sh
xenolith-cli build myproject --engine <abs-engine-root>   # add --release / --target …
# fallback: make -C myproject -j8
```

### 4.3 Key `LOCAL_*` variables

| Variable | Meaning |
|---|---|
| `LOCAL_EXECUTABLE` | build an executable with this name |
| `LOCAL_LIBRARY` / `LOCAL_VERSION` | build a library (versioned). `LOCAL_BUILD_STATIC` / `LOCAL_BUILD_SHARED` (default `1`/`1`) choose archive/.so; `LOCAL_BUILD_SHARED=2/3` for standalone/live-reload module |
| `LOCAL_WASM_MODULE` | build a WebAssembly component |
| `LOCAL_MAIN` | TU holding `main()` (compiled apart, so sources are reusable for tests) |
| `LOCAL_MODULES` / `LOCAL_MODULES_OPTIONAL` | direct module deps (transitive resolved) / include-if-present |
| `LOCAL_MODULES_PATHS` | the `*-modules.mk` catalogs that define modules |
| `LOCAL_SRCS_DIRS` / `LOCAL_SRCS_OBJS` | source dirs (recursive) / individual sources |
| `LOCAL_INCLUDES_DIRS` / `LOCAL_INCLUDES_OBJS` | header dirs (recursive) / direct include dirs |
| `LOCAL_SHADERS_DIR` / `LOCAL_SHADERS_INCLUDE` | GLSL dirs / shader include dirs |
| `LOCAL_CFLAGS` / `LOCAL_CXXFLAGS` / `LOCAL_LDFLAGS` / `LOCAL_LIBS` | extra flags / libs (`-lfoo`, `-l:libfoo.a`) |
| `LOCAL_PRIVATE_INCLUDE_PCH` | precompiled header(s), e.g. `SPCommon.h` |
| `LOCAL_OUTDIR` / `LOCAL_INSTALL_DIR` | build output root / install destination |

### 4.4 `APPCONFIG_*` (executables get a generated config header)

| Variable | Meaning |
|---|---|
| `APPCONFIG_APP_NAME` | human-readable name (default = `LOCAL_EXECUTABLE`) |
| `APPCONFIG_BUNDLE_NAME` | reverse-DNS id, e.g. `org.stappler.MyApp` (used for macOS bundle id, Windows AppContainer name) |
| `APPCONFIG_APP_PATH_COMMON` | resource/sandbox mode. **Don't set on macOS** — non-default values break app activation and the window never maps (leave it unset, like `tests/window`). Linux: `>0` = use XDG dirs, `0` = self-contained next to exe. Windows: `1` AppData, `2` AppContainer paths, `3` run inside an AppContainer |
| `APPCONFIG_VERSION_API` / `_REV` / `_BUILD` / `_VARIANT` | version components |
| `APPCONFIG_STRINGS` / `APPCONFIG_VALUES` | extra string / numeric defines |

### 4.5 Available modules

You list only direct deps; the resolver walks `_DEPENDS_ON` transitively and
aborts with `Module not found: <name>`. The final ordered set prints at configure
time as `Enabled modules: …`.

- **runtime** (`runtime/runtime.mk`): `runtime` (the umbrella — what apps use),
  `runtime_core`, `runtime_libc_wrapper`, `runtime_libc_impl`, `runtime_musl_libc`,
  `runtime_malloc`, `runtime_window`.
- **stappler** (`stappler/stappler-modules.mk`): `stappler_core`, `stappler_data`,
  `stappler_filesystem`, `stappler_bitmap`, `stappler_crypto`, `stappler_db`,
  `stappler_sql`, `stappler_search`, `stappler_network`, `stappler_font`,
  `stappler_vg`, `stappler_tess`, `stappler_zip`, `stappler_wasm`,
  `stappler_makefile`, `stappler_pug`, `stappler_document`, `stappler_layout`,
  `stappler_brotli_lib`.
- **xenolith** (`xenolith/xenolith-modules.mk`): `xenolith_core`,
  `xenolith_application`, `xenolith_backend_vk`, `xenolith_font`,
  `xenolith_renderer_basic2d` (+ `_shaders`), `xenolith_renderer_ui`,
  `xenolith_renderer_pug`, `xenolith_renderer_richtext`,
  `xenolith_resources_assets`, `xenolith_resources_network`,
  `xenolith_resources_storage`, `xenolith_remote`.

Typical sets: a GUI app → `runtime xenolith_application xenolith_renderer_basic2d
xenolith_backend_vk`; a non-graphical Stappler app → `runtime stappler_core` plus
what you need.
