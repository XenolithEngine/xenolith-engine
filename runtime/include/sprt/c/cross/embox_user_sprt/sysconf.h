// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// sysconf()/pathconf() are answered inside our libc, so the _SC_*/_PC_*
// numbering is a free choice and the portable one is right. embox_sprt must
// use Embox's (its _SC_PAGESIZE is 1, not 30) only because it forwards the
// name argument to the Embox libc.
#include <sprt/c/cross/linux_sprt/sysconf.h>
