/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

/* loongarch64 LP64D: memcpy memmove memset memcmp bcmp memchr strlen strcmp.
 *
 * The same structure as the aarch64 version: LA64 cores access unaligned memory
 * in hardware, so copies use overlapping head and tail accesses. The SIMD level is
 * LSX (128-bit), on $vr0-$vr7: $vr24-$vr31 hold callee-saved FP registers. */

#include "../../include/sprt_fs_config.h"

#if SPRT_FS_HAVE_ASM && defined(__loongarch64)

#define FS_FUNC(name) \
	.text; \
	.p2align 4; \
	.global SPRT_FS_NAME(name); \
	.type SPRT_FS_NAME(name), @function; \
	SPRT_FS_NAME(name):

#define FS_END(name) .size SPRT_FS_NAME(name), .- SPRT_FS_NAME(name)

/* r gets 0x80 in every byte of x that is zero, exact for the lowest one;
 * with $a6 = 0x0101..01 and $a7 = 0x8080..80 */
#define FS_ZERO(r, x) \
	sub.d r, x, $a6; \
	andn r, r, x; \
	and r, r, $a7

#define FS_ZERO_CONSTS \
	li.d $a6, 0x0101010101010101; \
	li.d $a7, 0x8080808080808080

/* ---- memcpy (a0 dst, a1 src, a2 n) -> a0 dst */

FS_FUNC(memcpy)
.Lcpy_body:
	add.d $a4, $a1, $a2 /* src end */
	add.d $a5, $a0, $a2 /* dst end */
	li.d $t8, 16
	bltu $t8, $a2, .Lcpy_gt16
	li.d $t8, 8
	bltu $a2, $t8, .Lcpy_lt8
	ld.d $t0, $a1, 0
	ld.d $t1, $a4, -8
	st.d $t0, $a0, 0
	st.d $t1, $a5, -8
	jr $ra
.Lcpy_lt8:
	li.d $t8, 4
	bltu $a2, $t8, .Lcpy_lt4
	ld.w $t0, $a1, 0
	ld.w $t1, $a4, -4
	st.w $t0, $a0, 0
	st.w $t1, $a5, -4
	jr $ra
.Lcpy_lt4:
	beqz $a2, 1f
	srli.d $t2, $a2, 1
	ld.bu $t0, $a1, 0
	ldx.bu $t1, $a1, $t2
	ld.bu $t3, $a4, -1
	st.b $t0, $a0, 0
	stx.b $t1, $a0, $t2
	st.b $t3, $a5, -1
1:	jr $ra

#if SPRT_FS_ASM_SIMD
.Lcpy_gt16:
	li.d $t8, 32
	bltu $t8, $a2, .Lcpy_gt32
	vld $vr0, $a1, 0
	vld $vr1, $a4, -16
	vst $vr0, $a0, 0
	vst $vr1, $a5, -16
	jr $ra
.Lcpy_gt32:
	li.d $t8, 64
	bltu $t8, $a2, .Lcpy_gt64
	vld $vr0, $a1, 0
	vld $vr1, $a1, 16
	vld $vr2, $a4, -32
	vld $vr3, $a4, -16
	vst $vr0, $a0, 0
	vst $vr1, $a0, 16
	vst $vr2, $a5, -32
	vst $vr3, $a5, -16
	jr $ra
.Lcpy_gt64:
	/* the last 64 bytes are loaded first and stored last */
	vld $vr4, $a4, -64
	vld $vr5, $a4, -48
	vld $vr6, $a4, -32
	vld $vr7, $a4, -16
	addi.d $t2, $a2, -64
	move $t3, $a0
1:	vld $vr0, $a1, 0
	vld $vr1, $a1, 16
	vld $vr2, $a1, 32
	vld $vr3, $a1, 48
	vst $vr0, $t3, 0
	vst $vr1, $t3, 16
	vst $vr2, $t3, 32
	vst $vr3, $t3, 48
	addi.d $a1, $a1, 64
	addi.d $t3, $t3, 64
	addi.d $t2, $t2, -64
	blt $zero, $t2, 1b
	vst $vr4, $a5, -64
	vst $vr5, $a5, -48
	vst $vr6, $a5, -32
	vst $vr7, $a5, -16
	jr $ra
