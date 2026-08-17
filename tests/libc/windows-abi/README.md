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

Three POSIX names in `windows_sprt/sockdef.h` have no Winsock constant to be pinned
against, and each is handled on its own terms instead:

- `SOCK_CLOEXEC` / `SOCK_NONBLOCK` — Linux `socket()` / `accept4()` type bits that the
  shims mask out of the type and apply through `FIONBIO`. What is asserted is the
  property the masking relies on: that neither collides with a Winsock `SOCK_` type.
- `MSG_NOSIGNAL` — a request to suppress a signal Windows does not have, so it is 0,
  and `check.cpp` asserts that it stays 0. The flag word goes to Winsock untouched,
  and a private bit in it would be one Winsock never defined.

`SO_REUSEPORT`, `MSG_DONTWAIT` and `MSG_EOR` used to sit in the same list as "emulated"
without an emulation behind them. Winsock has no shared listening port, no per-call
non-blocking flag and no record boundaries, so they are simply absent from the Windows
table now, and `<sys/socket.h>` guards them like every other optional name.

`SOL_IP` / `SOL_IPV6` are Linux spellings of the per-protocol `setsockopt()` level, which
Winsock only spells `IPPROTO_IP` / `IPPROTO_IPV6`; they are pinned against those.

`PF_LINK` / `PF_HYPERV` have no Winsock `PF_` spelling, so they are pinned against the
`AF_` spelling they alias.

`IPPROTO_*` are a Winsock *enum* (not macros), so the standard ones are asserted
directly while Linux-only protocol numbers (`IPPROTO_MPTCP`, `IPPROTO_BEETPH`, …)
are omitted.

## The netinet table is checked in both directions

The netinet asserts are guarded on `defined(__SPRT_X) || defined(X)` — the same
guard the runtime uses for its hosted platforms — so **either** side defining a
name forces the assert to compile:

| `__SPRT_X` | Winsock `X` | result |
|---|---|---|
| yes | yes | values compared |
| yes | no  | `error: use of undeclared identifier 'X'` — the table carries a name Winsock does not have |
| no  | yes | `error: use of undeclared identifier '__SPRT_X'` — the table is missing an option Winsock has |
| no  | no  | skipped |

so `windows_sprt/netinetdef.h` must carry Winsock's option surface **exactly**.
That is not pedantry: a defined name is a promise, and portable code takes it at
face value. curl's `cf-socket.c` does

```c
#ifdef IP_BIND_ADDRESS_NO_PORT
  (void)setsockopt(sockfd, SOL_IP, IP_BIND_ADDRESS_NO_PORT, &on, sizeof(on));
#endif
```

— no `__linux__` guard, the macro *is* the feature test. While the table still
carried the Linux number for it, that line broke the Windows curl build outright
(`int *` vs winsock's `const char *optval`), and had it compiled it would have
set `IP_RECVIF`, which is what option 24 means on Windows.

Linux-only options (`IP_FREEBIND`, `IP_TRANSPARENT`, the `IP_PMTUDISC_*` /
`IPV6_PMTUDISC_*` families, `IPV6_AUTOFLOWLABEL`, `MCAST_MSFILTER`, …) are
therefore absent from the Windows table, and that absence is what makes them skip
here. The Linux/Android/macOS tables keep them; those targets validate the same
way against their own `<netinet/in.h>` in `SPRuntimeCSysSocket.cpp`.
