# macos-abi — compile-time Darwin ABI parity for the macOS and `macOS+open` targets

The Windows sibling of this directory (`../windows-abi`) pins sprt's `__SPRT_*`
tables against a real Windows SDK. The Apple side needs the same thing and one
more besides, because **`*-apple-macosx+open` is built against no Apple SDK at
all**: its sysroot is assembled from apple-oss checkouts plus a hand-written
overlay, and `runtime/toolchains/target-apple/README.md` describes the real SDK
as being "consulted **only as a read-only reference for validation** (constant
values, export lists, symbol ownership)". That validation was real, but it was
done by hand, once — the audit script was not kept. This directory is the
re-runnable form of it.

Everything here is **compile-time only**. Nothing is linked, nothing is run, and
no Apple hardware is involved: it all works from Linux with a stock `clang`.

```sh
./check.sh                          # everything: both arches, both sysroots
./check.sh --arch aarch64
./check.sh --sysroot open
./check.sh --only sprt              # or: sysroot, tbd
./check.sh -v                       # print the clang command lines
```

## You will usually see SKIP, and that is correct

The macOS SDK is **not vendored, not fetched, and not redistributable** — its
licence restricts use to Apple hardware. Having one is the exception, not the
rule. `check.sh` looks for it in this order:

1. `--sdk PATH`, or `$SP_MACOS_SDK`
2. `xcrun --sdk macosx --show-sdk-path` — i.e. the SDK installed on a real Mac
3. `runtime/toolchains/src/MacOSX.sdk` — a copy dropped in by hand

which is the same ladder `target-apple/init-target.mk` and
`common/configure.mk` use to resolve `SP_MACOS_SDK` for the build itself. With
none of them present the script prints `SKIP` and exits **0**, so it is safe to
run anywhere. A `$SP_MACOS_SDK` that is set but wrong is an error rather than a
silent fallback to a different SDK.

The `+open` half additionally needs `runtime/toolchains/targets/<arch>-apple-macosx+open`
to be installed; without it that half is skipped and the rest still runs.

## What is checked

### 1. sprt's tables against a sysroot — `check-*.cpp`

`static_assert`s pinning `runtime/include/sprt/c/cross/macos_sprt/**` and
`sprt/c/sys/__sprt_darwin.h` against the real Darwin headers. Each TU is
compiled **four times**: `{x86_64, arm64}` × `{the real SDK, the +open sysroot}`
— against the SDK this validates sprt, against `+open` it validates sprt against
what builds actually use.