#else
.Lcpy_gt16:
	li.d $t8, 32
	bltu $t8, $a2, .Lcpy_gt32
	ld.d $t0, $a1, 0
	ld.d $t1, $a1, 8
	ld.d $t2, $a4, -16
	ld.d $t3, $a4, -8
	st.d $t0, $a0, 0
	st.d $t1, $a0, 8
	st.d $t2, $a5, -16
	st.d $t3, $a5, -8
	jr $ra
.Lcpy_gt32:
	/* the last 32 bytes are loaded first and stored last */
	ld.d $t4, $a4, -32
	ld.d $t5, $a4, -24
	ld.d $t6, $a4, -16
	ld.d $t7, $a4, -8
	addi.d $a3, $a2, -32
	move $a6, $a0
1:	ld.d $t0, $a1, 0
	ld.d $t1, $a1, 8
	ld.d $t2, $a1, 16
	ld.d $t3, $a1, 24
	st.d $t0, $a6, 0
	st.d $t1, $a6, 8
	st.d $t2, $a6, 16
	st.d $t3, $a6, 24
	addi.d $a1, $a1, 32
	addi.d $a6, $a6, 32
	addi.d $a3, $a3, -32
	blt $zero, $a3, 1b
	st.d $t4, $a5, -32
	st.d $t5, $a5, -24
	st.d $t6, $a5, -16
	st.d $t7, $a5, -8
	jr $ra
#endif
FS_END(memcpy)

/* ---- memmove (a0 dst, a1 src, a2 n) -> a0 dst */

#if SPRT_FS_ASM_SIMD
#define FS_SMALL 64
#else
#define FS_SMALL 32
#endif

FS_FUNC(memmove)
	sub.d $t0, $a0, $a1
	bgeu $t0, $a2, .Lcpy_body /* dst below src, or no overlap: forward */
	li.d $t8, FS_SMALL
	bgeu $t8, $a2, .Lcpy_body /* short: everything is loaded before anything is stored */
	/* backward: the first bytes are loaded first and stored last */
	add.d $a4, $a1, $a2
	add.d $a5, $a0, $a2
#if SPRT_FS_ASM_SIMD
	vld $vr4, $a1, 0
	vld $vr5, $a1, 16
	vld $vr6, $a1, 32
	vld $vr7, $a1, 48
	addi.d $t2, $a2, -64
1:	vld $vr0, $a4, -64
	vld $vr1, $a4, -48
	vld $vr2, $a4, -32
	vld $vr3, $a4, -16
	vst $vr3, $a5, -16
	vst $vr2, $a5, -32
	vst $vr1, $a5, -48
	vst $vr0, $a5, -64
	addi.d $a4, $a4, -64
	addi.d $a5, $a5, -64
	addi.d $t2, $t2, -64
	blt $zero, $t2, 1b
	vst $vr4, $a0, 0
	vst $vr5, $a0, 16
	vst $vr6, $a0, 32
	vst $vr7, $a0, 48
#else
	ld.d $t4, $a1, 0
	ld.d $t5, $a1, 8
	ld.d $t6, $a1, 16
	ld.d $t7, $a1, 24
	addi.d $a3, $a2, -32
1:	ld.d $t0, $a4, -32
	ld.d $t1, $a4, -24
	ld.d $t2, $a4, -16
	ld.d $t3, $a4, -8
	st.d $t3, $a5, -8
	st.d $t2, $a5, -16
	st.d $t1, $a5, -24
	st.d $t0, $a5, -32
	addi.d $a4, $a4, -32
	addi.d $a5, $a5, -32
	addi.d $a3, $a3, -32
	blt $zero, $a3, 1b
	st.d $t4, $a0, 0
	st.d $t5, $a0, 8
	st.d $t6, $a0, 16
	st.d $t7, $a0, 24
#endif
	jr $ra
FS_END(memmove)

/* ---- memset (a0 dst, a1 c, a2 n) -> a0 dst */

FS_FUNC(memset)
	andi $a1, $a1, 0xff
	li.d $t0, 0x0101010101010101
	mul.d $a1, $a1, $t0
	add.d $a4, $a0, $a2 /* end */
	li.d $t8, 16
	bltu $t8, $a2, .Lset_gt16
	li.d $t8, 8
	bltu $a2, $t8, .Lset_lt8
	st.d $a1, $a0, 0
	st.d $a1, $a4, -8
	jr $ra
