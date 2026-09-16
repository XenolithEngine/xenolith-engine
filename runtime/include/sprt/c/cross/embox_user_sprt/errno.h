#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_ERRNO_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_ERRNO_H_

// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// This is the one cross header where the divergence is not theoretical:
// embox_sprt/errno.h carries Embox's native codes (EPERM 1001, EACCES 301,
// and an ENOTEMPTY of 66 that Linux spells EREMOTE). The kernel's
// xl_errno.c maps those onto the values below before they cross into EL0, so
// user code sees Linux's numbering and nothing else.
#include <sprt/c/cross/linux_sprt/errno.h>

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_ERRNO_H_
