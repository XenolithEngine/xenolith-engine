# The ready-made test projects (use these to verify changes)

*The ready-made harnesses, what each one exercises, and which to use for a given change.*

*Part of the [build & test guide](../../AGENTS.md).*

| Project | Artifact | Modules / what it exercises | Kind |
|---|---|---|---|
| `tests/runtime` | `runtimetest` | `runtime_libc_wrapper` + `runtime` — the Xenolith Runtime (libc/STL/pthread) | CLI, self-checking |
| `tests/libc` | `libctest` | the internal libc implementation — `runtime/libc_impl` **and** the `runtime_libc_wrapper` wrappers (including the substitute/replacement functions the wrappers supply when a function is missing on the platform). Built for the host **and** `x86_64-pc-windows-msvc`; `compare.sh` diffs the two for behavioural identity | CLI, host-vs-Windows diff |
| `tests/stappler` | `stapplertest` | the `stappler_*` app modules (core/data/bitmap/crypto/db/document/font/vg/pug/makefile/layout/network) — **fast smoke build** | CLI |
| `tests/particles` | `particlestest` | `runtime` only + the header-only `XL2dGlslParticleSim.h` — the CPU reference of the GPU particle emission cycle, the same text the particle update shader compiles | CLI, self-checking |
| `tests/tess` | `tesstest` | the tesselator (`stappler/tess`) and the vector layer, against the whole 2d icon set — a pinned digest per icon **and** a pinned raster per icon, plus a deterministic wire benchmark. No device, no window, no frame | CLI, golden |
| `tests/compute` | `computetest` | Vulkan compute with no window (`xenolith_backend_vk` + `xenolith_core`): a `core::Queue` with one compute pass, `Loop::runRenderQueue`, `Loop::captureBuffer`, and a lost device through `vk::Device::setTestFault`. Needs a Vulkan device; without one it prints SKIP. `computetest timings` is the round-trip benchmark ([Measuring compute](measuring-compute.md)) | CLI, GPU |
| `examples/window/particles` | `particles` | the GPU particles of `basic2d` behind a control panel; `tests/window/particles-check.py` runs it headless and compares a GPU snapshot of the particles with the CPU reference the example computes from the same `XL2dGlslParticleSim.h` | GUI, driven by a check |
| `examples/window/fileexplorer` | `fileexplorer` | `ui::FilesystemModel` behind a two-pane navigator - a places tree, and a right pane that is either `ui::TableView` or the example's own virtualized icon grid, with image thumbnails decoded off the app thread; `tests/window/filesystem-explorer-check.py` builds a directory tree of its own and drives the app headless over it. It is the only coverage `ui::FilesystemModel` has | GUI, driven by a check |
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
- Changed `ui::FilesystemModel` (`xenolith/renderer/ui/view/XLUiFilesystemModel.*`) or
  `stappler/filesystem` → build `examples/window/fileexplorer` and run
  `tests/window/filesystem-explorer-check.py` (the runner selects it by name for either). The check
  writes its own tree - subdirectories, files, a dot-file and a directory of single-colour PNGs -
  so it asserts a known listing rather than whatever the machine happens to have; the only thing it
  reads about the real filesystem is that `/` is among the places.
- Changed the basic2d particle system or its GLSL (`xenolith/renderer/basic2d/particle`,
  `glsl/include/XL2dGlslParticle*.h`, `xl_2d_particle_update.comp`, `backend/vk/XL2dVkParticlePass`)
  → `tests/particles`, then build `examples/window/particles` and run
  `tests/window/particles-check.py` (the runner selects both). The shader build does not track
  included headers: touch the `.comp` after editing one. The model and the checks are described in
  [the particles guide](../usage/basic2d/particles.adoc).
- Changed `xenolith/core` or `xenolith/backend/vk` → `tests/compute` (the runner
  owes it for both). It covers the round trip on 1 … 10⁵ records and the device-lost
  refusals: a request after `VK_ERROR_DEVICE_LOST` gets exactly one failed callback
  and nothing hangs. Read its `device` line: SKIP counts 0 checks and proves
  nothing. `XL_COMPUTE_DEVICE=<n>` picks a device, `XL_COMPUTE_VALIDATION=1` turns
  the validation layer on. Every section runs twice, once per fence path: `export/`
  (sync_fd on the looper, Linux with a device that can export) and `polled/` (the
  loop's timer, what Windows and Android use). `export/fences-used` fails if nothing
  was really exported, and `export/fd` if the fds leak.
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