.Lset_lt8:
	li.d $t8, 4
	bltu $a2, $t8, .Lset_lt4
	st.w $a1, $a0, 0
	st.w $a1, $a4, -4
	jr $ra
.Lset_lt4:
	beqz $a2, 1f
	st.b $a1, $a0, 0
	st.b $a1, $a4, -1
	li.d $t8, 2
	bltu $a2, $t8, 1f
	st.b $a1, $a0, 1
1:	jr $ra

#if SPRT_FS_ASM_SIMD
.Lset_gt16:
	vreplgr2vr.d $vr0, $a1
	li.d $t8, 32
	bltu $t8, $a2, .Lset_gt32
	vst $vr0, $a0, 0
	vst $vr0, $a4, -16
	jr $ra
.Lset_gt32:
	li.d $t8, 64
	bltu $t8, $a2, .Lset_gt64
	vst $vr0, $a0, 0
	vst $vr0, $a0, 16
	vst $vr0, $a4, -32
	vst $vr0, $a4, -16
	jr $ra
.Lset_gt64:
	vst $vr0, $a4, -64
	vst $vr0, $a4, -48
	vst $vr0, $a4, -32
	vst $vr0, $a4, -16
	addi.d $t2, $a2, -64
	move $t3, $a0
1:	vst $vr0, $t3, 0
	vst $vr0, $t3, 16
	vst $vr0, $t3, 32
	vst $vr0, $t3, 48
	addi.d $t3, $t3, 64
	addi.d $t2, $t2, -64
	blt $zero, $t2, 1b
	jr $ra
#else
.Lset_gt16:
	li.d $t8, 32
	bltu $t8, $a2, .Lset_gt32
	st.d $a1, $a0, 0
	st.d $a1, $a0, 8
	st.d $a1, $a4, -16
	st.d $a1, $a4, -8
	jr $ra
.Lset_gt32:
	st.d $a1, $a4, -32
	st.d $a1, $a4, -24
	st.d $a1, $a4, -16
	st.d $a1, $a4, -8
	addi.d $t2, $a2, -32
	move $t3, $a0
1:	st.d $a1, $t3, 0
	st.d $a1, $t3, 8
	st.d $a1, $t3, 16
	st.d $a1, $t3, 24
	addi.d $t3, $t3, 32
	addi.d $t2, $t2, -32
	blt $zero, $t2, 1b
	jr $ra
#endif
FS_END(memset)

/* ---- memcmp, bcmp (a0 a, a1 b, a2 n) -> a0 */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), @function
SPRT_FS_NAME(bcmp):
	li.d $t8, 8
	bltu $a2, $t8, .Lcmp_lt8
	li.d $t8, 16
	bltu $a2, $t8, .Lcmp_8_15
	/* 16 bytes a step, then the last 16 overlapping the ones compared */
	add.d $a4, $a0, $a2
	addi.d $a4, $a4, -16 /* the last block of a */
	add.d $a5, $a1, $a2
	addi.d $a5, $a5, -16 /* the last block of b */
#if SPRT_FS_ASM_SIMD
	b 2f
1:	vld $vr0, $a0, 0
	vld $vr1, $a1, 0
	vseq.b $vr2, $vr0, $vr1
	vmsknz.b $vr2, $vr2
	vpickve2gr.hu $t0, $vr2, 0
	li.d $t8, 0xffff
	bne $t0, $t8, 3f
	addi.d $a0, $a0, 16
	addi.d $a1, $a1, 16
2:	bltu $a0, $a4, 1b
	move $a0, $a4
	move $a1, $a5
	vld $vr0, $a0, 0
	vld $vr1, $a1, 0
	vseq.b $vr2, $vr0, $vr1
	vmsknz.b $vr2, $vr2
	vpickve2gr.hu $t0, $vr2, 0
	li.d $t8, 0xffff
	bne $t0, $t8, 3f
	move $a0, $zero
	jr $ra
	/* the first byte that is not equal */
3:	xor $t0, $t0, $t8
	ctz.d $t0, $t0
	ldx.bu $t1, $a0, $t0
	ldx.bu $t2, $a1, $t0
	sub.d $a0, $t1, $t2
	jr $ra
#else
	b 2f
1:	ld.d $t0, $a0, 0
	ld.d $t2, $a1, 0
	bne $t0, $t2, .Lcmp_word_diff
	ld.d $t0, $a0, 8
	ld.d $t2, $a1, 8
	bne $t0, $t2, .Lcmp_word_diff
	addi.d $a0, $a0, 16
	addi.d $a1, $a1, 16
