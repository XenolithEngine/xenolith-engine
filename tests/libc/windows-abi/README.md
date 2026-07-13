# windows-abi — compile-time WINAPI parity for the `__SPRT_*` socket + netinet constants

On the Windows target the SPRT socket shims (`runtime/libc_wrapper/sys/SPRuntimeCSysSocket.cpp`)
forward constants straight through to native Winsock with **no translation**, e.g.

```c
::send(fd, buf, n, __flags);                       // __flags carries __SPRT_MSG_* bits
::socket(__SPRT_AF_INET, __SPRT_SOCK_STREAM, 0);
::setsockopt(fd, IPPROTO_IPV6, __SPRT_IPV6_V6ONLY, ...);
```

So every `__SPRT_*` value that reaches Winsock must equal the value in the real
Windows SDK header. The runtime's own `static_assert`s only compile in the
*hosted* branch of that file and only validate the **Linux** values, so they
never see Winsock's numbers.

This subproject closes that gap with two translation units:

[`check.cpp`](check.cpp) `static_assert`s each Windows `__SPRT_*` **constant**
against the identically named constant in the live Windows SDK headers vendored
under `runtime/toolchains/src/xwin/splat`. It covers both tables:

- the core socket constants — `runtime/include/sprt/c/cross/windows_sprt/sockdef.h`
  (SHUT/SOCK/AF/PF/SO/MSG/SOL/SOMAXCONN);
- the cross netinet constants — `runtime/include/sprt/c/cross/__sprt_netinet.h`
  (IPPROTO_/IP_/IPV6_/MCAST_/INET*_ADDRSTRLEN), whose Windows values are selected
  there via `SPRT_WINDOWS`.

[`check-types.cpp`](check-types.cpp) `static_assert`s the SPRT socket **struct
layouts** (`cross/windows_sprt/socket.h`: `sockaddr`, `sockaddr_in`, `in_addr`,
`in6_addr`, `sockaddr_in6`, `linger`, `socklen_t`) — size and field offsets —
against the winsock structs, since the wrappers `bind`/`setsockopt`/… forward
them to Winsock unrepacked. That header is a full winsock replacement (it also
defines `IN_ADDR`, `SOCKET`, `hostent`, `WSADATA`, …), so it cannot share global
scope with `<winsock2.h>`: `__SPRT_BUILD` namespaces the core structs as
`__sprt_*` tags and the include is wrapped in a `namespace` to isolate the helper
types, letting both live in one TU. (This check caught `struct linger` being
declared with POSIX `int` fields instead of winsock's `u_short`.)

It is **compile-time only** — nothing is linked or run. A clean parse means every
checked constant matches; a failing `static_assert` names the diverging constant
and prints both values.

## Run

```sh
./check.sh                 # x86_64-pc-windows-msvc (default)
./check.sh --arch aarch64  # aarch64 SDK headers
./check.sh -v              # print the clang command line
```

Requires `clang` (any recent version; MSVC-target support is built in) and a
populated `splat/` tree — see the `xwin/splat` rule in
`runtime/toolchains/src.mk`. If the SDK is not vendored the script prints `SKIP`
and exits 0.

## Scope / intentional gaps

A handful of `__SPRT_MSG_*` / `__SPRT_SOCK_*` / `__SPRT_SO_*` names are POSIX/Linux
flags that SPRT emulates and that Winsock has no bit for
(`SOCK_CLOEXEC`, `SOCK_NONBLOCK`, `SO_REUSEPORT`, `MSG_DONTWAIT`, `MSG_EOR`,
`MSG_NOSIGNAL`). They have no native counterpart to pin against and are left
unchecked (documented inline in `check.cpp`). `PF_LINK` / `PF_HYPERV` have no
Winsock `PF_` spelling, so they are pinned against the `AF_` spelling they alias.

The netinet asserts are `#ifdef`-guarded on the Winsock name, so Linux-only
options that Winsock lacks (e.g. `IP_FREEBIND`, `IP_TRANSPARENT`, the
`IP_PMTUDISC_*` / `IPV6_PMTUDISC_*` families, `IPV6_AUTOFLOWLABEL`,
`MCAST_MSFILTER`) are skipped automatically. `IPPROTO_*` are a Winsock *enum*
(not macros), so the standard ones are asserted directly while Linux-only
protocol numbers (`IPPROTO_MPTCP`, `IPPROTO_BEETPH`, …) are omitted.
