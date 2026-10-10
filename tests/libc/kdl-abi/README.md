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
(no file stale, none extra). `--self-test` also changes a field type, a guard, a macro body, an
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