2:	bltu $a0, $a4, 1b
	ld.d $t0, $a4, 0
	ld.d $t2, $a5, 0
	bne $t0, $t2, .Lcmp_word_diff
	ld.d $t0, $a4, 8
	ld.d $t2, $a5, 8
	bne $t0, $t2, .Lcmp_word_diff
	move $a0, $zero
	jr $ra
#endif
.Lcmp_8_15:
	ld.d $t0, $a0, 0
	ld.d $t2, $a1, 0
	bne $t0, $t2, .Lcmp_word_diff
	add.d $a4, $a0, $a2
	add.d $a5, $a1, $a2
	ld.d $t0, $a4, -8
	ld.d $t2, $a5, -8
	bne $t0, $t2, .Lcmp_word_diff
	move $a0, $zero
	jr $ra
.Lcmp_word_diff:
	/* in memory order, the first differing byte decides */
	revb.d $t0, $t0
	revb.d $t2, $t2
	sltu $t3, $t0, $t2
	sub.d $a0, $zero, $t3
	ori $a0, $a0, 1
	jr $ra
.Lcmp_lt8:
	li.d $t8, 4
	bltu $a2, $t8, .Lcmp_lt4
	ld.wu $t0, $a0, 0
	ld.wu $t2, $a1, 0
	bne $t0, $t2, .Lcmp_dword_diff
	add.d $a4, $a0, $a2
	add.d $a5, $a1, $a2
	ld.wu $t0, $a4, -4
	ld.wu $t2, $a5, -4
	bne $t0, $t2, .Lcmp_dword_diff
	move $a0, $zero
	jr $ra
.Lcmp_dword_diff:
	revb.2w $t0, $t0
	revb.2w $t2, $t2
	sltu $t3, $t0, $t2
	sub.d $a0, $zero, $t3
	ori $a0, $a0, 1
	jr $ra
.Lcmp_lt4:
	/* bytes 0, n/2 and n-1 cover 1..3 bytes in order: compare them as one number */
	beqz $a2, 1f
	srli.d $t4, $a2, 1
	addi.d $t5, $a2, -1
	ld.bu $t0, $a0, 0
	ld.bu $t2, $a1, 0
	ldx.bu $t1, $a0, $t4
	ldx.bu $t3, $a1, $t4
	slli.d $t0, $t0, 16
	slli.d $t2, $t2, 16
	slli.d $t1, $t1, 8
	slli.d $t3, $t3, 8
	or $t0, $t0, $t1
	or $t2, $t2, $t3
	ldx.bu $t1, $a0, $t5
	ldx.bu $t3, $a1, $t5
	or $t0, $t0, $t1
	or $t2, $t2, $t3
	sub.d $a0, $t0, $t2
	jr $ra
1:	move $a0, $zero
	jr $ra
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (a0 s, a1 c, a2 n) -> a0 */

FS_FUNC(memchr)
	beqz $a2, .Lchr_null
	/* a3: the end, saturated when s + n wraps */
	add.d $a3, $a0, $a2
	bgeu $a3, $a0, 1f
	addi.d $a3, $zero, -1
1:	andi $a1, $a1, 0xff
#if SPRT_FS_ASM_SIMD
	vreplgr2vr.b $vr0, $a1
	andi $t0, $a0, 15
	bstrins.d $a0, $zero, 3, 0 /* the aligned block */
	vld $vr1, $a0, 0
	vseq.b $vr1, $vr1, $vr0
	vmsknz.b $vr1, $vr1
	vpickve2gr.hu $t1, $vr1, 0
	srl.d $t1, $t1, $t0 /* the bytes before s */
	beqz $t1, 2f
	ctz.d $t1, $t1
	add.d $t1, $t1, $t0
	add.d $t1, $a0, $t1
	bgeu $t1, $a3, .Lchr_null
	move $a0, $t1
	jr $ra
2:	addi.d $a0, $a0, 16
	/* one more block up to a 32-byte boundary: a pair read from there is never
	 * split between pages, so reading past a match cannot fault */
	andi $t0, $a0, 31
	beqz $t0, 3f
	bgeu $a0, $a3, .Lchr_null
	vld $vr1, $a0, 0
	vseq.b $vr1, $vr1, $vr0
	vmsknz.b $vr1, $vr1
	vpickve2gr.hu $t1, $vr1, 0
	bnez $t1, .Lchr_found
	addi.d $a0, $a0, 16
	/* two blocks at a time while both start before the end; the blocks before
	 * may already have passed it */
