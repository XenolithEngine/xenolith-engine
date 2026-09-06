// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// FP_* classification numbering belongs to whoever implements fpclassify,
// and here that is our own libc (musl's), not Embox's math module. So unlike
// embox_sprt/math.h - which has to mirror Embox's swapped NORMAL/ZERO - this
// one takes the glibc/musl numbering unchanged.
#include <sprt/c/cross/linux_sprt/math.h>
