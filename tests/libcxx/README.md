# libc++ conformance suite against the sprt runtime

This runs the **upstream libc++ test suite** (`llvm-project/libcxx/test/std/…`)
against the sprt runtime's C++ standard library and validates it on three targets:

| Target | Triple | Executes via |
|--------|--------|--------------|
| Linux (native) | `x86_64-unknown-linux-gnu` | runs the binary directly |
| Windows | `x86_64-pc-windows-msvc` | **wine** |
| WebAssembly | `wasm32-unknown-unknown` | headless **Node.js** (`runtime/wasm-js/run-node.mjs`) |

The standard library under test is the **vendored libc++ port** (`runtime/libcxx`)
retargeted onto sprt's own libc through the overlay in `runtime/include_libc/cxx`
(which supplies `__config_site`/`__assertion_handler` and points every libc++
OS/threading/allocation hook at the sprt primitives). The libc++ suite explicitly
supports this "foreign standard library" mode (see
`libcxx/test/configs/stdlib-native.cfg.in`): the sources under `test/std/` exercise
**standard behaviour**, not libc++ internals, so building them against this include
stack and linking the sprt runtime measures conformance end to end.

The libc++-**internal** tests under `test/libcxx/` are not run — only `test/std/`.

### Include contract

Every runner puts the same layers on the search path (highest priority first):

```
runtime/include_libc/cxx     # libc++ overlay: __config_site + sprt retargeting
runtime/libcxx/include       # the vendored libc++ headers
runtime/include_libc         # sprt's C library headers
runtime/include              # sprt runtime (sprt::* primitives the overlay calls)
libcxx/test/support          # portable test helpers (min_allocator.h, MoveOnly.h, …)
```

## Usage

Each target has a **single-scope** driver (compiles → links → runs one slice, and
forwards extra `llvm-lit` args such as `-v`/`-a`) and a **batch** driver (sweeps the
whole tracked scope set and prints one machine-readable summary line per scope,
feeding the conformance dashboards).

| | single scope | full sweep |
|---|---|---|
| Linux | `run.sh [<scope>] [lit args…]` | `run-all.sh [<scope>…]` |
| Windows (wine) | `run-win.sh <scope>` | `run-win.sh` |
| Wasm (Node) | `run-wasm.sh [<scope>] [lit args…]` | `run-all-wasm.sh [<scope>…]` |

`<scope>` is a path under `libcxx/test/std` (e.g. `containers/sequences/vector`,
`input.output`, `algorithms`). The batch summary line is:

```
SCOPE|discovered|pass|compile_fail|link_fail|run_fail|unsupported|unresolved
```

### Linux (native)

```sh
tests/libcxx/run.sh                                  # containers/associative
tests/libcxx/run.sh containers/sequences/vector
tests/libcxx/run.sh algorithms -v                    # -v prints failing output
tests/libcxx/run.sh input.output -a                  # -a shows every result
tests/libcxx/run-all.sh                              # full sweep -> dashboard lines
```

### Windows (under wine)