| TU | pins |
|---|---|
| `check-errno.cpp` | all 108 Darwin errno numbers (`EDEADLK` 11, `EAGAIN` 35 — the reverse of Linux) |
| `check-signal.cpp` | `SIG*` incl. `SIGEMT`/`SIGINFO`, `NSIG`, `sigset_t` |
| `check-fcntl.cpp` | `O_*`/`F_*`/`AT_*`, per architecture |
| `check-socket.cpp` | `AF_`/`PF_`/`SOCK_`/`SO_`/`MSG_`/`SCM_`/`NET_RT_` (`AF_INET6` 30, `SOL_SOCKET` 0xffff) |
| `check-netinet.cpp` | `IPPROTO_`/`IP_`/`IPV6_`/`MCAST_`/`TCP_` setsockopt options |
| `check-types.cpp` | struct layouts that cross the ABI unrepacked |
| `check-mman.cpp` | `PROT_`/`MAP_`/`MS_`/`MADV_` (`MAP_ANON` 0x1000, not Linux's 0x20) |
| `check-poll.cpp` | `POLL*` and `struct pollfd` |
| `check-sysconf.cpp` | the `_SC_*` selector numbers |
| `check-locale.cpp` | `LC_*`, the `newlocale` masks, `nl_langinfo` items, Darwin runetype bits |
| `check-stdio.cpp` | stdio limits, `setvbuf` modes, `jmp_buf`, `fenv` |
| `check-time.cpp` | `CLOCK_*`, `timespec`, `timeval` |
| `check-darwin.cpp` | `os_unfair_lock`, the `os_sync_*` futex API, CoreFoundation types and `CFRunLoop` |

Two things make this stricter than it looks.

**Include order is the opposite of the Windows harness, and it is load-bearing.**
The system headers are included *first*, before `abi_check.h`. sprt's tables
carry an unprefixed alias block (`#ifndef EPERM` / `#define EPERM __SPRT_EPERM`)
so they can stand in for the platform header on a freestanding target; if sprt
came first, that block would define the bare names itself and every assert would
decay into `__SPRT_X == __SPRT_X` — passing even for a name Darwin does not have.
That is not hypothetical: it is how `__SPRT_ENOTCAPABLE` (107, past Darwin's
`ELAST` 106) sailed through the first draft. With the system headers first, a
bare name in an assert is always the platform's, and the contract becomes
two-directional:

| `__SPRT_X` | Darwin `X` | result |
|---|---|---|
| yes | yes | values compared |
| yes | no | `error: use of undeclared identifier 'X'` — the table invents a name |
| no | yes | `error: use of undeclared identifier '__SPRT_X'` — the table is missing one |

`-Werror=macro-redefined` backs this up: an sprt header whose alias block lost
its guard cannot quietly take a bare name back.

**Function types are compared, not just constants.** `check-darwin.cpp` uses
`SPRT_SIGNATURE` to compare sprt's hand-written prototypes against libSystem's
after normalising both to their ABI shape (an enum becomes its underlying type,
any object pointer becomes `void *`). That tolerates sprt spelling
`os_sync_wait_on_address_flags_t` as `unsigned int` and `CFRunLoopRef` as
`void *` — neither changes how an argument is passed — while still catching a
parameter widened from `uint32_t` to `size_t` or a changed return type.

### 2. The `+open` sysroot against the real SDK — `probe/`

The two header sets spell the same names, so they cannot share a translation
unit the way the `check-*.cpp` files can. Instead each probe is compiled **twice**
— once per sysroot — and the emitted values are diffed. Each entry becomes an
`extern "C" __attribute__((used))` constant whose initialiser lands in the LLVM
IR, so no target code ever runs. A name present in only one sysroot fails to
compile there, which is itself the finding.

- **`probe-posix.c`** — 326 values: errno, fcntl, signal, socket, netinet, mman,
  poll, kqueue, termios, sysconf, mach, plus `sizeof`/`alignof`/`offsetof` for
  `struct stat`, `dirent`, `kevent`, `statfs`, `termios`, `rusage`, the socket
  structs and the pthread types.
- **`probe-frameworks.mm`** — 973 enumerators across AppKit, Metal, QuartzCore,
  CoreGraphics, Foundation, CoreFoundation, CoreServices and Network.
  **Generated** by `gen-probe-frameworks.py` from the overlay headers themselves,
  so coverage cannot rot: adding a declaration to the overlay and not
  regenerating is reported by `check.sh` as a failure.
- **`probe-objc-encodings.mm`** — 61 Objective-C property and method *types*,
  via `@encode(__typeof__(expr))` on a `nil`-typed receiver (a compile-time
  string; no message is ever sent). This is the half no value check can see: a
  property declared `CGFloat` where AppKit has `NSInteger` compiles fine on
  `+open` and misbehaves on a real Mac. Unlike the value probes this one is
  deliberately **one-directional** — the overlay is allowed to be narrower than
  AppKit, it just may not contradict it.

`SYS_*` syscall numbers are not duplicated here:
`target-apple/gen-syscall-header.sh` already takes an SDK argument and verifies
its generated `sys/syscall.h` against the SDK's.

### 3. The `.tbd` link stubs against the SDK's — `tbd-audit.py`

Indexes every symbol in the SDK's 2038 `.tbd` files (≈495k symbols), folding
`usr/lib/system/*` into the libSystem umbrella the way the linker does, and
expanding `objc-classes:` / `objc-ivars:` / `objc-eh-types:` into their real
`_OBJC_CLASS_$_`-style spellings. Then, for each of the 10913 symbols the
`+open` stubs export, it checks that

1. the symbol exists in the SDK at all, and
2. **the same library owns it**.

(2) is the one that matters and the one prose cannot keep honest: a symbol in
the wrong stub links fine here but records a load command naming a dylib that
does not export it, and fails at launch on a real Mac.

The audit runs over the git-tracked `open/sysroot` *and* each installed
`targets/<triple>+open`, and reports when they differ — the stubs travel
`open/sysroot → intermediate → targets`, and nothing re-runs those stages until
the target is rebuilt, so a re-bake can sit unpublished. `check.sh` does the same
comparison for the overlay's framework headers.

`libc++.tbd` / `libc++abi.tbd` / `libunwind.tbd` are excluded: they are this
toolchain's own cross-built products, generated by `llvm-readtapi -stubify` and
never baked, so comparing them against Apple's would be comparing two different
implementations.

Waivers live in `tbd-exceptions.txt`, one reason per line. Only two things are
waivable — a symbol genuinely newer than the pinned SDK, or an ownership choice
that is understood and deliberate — and both current entries are documented in
place.

## Deliberate omissions

Everything the harness cannot pin is named, with the reason, next to the checks:

- **`struct stat`, `struct dirent`, `struct statvfs`** are *not* layout-asserted
  against Darwin's. sprt defines its own (`bits/stat_data.h` is 120 bytes against
  Darwin's 144) and converts explicitly in `convertStatFromNative()`
  (`libc_wrapper/sys/SPRuntimeCSysStat.cpp`). Asserting equality would be
  asserting the wrong thing; those paths need field-level round-trip coverage,
  which is the runtime test suite's job. `probe-posix.c` still pins the *native*
  layouts across the two sysroots.
- **`fd_set`** deliberately diverges: sprt uses 64-bit `fds_bits` words (as glibc
  does), Darwin uses 32-bit. `check-types.cpp` pins the invariant that actually
  has to hold — same size, and a little-endian target, which is what makes the
  two bit-compatible — instead of an alignment equality that would be false.
- **`SIG_DFL`/`SIG_IGN`/`SIG_HOLD`/`SIG_ERR`** are function-pointer sentinels; an
  int-to-pointer cast is not a constant expression, so they cannot be
  `static_assert`ed (the same limit that stops the Windows harness pinning
  `_CRTDBG_FILE_STDOUT`).
- **Kernel-only names** — `MSG_USEUPCALL`, the `F_WAIT`/`F_FLOCK`/`F_POSIX`/…
  flock flags — are `#ifdef KERNEL` in xnu and stripped from the published SDK.
  They are the few places the `+open` sysroot (verbatim xnu) is legitimately
  wider than the SDK.
- **Names newer than the pinned SDK** — the macOS 15 `O_RESOLVE_BENEATH`/
  `AT_NODELETEBUSY`/… bits, the `OS_UNFAIR_LOCK_FLAG_*` API, Metal 3.2's
  `MTLMathMode*`. Their values cannot be confirmed against a 14.5 SDK. Where a
  weaker invariant still exists it is asserted anyway: `check-fcntl.cpp` pins
  that none of the macOS 15 `O_*`/`AT_*` bits collides with a flag 14.5 already
  defines, which is what would actually corrupt a syscall.

## Scope

This harness proves that values, layouts, signatures and symbol ownership agree.
It does **not** prove that a hand-written framework header declares every method
real code needs — that remains covered by actually building
`runtime/window/macos` against `+open`.
