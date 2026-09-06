// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// O_*/F_*/AT_* are arguments to openat(56) and are read by the kernel's
// xl_flags.c as Linux values - that table is generated from THIS file's
// target. embox_sprt/aarch64_sprt/fcntl.h spells out Embox's own layout
// (O_CREAT 0x100 against Linux's 0100) because it forwards untranslated;
// doing the same here would undo the translation the kernel performs.
#include <sprt/c/cross/linux_sprt/aarch64_sprt/fcntl.h>