3:	bgeu $a0, $a3, .Lchr_null
	sub.d $t0, $a3, $a0
	li.d $t8, 32
	bgeu $t8, $t0, 4f
	vld $vr1, $a0, 0
	vld $vr2, $a0, 16
	vseq.b $vr1, $vr1, $vr0
	vseq.b $vr2, $vr2, $vr0
	vor.v $vr3, $vr1, $vr2
	vsetnez.v $fcc0, $vr3
	bcnez $fcc0, 5f
	addi.d $a0, $a0, 32
	b 3b
5:	vmsknz.b $vr1, $vr1
	vpickve2gr.hu $t1, $vr1, 0
	bnez $t1, .Lchr_found
	addi.d $a0, $a0, 16
	vmsknz.b $vr2, $vr2
	vpickve2gr.hu $t1, $vr2, 0
	b .Lchr_found
4:	bgeu $a0, $a3, .Lchr_null
	vld $vr1, $a0, 0
	vseq.b $vr1, $vr1, $vr0
	vmsknz.b $vr1, $vr1
	vpickve2gr.hu $t1, $vr1, 0
	bnez $t1, .Lchr_found
	addi.d $a0, $a0, 16
	b 4b
.Lchr_found:
	ctz.d $t1, $t1
	add.d $t1, $a0, $t1
	bgeu $t1, $a3, .Lchr_null
	move $a0, $t1
	jr $ra
#else
	FS_ZERO_CONSTS
	mul.d $a1, $a1, $a6 /* c in every byte */
	andi $t0, $a0, 7
	bstrins.d $a0, $zero, 2, 0
	ld.d $t1, $a0, 0
	xor $t1, $t1, $a1
	/* the bytes before s may not match */
	slli.d $t0, $t0, 3
	addi.d $t2, $zero, 1
	sll.d $t2, $t2, $t0
	addi.d $t2, $t2, -1
	or $t1, $t1, $t2
	b 3f
2:	addi.d $a0, $a0, 8
	bgeu $a0, $a3, .Lchr_null
	ld.d $t1, $a0, 0
	xor $t1, $t1, $a1
3:	FS_ZERO($t2, $t1)
	beqz $t2, 2b
	ctz.d $t2, $t2
	srli.d $t2, $t2, 3
	add.d $t2, $a0, $t2
	bgeu $t2, $a3, .Lchr_null
	move $a0, $t2
	jr $ra
#endif
.Lchr_null:
	move $a0, $zero
	jr $ra
FS_END(memchr)

/* ---- strlen (a0 s) -> a0 */

FS_FUNC(strlen)
	move $a1, $a0
#if SPRT_FS_ASM_SIMD
	andi $t0, $a0, 15
	bstrins.d $a1, $zero, 3, 0
	vld $vr0, $a1, 0
	vseqi.b $vr0, $vr0, 0
	vmsknz.b $vr0, $vr0
	vpickve2gr.hu $t1, $vr0, 0
	srl.d $t1, $t1, $t0
	beqz $t1, 1f
	ctz.d $a0, $t1
	jr $ra
	/* single blocks up to a 32-byte boundary, then two at a time */
1:	addi.d $a1, $a1, 16
	andi $t0, $a1, 31
	beqz $t0, 2f
	vld $vr0, $a1, 0
	vsetanyeqz.b $fcc0, $vr0
	bceqz $fcc0, 1b
	vseqi.b $vr0, $vr0, 0
	vmsknz.b $vr0, $vr0
	vpickve2gr.hu $t1, $vr0, 0
	b .Llen_found
2:	vld $vr0, $a1, 0
	vld $vr1, $a1, 16
	vmin.bu $vr2, $vr0, $vr1
	vsetanyeqz.b $fcc0, $vr2
	bcnez $fcc0, 3f
	addi.d $a1, $a1, 32
	b 2b
3:	vseqi.b $vr0, $vr0, 0
	vmsknz.b $vr0, $vr0
	vpickve2gr.hu $t1, $vr0, 0
	bnez $t1, .Llen_found
	addi.d $a1, $a1, 16
	vseqi.b $vr1, $vr1, 0
	vmsknz.b $vr1, $vr1
	vpickve2gr.hu $t1, $vr1, 0
