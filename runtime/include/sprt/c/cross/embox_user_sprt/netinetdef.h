#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_NETINETDEF_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_NETINETDEF_H_

// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// No socket syscall is implemented yet (M3). When they arrive they arrive as
// the generic-ABI numbers 198+, carrying Linux's option tables - not the
// Embox-native ones in embox_sprt/netinetdef.h, which exist only because that
// target hands setsockopt() straight to the Embox libc.
#include <sprt/c/cross/linux_sprt/netinetdef.h>

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_NETINETDEF_H_
