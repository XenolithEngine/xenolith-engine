# kdl-abi

Proves that the headers `xlkdl` generates from `runtime/kdl/abi` into `runtime/include/sprt/gen` are,
semantically, the hand-written ones they replaced. Those are taken from `runtime/include` of the git
revision that still has them (`--rev`, by default `ffe9fede`) or from `--originals DIR`; a header
`sprt/gen/X` was `sprt/X` there.

    make -C utils/xlkdl                       # or xenolith-cli build utils/xlkdl
    tests/libc/kdl-abi/compare.py             # both sets
    tests/libc/kdl-abi/compare.py --set libc  # one set
    tests/libc/kdl-abi/compare.py --inline    # with every block written in place
    tests/libc/kdl-abi/compare.py --self-test # plus the breaks that must be caught

Two sets, each with its matrix:

- `windows` (`sprt/wrappers/windows/abi`): x86_64/aarch64 MSVC and x86_64 Linux; C11, C++20; with
  and without `UNICODE`; `_M_ARM64EC` on aarch64.
- `libc` (`sprt/c`): every platform and arch of `sprt/c/cross` (e2k and e2k32 selected by their
  predefines on a clang target of the same pointer size); C11, C++20; with and without
  `__SPRT_BUILD`. Embox and NuttX compile against the OS headers in `stubs/`.

For each configuration one unit includes every non-fragment header of the set - those that compile
alone first, then those that need another header before them - and is compiled by the old paths
against the original tree, and by the `sprt/gen` paths with the generated overlay before the working
tree's `runtime/include`. Three things have to be equal: the macros
(`clang -E -dM`, bodies token by token), the declarations (`-ast-dump=json` without ids, positions,
tag ownership and doc comments, `extern "C"` blocks spread into their declarations), and the record
layouts (`-fdump-record-layouts-complete`). When they differ, each header is compiled on its own to
name the one. A header no configuration reaches is compiled on its own; one whose original compiles
nowhere is reported as such.

Before the sets it checks that `runtime/include/sprt/gen` is exactly what the descriptions generate
(no file stale, none extra; the files of the API map count as generated, `compare_api.py` proves them,
and their guards are new macros: the hand-written API headers include them). `--self-test` also changes a field type, a guard, a macro body, an
arch fragment, a block, an override and a platform fragment in the overlay, and breaks the descriptions eight ways (an
unknown property, an unclosed bracket in a type, a name from a header that is not included, an
unknown `@condition`, a public macro without a rule, a number C would read otherwise, a target
number used twice, an arch of the map missing from the list of targets); it passes
only when each is caught, with its place for the latter.

`CLANG` names the compiler (default `clang`); nothing is linked and nothing runs. The language of the
descriptions: [docs/usage/tools/xlkdl-abi.adoc](../../../docs/usage/tools/xlkdl-abi.adoc).

## The Apple `+open` overlay

    tests/libc/kdl-abi/compare_apple.py [--self-test] [--targets DIR]

proves the split of the hand-written headers of `runtime/toolchains/target-apple/open/sysroot`
(API by hand, ABI generated from `runtime/kdl/abi/apple-open`) against the overlay of `ffe9fede`.
For x86_64 and arm64 it copies the installed `<arch>-apple-macosx+open` sysroot of `--targets`
twice, puts each overlay over a copy as `open-sysroot.mk` does, and compiles one unit of every
hand-written header as Objective-C (C23, for the digit separators), Objective-C++, and - for the
`include_libc` shims - C and C++. Macros are compared by name, declarations and record layouts as
multisets (the split moves a header's ABI to its top), compiler addresses dropped. `--self-test`
changes an enum value, a field type and a macro body of the generated ABI and expects each caught.


## The API: functions, umbrellas, DLLs

    tests/libc/kdl-abi/compare_api.py [--rev REV] [--self-test] [--update-fixtures] [-j N]

It proves what `xlkdl api` writes from `runtime/kdl/api` against the hand-written runtime of
`814d1c4f`. That revision's `runtime/include`, `include_libc` and `libc_impl` are extracted;
the worktree is the generated side. It checks:

- **Fixture.** `fixtures/api` describes every form of a function, an umbrella and a DLL. Its
  output has to be `fixtures/api/expected`, file for file. Nine broken copies of it have to be
  refused at `file:line`:
  - a function of a DLL no description has;
  - an umbrella without a public name;
  - a variadic wrapper;
  - a variadic umbrella;
  - an unknown linkage;
  - `unicode=` outside `charset=`;
  - a function that is also an `export` of its DLL;
  - a preload that is neither a function nor an export;
  - a DLL described twice.

  `--update-fixtures` rewrites `expected`.
- **Committed.** `xlkdl api --check`: the generated files are what the descriptions generate.
- **Headers.** Each unit is compiled against both trees for each of its configurations:
  - every header of `sprt/wrappers/windows` on its own (two of them conflict when included
    together): x86_64 and aarch64 MSVC, C and C++, with and without `UNICODE`. A C++-only
    header in C has no original, and must not compile now either;
  - `sprt/c/__sprt_dirent.h`: the libc matrix of `compare.py`;
  - `<dirent.h>`: the libc targets, C and C++, with `SPRT_UMBRELLA_REQUIRED` 0 (prototypes)
    and 1 (inline bodies).

  It compares:
  - the functions (`-ast-dump=json`): name, type with its calling convention, attributes,
    storage, `inline`, the linkage of the first declaration of the name (a C++ redeclaration
    outside `extern "C"` is still a C function), and for a body the function it calls with the
    parameters in order. Parameter names are not compared;
  - what precedes each function's name after preprocessing: on x64 and arm64 clang drops
    `__stdcall` from the type;
  - every other declaration, compiler pointers dropped;
  - the macros, less the guards of the generated headers.

  Where the original does not compile (C on Apple targets, C++-only headers in C), the
  generated must not compile either.
- **DLLs.** For each of the 26 DLLs of the old `.def` files:
  - the `.def`: `LIBRARY` and the set of exports;
  - `llvm-nm` of the import libraries `llvm-lib` builds from the old and the new `.def` for x64
    and arm64;
  - the table, where there was one: class, file, the set of exports, preloads (from the list
    macro as well). The generated table lists the functions first, so the order is not compared;
  - the wrappers: the old `dllstub2_<dll>.cc` against the generated one plus the hand-written
    rest (`dllstub_<dll>.cc`), each compiled after `dllloader.cc` with
    `__SPRT_WIN_USE_IMPORT_LIB=0` for x86_64 and aarch64 Windows, by the host toolchain against
    the target sysroots of `TOOLCHAINS`, and both against the new loader tables (where a record
    stands is the loader's own business). Each function's disassembly has to be the same, with
    branch targets relative to their function and arm64 page addresses left to their
    relocations. A file that never compiled (ws2_32: its POSIX socket wrappers collide with the
    inline ones of `include_libc/arpa/inet.h`) has to fail now too, and its generated wrappers
    are compared with the ones they were written from.

`--self-test` breaks the descriptions six ways and expects each caught: a parameter type, a
calling convention, the DLL of a wrapper, an export, the guard of the umbrella bodies, an
`export` of a DLL. The language: [docs/usage/tools/xlkdl-api.adoc](../../../docs/usage/tools/xlkdl-api.adoc).
