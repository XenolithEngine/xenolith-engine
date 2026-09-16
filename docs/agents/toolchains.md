# Toolchains & tools

*The SDK toolchains, how they are found, and the tools around them.*

*Part of the [build & test guide](../../AGENTS.md).*

- **GNU Make 4.1+** is required. macOS ships an older `make` — use `gmake` there.
- The SDK ships its **own clang/LLD/lldb toolchain**, selected automatically per
  host triple (the host half) and per target triple (the target half). The build
  resolves each half by searching **two locations in order** (see
  `make/utils/defaults.mk`):
  1. `<root>/toolchains/hosts/<host>/host.mk` and
     `<root>/toolchains/targets/<target>/target.mk` — **externally installed
     toolchains from the binary releases** (checked first).
  2. if not found there, `<root>/runtime/toolchains/hosts/<host>/host.mk` and
     `<root>/runtime/toolchains/targets/<target>/target.mk` — **toolchains built
     from source on this machine** (the in-repo `runtime/toolchains` build).

  A release install therefore takes precedence over a local source build of the
  same triple; remove/rename the `toolchains/...` copy to force the source build.
  No platform SDK is needed (Windows without UCRT/Windows SDK; macOS without the
  macOS SDK when using a `+sprt` target). Override the whole search with
  `STAPPLER_HOST_FILE=` / `STAPPLER_TARGET_FILE=` (an explicit file wins over both
  locations).
- **`headergen`** (`utils/headergen/`) regenerates the icon tables the 2d renderer
  ships — `headergen material <material-design-icons>/src` writes `XL2dIcons.{h,cpp}`
  into `gen/`. The path points at `src`, not at the checkout root: icon names are
  built from the path, and from the root every name gains a `Src_` prefix. Run it
  when the upstream icon set moves; for the current set its output is
  byte-identical to `xenolith/renderer/basic2d/icons`.
- **`xlmake`** (`utils/xlmake/`) is the project's GNU-make-compatible build driver
  (also a drop-in for the VSCode Makefile Tools extension); the `make` invocations
  in this document work with either.
- To **run** a binary that is not native to the build machine you need the
  matching runner installed: **Wine** for a Windows `.exe`, an **Android
  device/emulator** (plus Gradle to assemble the APK) for Android, and **macOS**
  for a `.app` bundle. Building (compile-verification) never requires these.
