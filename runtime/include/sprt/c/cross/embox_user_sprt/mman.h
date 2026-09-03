// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// PROT_*/MAP_* are arguments to mmap(222) and mprotect(226) and are read by
// the kernel's xl_mm.c as Linux values. embox_sprt/mman.h renumbers them to
// Embox's packed layout precisely because THAT path forwards to the Embox
// libc; this one must not.
#include <sprt/c/cross/linux_sprt/mman.h>