`run-win.sh` builds the sprt libc++ for the Windows PE/COFF target (MSVC C++ ABI,
sprt's own POSIX libc — see "Windows POSIX mode" below), links with `lld-link`
against the vendored Windows import lib, and executes each `.exe` under wine.

```sh
tests/libcxx/run-win.sh                              # full sweep
tests/libcxx/run-win.sh input.output time            # just these scopes
```

The single `run.sh` driver is also target-parameterised — `STAPPLER_TARGET`
selects the triple — but `run-win.sh` bundles the wine executor and the
Windows-specific link line, so it is the supported path for Windows.

### WebAssembly (headless Node)

`run-wasm.sh` compiles with the wasm clang, links a `.wasm` module, and runs it
through `runtime/wasm-js/run-node.mjs` (a headless Node runner over shared memory).
`sprt_format.py` is target-agnostic — it links to a file and runs `$SPRT_EXEC
<file>` — so the wasm module is executed verbatim. Node.js must be on `PATH`.

```sh
tests/libcxx/run-wasm.sh utilities/optional
tests/libcxx/run-wasm.sh numerics -v
tests/libcxx/run-all-wasm.sh                         # full sweep -> dashboard lines
```

## How it works

1. The driver builds the sprt runtime via the existing `tests/libc` build
   (`make -C tests/libc STAPPLER_TARGET=<triple>`) and collects the runtime object
   files — everything that is not a `tests/libc` test TU — into an object list.
2. It rebuilds `SPRTCxxNewDelete.cpp` with `-DSPRT_NO_STRONG_OPERATOR_NEW_DELETE`
   and swaps it into that list: the runtime normally installs the replaceable
   global `operator new`/`delete` as **strong** symbols (it owns the allocator), but
   libc++'s allocation tests define their own replacement operators, which would
   collide at link. With the switch, only the sprt-internal (non-replaceable)
   nothrow operators remain and libc++abi's weak set backs the rest, so a test that
   replaces `operator new` links cleanly and one that doesn't still links.
3. It exports the toolchain contract (compiler, compile/link flags, the runtime
   object list, the executor `$SPRT_EXEC`, and the active `-std`) as environment
   variables.
4. `lit.cfg.py` + `sprt_format.py` drive `llvm-lit`: each `*.pass.cpp` is
   compiled → linked against the runtime → run (exit 0 = PASS); each
   `*.compile.pass.cpp` is compile-only. Failures are classified by phase into
   **Compile Failed** / **Link Failed** / **Runtime Failed** (hence the three
   `*_fail` columns in the summary line).

`// UNSUPPORTED:` / `// REQUIRES:` / `// XFAIL:` / `// ADDITIONAL_COMPILE_FLAGS:`
directives are honoured via lit's own boolean-expression evaluator against the
curated feature set in `sprt_format.py`. Persistent config-inherent XFAILs are
tracked in `xfail-tests.md`.

## Feature set / gating

`sprt_format.py: FEATURES` describes what the target can do: the active language
standard (`c++20` + the `std-at-least-c++NN` ladder), a `msvc` tag on the
`*-windows-msvc` triple (the MS C++ ABI differs from Itanium in ways some tests
XFAIL/UNSUPPORT), and capability tags whose **absence** skips tests needing them
(`no-exceptions` — the runtime is built `-fno-exceptions`, `no-localization`,
`no-random-device`, …). Extend it per-run with `SPRT_EXTRA_FEATURES="…"` or by
editing `FEATURES`.

## Modes

- **`SPRT_COMPILE_ONLY=1`** — measure *compilation* conformance only: skip the
  runtime build, object collection and the whole link/run stage. Isolates "does it
  compile against the ported libc++" for the dashboard (linking/running are separate
  porting stages).
- **`SPRT_STD_THREADING=sprt`** (linux `run.sh`) — build the whole stack with
  `-DSPRT_STD_THREADING_SPRT` so the overlay backs `std::mutex`/`condition_variable`/
  `thread` with sprt primitives instead of the vendored upstream classes. The two
  modes are *different ABIs*; the libcxx-module objects are invalidated on a mode
  switch so no stale-flavour object is ever linked. Default is `upstream`.

## Windows POSIX mode

On `x86_64-pc-windows-msvc` the sprt libc++ is built with **`_LIBCPP_WIN32API`
suppressed** (via `_LIBCPP_SPRT_NO_WIN32API` in the overlay's `__config_site`): every
OS-service subsystem (filesystem, `chrono`, `system_error`, `<print>`, `fstream`)
takes its POSIX code path against sprt's own POSIX libc rather than libc++'s
`<windows.h>` reimplementations, and `std::filesystem::path::value_type` is
`char`/UTF-8. The MS C++ ABI and 16-bit `wchar_t` are unchanged. See
`feature-request-optional-win32api.md` for the upstream rationale.

## Caveats (why numbers are approximate)

- **Sparse feature-test macros.** Where `<version>` does not yet define a
  `__cpp_lib_*` macro, a test that gates its body on `#ifdef __cpp_lib_X` compiles an
  empty body and **passes trivially** — coverage is quietly understated until the
  macro is filled in.
- **Harness noise under parallelism.** The wine/`-j` path occasionally reports a
  spurious single-test compile/run failure that does not reproduce on re-run; treat a
  lone failure that vanishes on a second run as environmental.
- Some support headers assume exceptions/threads; a few tests fail to build for that
  reason rather than a genuine defect.
