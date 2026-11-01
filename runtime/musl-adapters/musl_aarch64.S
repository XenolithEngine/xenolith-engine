/* musl's hand-written aarch64 assembly, for the freestanding aarch64 targets
 * (Embox EL0 and Tau): setjmp/longjmp, for which libc_impl has no aarch64 asm
 * directory. The string functions and the fenv primitives come from
 * runtime/libc_freestanding.
 *
 * Conditioned on predefines only -- no #include of the sprt platform header.
 * Assembly is compiled with its own flag set (TARGET_GENERAL_SFLAGS), which
 * carries the target define but not the C include paths, so a header lookup here
 * would fail. The relative includes below resolve against this file.
 */

#if defined(__aarch64__) && (defined(__EMBOX_USER__) || defined(__TAU__))

#if defined(__TAU__)
/* musl's pair with x18, the shadow call stack (xenolith-tau RT-A-ShadowStack-1), in the word
 * after the 22 of __jb (__fl, free: sigsetjmp is setjmp on Tau). Without it a longjmp would leave
 * the shadow stack as deep as the frame it left. */
	.global __setjmp
	.global _setjmp
	.global setjmp
	.type __setjmp,@function
	.type _setjmp,@function
	.type setjmp,@function
__setjmp:
_setjmp:
setjmp:
	stp	x19, x20, [x0,#0]
	stp	x21, x22, [x0,#16]
	stp	x23, x24, [x0,#32]
	stp	x25, x26, [x0,#48]
	stp	x27, x28, [x0,#64]
	stp	x29, x30, [x0,#80]
	mov	x2, sp
	str	x2, [x0,#104]
	stp	d8, d9, [x0,#112]
	stp	d10, d11, [x0,#128]
	stp	d12, d13, [x0,#144]
	stp	d14, d15, [x0,#160]
	str	x18, [x0,#176]
	mov	x0, #0
	ret

	.global _longjmp
	.global longjmp
	.type _longjmp,%function
	.type longjmp,%function
_longjmp:
longjmp:
	ldp	x19, x20, [x0,#0]
	ldp	x21, x22, [x0,#16]
	ldp	x23, x24, [x0,#32]
	ldp	x25, x26, [x0,#48]
	ldp	x27, x28, [x0,#64]
	ldp	x29, x30, [x0,#80]
	ldr	x2, [x0,#104]
	mov	sp, x2
	ldp	d8, d9, [x0,#112]
	ldp	d10, d11, [x0,#128]
	ldp	d12, d13, [x0,#144]
	ldp	d14, d15, [x0,#160]
	ldr	x18, [x0,#176]
	cmp	w1, 0
	csinc	w0, w1, wzr, ne
	br	x30
#else
#include "../musl-libc/src/setjmp/aarch64/setjmp.s"
#include "../musl-libc/src/setjmp/aarch64/longjmp.s"
#endif

#endif
