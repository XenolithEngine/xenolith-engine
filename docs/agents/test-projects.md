# The ready-made test projects (use these to verify changes)

*The ready-made harnesses, what each one exercises, and which to use for a given change.*

*Part of the [build & test guide](../../AGENTS.md).*

| Project | Artifact | Modules / what it exercises | Kind |
|---|---|---|---|
| `tests/runtime` | `runtimetest` | `runtime_libc_wrapper` + `runtime` — the Xenolith Runtime (libc/STL/pthread) | CLI, self-checking |
| `tests/libc` | `libctest` | the internal libc implementation — `runtime/libc_impl` **and** the `runtime_libc_wrapper` wrappers (including the substitute/replacement functions the wrappers supply when a function is missing on the platform). Built for the host **and** `x86_64-pc-windows-msvc`; `compare.sh` diffs the two for behavioural identity | CLI, host-vs-Windows diff |
| `tests/stappler` | `stapplertest` | the `stappler_*` app modules (core/data/bitmap/crypto/db/document/font/vg/pug/makefile/layout/network) — **fast smoke build** | CLI |
| `tests/tess` | `tesstest` | the tesselator (`stappler/tess`) and the vector layer, against the whole 2d icon set — a pinned digest per icon **and** a pinned raster per icon, plus a deterministic wire benchmark. No device, no window, no frame | CLI, golden |
| `tests/window` | `testapp` | full xenolith GUI stack (`xenolith_application` + `renderer_ui` + `backend_vk` + `resources_assets`); transitively compiles the stappler modules | GUI |

**Which to use:**
- Changed a `stappler/` module → build `tests/window` (preferred — full stack) or
  `tests/stappler` (faster smoke). Drive either through the CLI ([Golden rules](golden-rules.md) / [the quick reference](quick-reference.md)).
- Changed `stappler/tess` or the vector canvas → `tests/tess`, and run BOTH
  goldens: `tesstest golden` compares the tesselated mesh of every icon against
  `golden/icons.txt`, `tesstest raster-golden` compares its rasterization
  against `golden/raster.txt`. Geometry can change without the digest moving and
  the other way round, so neither alone is the check. `--write` re-pins a golden,
  and re-pinning is a decision to record in the commit message, not a way to make
  a run green.
- Changed the runtime (`runtime`/`runtime_core`/wrapper) → `tests/runtime`; for
  the libc wrappers themselves also run `tests/libc`.
- Changed `runtime/libc_impl` (or the libc wrappers) → `tests/libc` (its
  `compare.sh` cross-builds for Windows and diffs against the host) **or**
  `tests/runtime`; either way the Windows cross-build is what actually compiles
  `libc_impl` ([per-platform detail, 3.2](platforms.md), [verifying on the right target](cross-target.md)).

A clean CLI-test-app verify (native):
```sh
xenolith-cli build tests/runtime --engine <abs-engine-root> \
  && tests/runtime/stappler-build/x86_64-unknown-linux-gnu/debug/cc/runtimetest
# fallback if CLI missing: make -C tests/runtime -j8 && <same binary>
# expect exit 0 and "N checks, 0 failures"
```