.Llen_found:
	ctz.d $t1, $t1
	add.d $t1, $a1, $t1
	sub.d $a0, $t1, $a0
	jr $ra
#else
	FS_ZERO_CONSTS
	andi $t0, $a0, 7
	bstrins.d $a1, $zero, 2, 0
	ld.d $t1, $a1, 0
	slli.d $t0, $t0, 3
	addi.d $t2, $zero, 1
	sll.d $t2, $t2, $t0
	addi.d $t2, $t2, -1
	or $t1, $t1, $t2
	b 2f
1:	addi.d $a1, $a1, 8
	ld.d $t1, $a1, 0
2:	FS_ZERO($t2, $t1)
	beqz $t2, 1b
	ctz.d $t2, $t2
	srli.d $t2, $t2, 3
	add.d $t2, $a1, $t2
	sub.d $a0, $t2, $a0
	jr $ra
#endif
FS_END(strlen)

/* ---- strcmp (a0 a, a1 b) -> a0 */

#if SPRT_FS_ASM_SIMD
#define FS_CMP_SHIFT 4
#else
#define FS_CMP_SHIFT 3
#endif

FS_FUNC(strcmp)
#if !SPRT_FS_ASM_SIMD
	FS_ZERO_CONSTS
#endif
	li.d $a5, 4096
.Lscmp_outer:
	/* t3: how many whole steps both strings take before either reaches the end of
	 * a page; the step that would cross it is taken byte by byte */
	andi $t0, $a0, 4095
	andi $t1, $a1, 4095
	sltu $t2, $t0, $t1
	maskeqz $t1, $t1, $t2
	masknez $t0, $t0, $t2
	or $t0, $t0, $t1 /* the larger offset */
	sub.d $t0, $a5, $t0
	srli.d $t3, $t0, FS_CMP_SHIFT
	beqz $t3, .Lscmp_byte
#if SPRT_FS_ASM_SIMD
1:	vld $vr0, $a0, 0
	vld $vr1, $a1, 0
	vseq.b $vr2, $vr0, $vr1
	vseqi.b $vr3, $vr0, 0
	vorn.v $vr2, $vr3, $vr2 /* differs, or ends a */
	vsetnez.v $fcc0, $vr2
	bcnez $fcc0, 2f
	addi.d $a0, $a0, 16
	addi.d $a1, $a1, 16
	addi.d $t3, $t3, -1
	bnez $t3, 1b
	b .Lscmp_outer
2:	vmsknz.b $vr2, $vr2
	vpickve2gr.hu $t1, $vr2, 0
	ctz.d $t1, $t1
	ldx.bu $t2, $a0, $t1
	ldx.bu $t4, $a1, $t1
	sub.d $a0, $t2, $t4
	jr $ra
#else
1:	ld.d $t0, $a0, 0
	ld.d $t1, $a1, 0
	bne $t0, $t1, .Lscmp_bytes
	FS_ZERO($t2, $t0)
	bnez $t2, .Lscmp_equal
	addi.d $a0, $a0, 8
	addi.d $a1, $a1, 8
	addi.d $t3, $t3, -1
	bnez $t3, 1b
	b .Lscmp_outer
.Lscmp_equal:
	move $a0, $zero
	jr $ra
	/* a difference or a terminator is in these 8 bytes */
.Lscmp_bytes:
	ld.bu $t0, $a0, 0
	ld.bu $t1, $a1, 0
	bne $t0, $t1, 3f
	beqz $t0, 3f
	addi.d $a0, $a0, 1
	addi.d $a1, $a1, 1
	b .Lscmp_bytes
3:	sub.d $a0, $t0, $t1
	jr $ra
#endif
.Lscmp_byte:
	ld.bu $t0, $a0, 0
	ld.bu $t1, $a1, 0
	bne $t0, $t1, 4f
	beqz $t0, 4f
	addi.d $a0, $a0, 1
	addi.d $a1, $a1, 1
	b .Lscmp_outer
4:	sub.d $a0, $t0, $t1
	jr $ra
FS_END(strcmp)

#endif /* SPRT_FS_HAVE_ASM && __loongarch64 */

#if defined(__loongarch64) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
