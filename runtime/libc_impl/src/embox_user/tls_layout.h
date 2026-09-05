
// Where the executable's TLS image sits relative to the thread pointer.
//
// A header of its own, for one four-line function, because this number cannot be
// checked by running the code: getting it wrong does not fault, it silently reads
// and writes the wrong object. What CAN check it is the linker -- every
// :tprel_hi12:/:tprel_lo12_nc: pair it emits encodes the same offset -- so the
// formula is kept here where a test can compile against it and compare.
// xenolith-os scripts/check-libc-embox-user.sh does exactly that.
//
// aarch64 is TLS variant I: TPIDR_EL0 points at the thread pointer, the first 16
// bytes above it are the reserved TCB, and the image begins at the next address
// aligned to the segment's own p_align. Note the shape: it is align_up(16,
// p_align), NOT 16 -- a PT_TLS whose p_align exceeds 16 (any thread_local with
// alignas(32) or wider) pushes the whole image further out.

#ifndef RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_TLS_LAYOUT_H_
#define RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_TLS_LAYOUT_H_

// Reserved thread control block between the thread pointer and the image.
#define __SPRT_EL0_TCB_SIZE 16

// Offset of the first byte of the PT_TLS image above TPIDR_EL0, for a segment
// with the given p_align. A p_align of 0 or 1 means "no constraint".
//
// A macro rather than an inline function so that it can appear in a
// _Static_assert: that is how the check compares it against the linker's answer,
// and a `static inline` is not a constant expression in C11. Align is evaluated
// twice, which is safe because it is always a plain value here (p_align, or a
// literal in the test).
#define __el0_tls_gap(Align) \
	((unsigned long)(Align) > 1UL \
					? (((unsigned long)__SPRT_EL0_TCB_SIZE + (unsigned long)(Align) - 1UL) \
							  & ~((unsigned long)(Align) - 1UL)) \
					: (unsigned long)__SPRT_EL0_TCB_SIZE)

#endif // RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_TLS_LAYOUT_H_
