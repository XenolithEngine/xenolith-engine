# Golden rules (read first)

*The prohibitions and defaults that a build breaks silently without.*

*Part of the [build & test guide](../../AGENTS.md).*

- **Build with `xenolith-cli` when available; `make` is the fallback.**
  Check once per session: `command -v xenolith-cli`. If it is on
  `PATH`, **always** build through it — do **not** call `make` / `gmake` /
  `xlmake` directly. Only if the CLI is absent may you fall back to absolute
  `make -C <abs-path>` (the rest of this document documents that fallback and
  the underlying make system the CLI drives).
  ```sh
  # Preferred (CLI present) — engine checkout as STAPPLER_ROOT:
  xenolith-cli build <abs-proj-path> --engine <abs-engine-root> \
      [--target <triple>] [--release] [--run]

  # Fallback ONLY when `command -v xenolith-cli` fails:
  make -C <abs-proj-path> STAPPLER_TARGET=<triple> -j8
  ```
  Inside this monorepo, pass `--engine` (or `$XENOLITH_ENGINE`) pointing at the
  engine root so the live tree is used instead of a baked snapshot. Outside the
  monorepo (scaffolded apps with an installed SDK) omit `--engine`. Never
  hand-roll a compiler command line to "test a build".
- **One build system underneath.** Every project is a small `Makefile` that sets
  `LOCAL_*` variables and `include`s `make/universal.mk`. Project makefiles
  contain **no build rules**. The CLI (or raw `make` as fallback) is what
  drives them so flags, includes, modules and the toolchain match.
- **Every build is a cross-compile.** The target is the triple
  `STAPPLER_TARGET=<arch>-<vendor>-<os>[-<env>][+sprt]` (CLI: `--target <triple>`).
  With no target the host is auto-detected from `uname` (e.g.
  `x86_64-unknown-linux-gnu`); passing the matching triple explicitly is
  equivalent and recommended for reproducibility.
- **A successful build is `exit 0`.** Check the exit status. For CLI test apps a
  clean run also prints `N checks, 0 failures`.
- **`.cpp`/`.c` are compile units (SCU); `.cc` files are `#include`-only
  subunits** and are *never* compiled standalone. Do not `clang -c foo.cc` to
  "check" it — you get false-positive errors. Build the `.cpp`/`.c` SCU that
  `#include`s it (driven by `SP_SOURCE_FILES_PATTERN`).
- **Some code is not built on the Linux host.** `runtime/libc_impl`, Android-only
  code, and macOS-only code compile to *nothing* on a native Linux build. To
  verify changes there you **must** build the matching target (see [per-platform detail](platforms.md) and [verifying on the right target](cross-target.md)).
  A green native build does **not** prove those files even compiled.
- **Shell cwd can drift between calls.** Always pass **absolute** project (and
  `--engine`) paths to the CLI / `make -C`; never rely on `cd`.
- **App code cannot use POSIX sockets** (the runtime libc lacks them). Use the
  OpenSSL BIO socket API instead.
- **Before writing code, read the code style reference** —
  [docs/usage/codestyle/index.adoc](../usage/codestyle/index.adoc) (summary in
  [code style](code-style.md) below). This document covers building; that one covers how the sources are
  written.
