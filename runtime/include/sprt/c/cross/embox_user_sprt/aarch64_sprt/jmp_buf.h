// The EL0 boundary IS the Linux/aarch64 generic ABI (xenolith-os
// docs/EMBOX-SYSCALL-ABI.md, decision D1): the kernel-side dispatcher translates
// Embox's own numbering into these values, and its translation tables are
// GENERATED from these very files by xenolith-os scripts/gen-abi-tables.py.
// Forwarding, rather than copying the numbers, is what keeps both sides of the
// boundary reading from one source; a copy here would drift silently, and the
// failure mode is not a build error but a syscall that quietly does something
// else.
//
// musl's aarch64 __jmp_buf: 22 unsigned longs (x19-x28, fp, lr, sp, d8-d15).
// Embox's own is 12 words, which is why embox_sprt cannot share this - but
// our setjmp is musl's, so ours is musl's.
#include <sprt/c/cross/linux_sprt/aarch64_sprt/jmp_buf.h>
