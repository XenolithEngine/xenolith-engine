// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// SIG_DFL/SIG_IGN/SIG_ERR are 0/1/-1 for us as for Linux. embox_sprt has to
// override them to Embox's 0x1/0x3/0x5 sentinels (and to collapse SIG_SETMASK
// onto SIG_UNBLOCK); none of that applies once the signal state is ours.
#include <sprt/c/cross/linux_sprt/signal.h>

// Two sentinels linux_sprt does not carry, because glibc's signal() has no use
// for them: SIG_GET queries the current handler and SIG_ACK acknowledges one.
// The FREESTANDING signal() (libc_impl/src/builtin_signal.cpp) compares against
// both unconditionally, so a libc_impl target must define them -- which is
// exactly what wasm_sprt does, and for the same reason. Values are distinct from
// SIG_DFL(0)/SIG_IGN(1)/SIG_ERR(-1) above.
#define __SPRT_SIG_GET ((void (*)(int))2)
#define __SPRT_SIG_ACK ((void (*)(int))4)
