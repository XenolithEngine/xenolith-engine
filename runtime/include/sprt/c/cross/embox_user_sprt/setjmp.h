// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// setjmp/longjmp are entirely inside the process - musl's aarch64 pair, whose
// buffer is the 22-word __jmp_buf plus the mask, exactly linux_sprt's shape.
#include <sprt/c/cross/linux_sprt/setjmp.h>
