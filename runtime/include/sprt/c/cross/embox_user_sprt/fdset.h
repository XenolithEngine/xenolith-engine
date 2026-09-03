// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// fd_set never crosses the boundary - no select/pselect6 syscall is
// implemented (and none is planned before M3) - so this is purely our own
// libc's shape, and there is no reason for it not to be the usual one.
#include <sprt/c/cross/linux_sprt/fdset.h>
