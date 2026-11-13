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

/* aarch64 AAPCS64: memcpy memmove memset memcmp bcmp memchr strlen strcmp.
 *
 * The same structure as the x86_64 version. x18 is never touched (the shadow
 * call stack), and the SIMD level uses v0-v7 only, which need no saving. */

#include "../../include/sprt_fs_config.h"

#if SPRT_FS_HAVE_ASM && defined(__aarch64__)

#define FS_FUNC(name) \
	.text; \
	.p2align 4; \
	.global SPRT_FS_NAME(name); \
	.type SPRT_FS_NAME(name), %function; \
	SPRT_FS_NAME(name):

#define FS_END(name) .size SPRT_FS_NAME(name), .- SPRT_FS_NAME(name)

#if SPRT_FS_ASM_SIMD
#define FS_SMALL 64
#else
#define FS_SMALL 64
#endif

/* ---- memcpy (x0 dst, x1 src, x2 n) -> x0 dst */

FS_FUNC(memcpy)
.Lcpy_body:
	add x4, x1, x2 /* src end */
	add x5, x0, x2 /* dst end */
	cmp x2, #16
	b.hi .Lcpy_gt16
	cmp x2, #8
	b.lo .Lcpy_lt8
	ldr x6, [x1]
	ldur x7, [x4, #-8]
	str x6, [x0]
	stur x7, [x5, #-8]
	ret
.Lcpy_lt8:
	cmp x2, #4
	b.lo .Lcpy_lt4
	ldr w6, [x1]
	ldur w7, [x4, #-4]
	str w6, [x0]
	stur w7, [x5, #-4]
	ret
.Lcpy_lt4:
	cbz x2, .Lcpy_ret
	lsr x3, x2, #1
	ldrb w6, [x1]
	ldrb w7, [x1, x3]
	ldurb w8, [x4, #-1]
	strb w6, [x0]
	strb w7, [x0, x3]
	sturb w8, [x5, #-1]
.Lcpy_ret:
	ret

#if SPRT_FS_ASM_SIMD
.Lcpy_gt16:
	cmp x2, #32
	b.hi .Lcpy_gt32
	ldr q0, [x1]
	ldur q1, [x4, #-16]
	str q0, [x0]
	stur q1, [x5, #-16]
	ret
.Lcpy_gt32:
	cmp x2, #64
	b.hi .Lcpy_gt64
	ldp q0, q1, [x1]
	ldp q2, q3, [x4, #-32]
	stp q0, q1, [x0]
	stp q2, q3, [x5, #-32]
	ret
.Lcpy_gt64:
	/* the last 64 bytes are loaded first and stored last */
	ldp q4, q5, [x4, #-64]
	ldp q6, q7, [x4, #-32]
	sub x9, x2, #64
	mov x3, x0
1:	ldp q0, q1, [x1]
	ldp q2, q3, [x1, #32]
	stp q0, q1, [x3]
	stp q2, q3, [x3, #32]
	add x1, x1, #64
	add x3, x3, #64
	subs x9, x9, #64
	b.hi 1b
	stp q4, q5, [x5, #-64]
	stp q6, q7, [x5, #-32]
	ret
#else
.Lcpy_gt16:
	cmp x2, #32
	b.hi .Lcpy_gt32
	ldp x6, x7, [x1]
	ldp x8, x9, [x4, #-16]
	stp x6, x7, [x0]
	stp x8, x9, [x5, #-16]
	ret
.Lcpy_gt32:
	cmp x2, #64
	b.hi .Lcpy_gt64
	ldp x6, x7, [x1]
	ldp x8, x9, [x1, #16]
	ldp x10, x11, [x4, #-32]
	ldp x12, x13, [x4, #-16]
	stp x6, x7, [x0]
	stp x8, x9, [x0, #16]
	stp x10, x11, [x5, #-32]
	stp x12, x13, [x5, #-16]
	ret
.Lcpy_gt64:
	/* the last 32 bytes are loaded first and stored last */
	ldp x14, x15, [x4, #-32]
	ldp x16, x17, [x4, #-16]
	sub x9, x2, #32
	mov x3, x0
1:	ldp x6, x7, [x1]
	ldp x10, x11, [x1, #16]
	stp x6, x7, [x3]
	stp x10, x11, [x3, #16]
	add x1, x1, #32
	add x3, x3, #32
	subs x9, x9, #32
	b.hi 1b
	stp x14, x15, [x5, #-32]
	stp x16, x17, [x5, #-16]
	ret
#endif
FS_END(memcpy)

/* ---- memmove (x0 dst, x1 src, x2 n) -> x0 dst */

FS_FUNC(memmove)
	sub x3, x0, x1
	cmp x3, x2
	b.hs .Lcpy_body /* dst below src, or no overlap: forward */
	cmp x2, #FS_SMALL
	b.ls .Lcpy_body /* short: everything is loaded before anything is stored */

	/* backward: the first bytes are loaded first and stored last */
	add x4, x1, x2
	add x5, x0, x2
#if SPRT_FS_ASM_SIMD
	ldp q4, q5, [x1]
	ldp q6, q7, [x1, #32]
	sub x9, x2, #64
1:	ldp q0, q1, [x4, #-64]
	ldp q2, q3, [x4, #-32]
	stp q2, q3, [x5, #-32]
	stp q0, q1, [x5, #-64]
	sub x4, x4, #64
	sub x5, x5, #64
	subs x9, x9, #64
	b.hi 1b
	stp q4, q5, [x0]
	stp q6, q7, [x0, #32]
#else
	ldp x14, x15, [x1]
	ldp x16, x17, [x1, #16]
	sub x9, x2, #32
1:	ldp x6, x7, [x4, #-32]
	ldp x10, x11, [x4, #-16]
	stp x10, x11, [x5, #-16]
	stp x6, x7, [x5, #-32]
	sub x4, x4, #32
	sub x5, x5, #32
	subs x9, x9, #32
	b.hi 1b
	stp x14, x15, [x0]
	stp x16, x17, [x0, #16]
#endif
	ret
FS_END(memmove)

/* ---- memset (x0 dst, w1 c, x2 n) -> x0 dst */

FS_FUNC(memset)
	and x1, x1, #0xff
	mov x3, #0x0101010101010101
	mul x1, x1, x3
	add x4, x0, x2 /* end */
	cmp x2, #16
	b.hi .Lset_gt16
	cmp x2, #8
	b.lo .Lset_lt8
	str x1, [x0]
	stur x1, [x4, #-8]
	ret
.Lset_lt8:
	cmp x2, #4
	b.lo .Lset_lt4
	str w1, [x0]
	stur w1, [x4, #-4]
	ret
.Lset_lt4:
	cbz x2, .Lset_ret
	strb w1, [x0]
	sturb w1, [x4, #-1]
	cmp x2, #2
	b.lo .Lset_ret
	strb w1, [x0, #1]
.Lset_ret:
	ret

#if SPRT_FS_ASM_SIMD
.Lset_gt16:
	dup v0.2d, x1
	cmp x2, #32
	b.hi .Lset_gt32
	str q0, [x0]
	stur q0, [x4, #-16]
	ret
.Lset_gt32:
	cmp x2, #64
	b.hi .Lset_gt64
	stp q0, q0, [x0]
	stp q0, q0, [x4, #-32]
	ret
.Lset_gt64:
	stp q0, q0, [x4, #-64]
	stp q0, q0, [x4, #-32]
	sub x9, x2, #64
	mov x3, x0
1:	stp q0, q0, [x3]
	stp q0, q0, [x3, #32]
	add x3, x3, #64
	subs x9, x9, #64
	b.hi 1b
	ret
#else
.Lset_gt16:
	cmp x2, #32
	b.hi .Lset_gt32
	stp x1, x1, [x0]
	stp x1, x1, [x4, #-16]
	ret
.Lset_gt32:
	stp x1, x1, [x4, #-32]
	stp x1, x1, [x4, #-16]
	sub x9, x2, #32
	mov x3, x0
1:	stp x1, x1, [x3]
	stp x1, x1, [x3, #16]
	add x3, x3, #32
	subs x9, x9, #32
	b.hi 1b
	ret
#endif
FS_END(memset)

#if SPRT_FS_A64_SVE
/* SVE: predicated and first-fault loads, for any vector length. A first-fault load
 * suppresses a fault past its first element, so no read needs a page boundary
 * check: FFR tells how much of the vector was really loaded. */
	.arch_extension sve

/* ---- memcmp, bcmp (x0 a, x1 b, x2 n) -> w0 */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), %function
SPRT_FS_NAME(bcmp):
	mov x3, #0
	whilelo p0.b, x3, x2
	b.none 2f
1:	ld1b z0.b, p0/z, [x0, x3]
	ld1b z1.b, p0/z, [x1, x3]
	cmpne p1.b, p0/z, z0.b, z1.b
	b.any 3f
	incb x3
	whilelo p0.b, x3, x2
	b.any 1b
2:	mov w0, #0
	ret
	/* the bytes after the ones before the first difference */
3:	brkb p1.b, p0/z, p1.b
	lasta w4, p1, z0.b
	lasta w5, p1, z1.b
	sub w0, w4, w5
	ret
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (x0 s, w1 c, x2 n) -> x0 */

FS_FUNC(memchr)
	cbz x2, 6f
	dup z1.b, w1
	mov x3, #0
	whilelo p2.b, x3, x2
	setffr
1:	ldff1b z0.b, p2/z, [x0, x3]
	rdffrs p0.b, p2/z
	b.nlast 3f
	cmpeq p1.b, p2/z, z0.b, z1.b
	b.any 2f
	incb x3
	whilelo p2.b, x3, x2
	b.any 1b
	b 6f
2:	brkb p1.b, p2/z, p1.b
	incp x3, p1.b
	add x0, x0, x3
	ret
	/* only the lanes in p0 were loaded */
3:	cmpeq p1.b, p0/z, z0.b, z1.b
	b.any 4f
	incp x3, p0.b
	setffr
	whilelo p2.b, x3, x2
	b.any 1b
	b 6f
4:	brkb p1.b, p0/z, p1.b
	incp x3, p1.b
	add x0, x0, x3
	ret
6:	mov x0, #0
	ret
FS_END(memchr)

/* ---- strlen (x0 s) -> x0 */

FS_FUNC(strlen)
	ptrue p2.b
	mov x1, #0
	setffr
1:	ldff1b z0.b, p2/z, [x0, x1]
	rdffrs p0.b, p2/z
	b.nlast 3f
	cmpeq p1.b, p2/z, z0.b, #0
	b.any 2f
	incb x1
	b 1b
2:	brkb p1.b, p2/z, p1.b
	incp x1, p1.b
	mov x0, x1
	ret
3:	cmpeq p1.b, p0/z, z0.b, #0
	b.any 4f
	incp x1, p0.b
	setffr
	b 1b
4:	brkb p1.b, p0/z, p1.b
	incp x1, p1.b
	mov x0, x1
	ret
FS_END(strlen)

/* ---- strcmp (x0 a, x1 b) -> w0 */

FS_FUNC(strcmp)
	ptrue p1.b
	mov x2, #0
	setffr
1:	ldff1b z0.b, p1/z, [x0, x2]
	ldff1b z1.b, p1/z, [x1, x2]
	rdffrs p0.b, p1/z
	b.nlast 3f
	incb x2
	cmpeq p2.b, p1/z, z0.b, z1.b
	cmpne p3.b, p1/z, z0.b, #0
	nands p2.b, p1/z, p2.b, p3.b /* differs, or ends a */
	b.none 1b
2:	brkb p2.b, p1/z, p2.b
	lasta w0, p2, z0.b
	lasta w1, p2, z1.b
	sub w0, w0, w1
	ret
3:	incp x2, p0.b
	cmpeq p2.b, p0/z, z0.b, z1.b
	cmpne p3.b, p0/z, z0.b, #0
	nands p2.b, p0/z, p2.b, p3.b
	b.any 2b
	setffr
	b 1b
FS_END(strcmp)

#else /* SPRT_FS_A64_SVE */

/* ---- memcmp, bcmp (x0 a, x1 b, x2 n) -> w0 */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), %function
SPRT_FS_NAME(bcmp):
	cmp x2, #8
	b.lo .Lcmp_lt8
	cmp x2, #16
	b.lo .Lcmp_8_15
	/* 16 bytes a step, then the last 16 overlapping the ones compared */
	add x9, x0, x2
	sub x9, x9, #16 /* the last block of a */
	add x12, x1, x2
	sub x12, x12, #16 /* the last block of b */
	mov x10, x0
	mov x11, x1
	b 2f
1:	ldp x3, x4, [x10], #16
	ldp x5, x6, [x11], #16
	cmp x3, x5
	ccmp x4, x6, #0, eq
	b.ne .Lcmp_pair_diff
2:	cmp x10, x9
	b.lo 1b
	ldp x3, x4, [x9]
	ldp x5, x6, [x12]
	cmp x3, x5
	ccmp x4, x6, #0, eq
	b.ne .Lcmp_pair_diff
	mov w0, #0
	ret
.Lcmp_pair_diff:
	cmp x3, x5
	csel x3, x3, x4, ne
	csel x5, x5, x6, ne
	b .Lcmp_word_diff
.Lcmp_8_15:
	ldr x3, [x0]
	ldr x5, [x1]
	cmp x3, x5
	b.ne .Lcmp_word_diff
	add x6, x0, x2
	add x7, x1, x2
	ldur x3, [x6, #-8]
	ldur x5, [x7, #-8]
	cmp x3, x5
	b.ne .Lcmp_word_diff
	mov w0, #0
	ret
.Lcmp_word_diff:
	/* in memory order, the first differing byte decides */
	rev x3, x3
	rev x5, x5
	cmp x3, x5
	cset w0, hi
	csinv w0, w0, wzr, hs
	ret
.Lcmp_lt8:
	cmp x2, #4
	b.lo .Lcmp_lt4
	ldr w3, [x0]
	ldr w5, [x1]
	cmp w3, w5
	b.ne .Lcmp_dword_diff
	add x6, x0, x2
	add x7, x1, x2
	ldur w3, [x6, #-4]
	ldur w5, [x7, #-4]
	cmp w3, w5
	b.ne .Lcmp_dword_diff
	mov w0, #0
	ret
.Lcmp_dword_diff:
	rev w3, w3
	rev w5, w5
	cmp w3, w5
	cset w0, hi
	csinv w0, w0, wzr, hs
	ret
.Lcmp_lt4:
	/* bytes 0, n/2 and n-1 cover 1..3 bytes in order: compare them as one number */
	cbz x2, 1f
	lsr x8, x2, #1
	sub x9, x2, #1
	ldrb w3, [x0]
	ldrb w5, [x1]
	ldrb w6, [x0, x8]
	ldrb w7, [x1, x8]
	ldrb w10, [x0, x9]
	ldrb w11, [x1, x9]
	orr w3, w10, w3, lsl #16
	orr w5, w11, w5, lsl #16
	orr w3, w3, w6, lsl #8
	orr w5, w5, w7, lsl #8
	sub w0, w3, w5
	ret
1:	mov w0, #0
	ret
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (x0 s, w1 c, x2 n) -> x0 */

FS_FUNC(memchr)
	cbz x2, .Lchr_null
	/* x8: the end, saturated when s + n wraps */
	adds x8, x0, x2
	csinv x8, x8, xzr, cc
#if SPRT_FS_ASM_SIMD
	dup v0.16b, w1
	and x9, x0, #15
	bic x10, x0, #15
	ldr q1, [x10]
	cmeq v1.16b, v1.16b, v0.16b
	shrn v1.8b, v1.8h, #4 /* four bits a byte */
	fmov x3, d1
	lsl x9, x9, #2
	lsr x3, x3, x9 /* the bytes before s */
	cbz x3, 2f
	rbit x3, x3
	clz x3, x3
	add x3, x0, x3, lsr #2
	cmp x3, x8
	b.hs .Lchr_null
	mov x0, x3
	ret
2:	add x10, x10, #16
	/* one more block up to a 32-byte boundary: a pair read from there is never
	 * split between pages, so reading past a match cannot fault */
	tst x10, #31
	b.eq 3f
	cmp x10, x8
	b.hs .Lchr_null
	ldr q1, [x10]
	cmeq v1.16b, v1.16b, v0.16b
	shrn v1.8b, v1.8h, #4
	fmov x3, d1
	cbnz x3, .Lchr_found
	add x10, x10, #16
	/* two blocks at a time while both start before the end; the blocks before
	 * may already have passed it */
3:	subs x4, x8, x10
	b.ls .Lchr_null
	cmp x4, #32
	b.ls 4f
	ldp q1, q2, [x10]
	cmeq v1.16b, v1.16b, v0.16b
	cmeq v2.16b, v2.16b, v0.16b
	orr v3.16b, v1.16b, v2.16b
	umaxp v3.16b, v3.16b, v3.16b
	fmov x3, d3
	cbnz x3, 5f
	add x10, x10, #32
	b 3b
5:	shrn v1.8b, v1.8h, #4
	fmov x3, d1
	cbnz x3, .Lchr_found
	add x10, x10, #16
	shrn v2.8b, v2.8h, #4
	fmov x3, d2
	b .Lchr_found
4:	cmp x10, x8
	b.hs .Lchr_null
	ldr q1, [x10]
	cmeq v1.16b, v1.16b, v0.16b
	shrn v1.8b, v1.8h, #4
	fmov x3, d1
	cbnz x3, .Lchr_found
	add x10, x10, #16
	b 4b
.Lchr_found:
	rbit x3, x3
	clz x3, x3
	add x3, x10, x3, lsr #2
	cmp x3, x8
	b.hs .Lchr_null
	mov x0, x3
	ret
#else
	and x1, x1, #0xff
	mov x11, #0x0101010101010101
	mul x1, x1, x11
	mov x12, #0x8080808080808080
	and x9, x0, #7
	bic x10, x0, #7
	ldr x3, [x10]
	eor x3, x3, x1
	/* the bytes before s may not match */
	lsl x9, x9, #3
	mov x4, #1
	lsl x4, x4, x9
	sub x4, x4, #1
	orr x3, x3, x4
	b 2f
1:	add x10, x10, #8
	cmp x10, x8
	b.hs .Lchr_null
	ldr x3, [x10]
	eor x3, x3, x1
2:	sub x5, x3, x11
	bic x5, x5, x3
	and x5, x5, x12
	cbz x5, 1b
	/* the lowest flagged byte is exact: a borrow only reaches bytes above a zero */
	rbit x5, x5
	clz x5, x5
	add x5, x10, x5, lsr #3
	cmp x5, x8
	b.hs .Lchr_null
	mov x0, x5
	ret
#endif
.Lchr_null:
	mov x0, #0
	ret
FS_END(memchr)

/* ---- strlen (x0 s) -> x0 */

FS_FUNC(strlen)
#if SPRT_FS_ASM_SIMD
	bic x1, x0, #15
	and x2, x0, #15
	ldr q0, [x1]
	cmeq v0.16b, v0.16b, #0
	shrn v0.8b, v0.8h, #4
	fmov x3, d0
	lsl x2, x2, #2
	lsr x3, x3, x2
	cbz x3, 1f
	rbit x3, x3
	clz x3, x3
	lsr x0, x3, #2
	ret
	/* single blocks up to a 32-byte boundary, then two at a time */
1:	add x1, x1, #16
	tst x1, #31
	b.eq 2f
	ldr q0, [x1]
	cmeq v0.16b, v0.16b, #0
	shrn v0.8b, v0.8h, #4
	fmov x3, d0
	cbz x3, 1b
	b .Llen_found
2:	ldp q0, q1, [x1]
	uminp v2.16b, v0.16b, v1.16b
	uminp v2.16b, v2.16b, v2.16b
	cmeq v2.8b, v2.8b, #0
	fmov x3, d2
	cbnz x3, 3f
	add x1, x1, #32
	b 2b
3:	cmeq v0.16b, v0.16b, #0
	shrn v0.8b, v0.8h, #4
	fmov x3, d0
	cbnz x3, .Llen_found
	add x1, x1, #16
	cmeq v1.16b, v1.16b, #0
	shrn v1.8b, v1.8h, #4
	fmov x3, d1
.Llen_found:
	rbit x3, x3
	clz x3, x3
	add x3, x1, x3, lsr #2
	sub x0, x3, x0
	ret
#else
	mov x11, #0x0101010101010101
	mov x12, #0x8080808080808080
	and x2, x0, #7
	bic x1, x0, #7
	ldr x3, [x1]
	lsl x2, x2, #3
	mov x4, #1
	lsl x4, x4, x2
	sub x4, x4, #1
	orr x3, x3, x4
	b 2f
1:	add x1, x1, #8
	ldr x3, [x1]
2:	sub x5, x3, x11
	bic x5, x5, x3
	and x5, x5, x12
	cbz x5, 1b
	rbit x5, x5
	clz x5, x5
	add x5, x1, x5, lsr #3
	sub x0, x5, x0
	ret
#endif
FS_END(strlen)

/* ---- strcmp (x0 a, x1 b) -> w0 */

#if SPRT_FS_ASM_SIMD
#define FS_CMP_SHIFT 4
#else
#define FS_CMP_SHIFT 3
#endif

FS_FUNC(strcmp)
#if !SPRT_FS_ASM_SIMD
	mov x11, #0x0101010101010101
	mov x12, #0x8080808080808080
#endif
	mov x13, #4096
.Lscmp_outer:
	/* x9: how many whole steps both strings take before either reaches the end
	 * of a page; the step that would cross it is taken byte by byte */
	and x2, x0, #4095
	and x3, x1, #4095
	cmp x2, x3
	csel x2, x2, x3, hi
	sub x2, x13, x2
	lsr x9, x2, #FS_CMP_SHIFT
	cbz x9, .Lscmp_byte
#if SPRT_FS_ASM_SIMD
1:	ldr q0, [x0]
	ldr q1, [x1]
	cmeq v2.16b, v0.16b, v1.16b
	cmeq v3.16b, v0.16b, #0
	orn v2.16b, v3.16b, v2.16b /* differs, or ends a */
	shrn v2.8b, v2.8h, #4
	fmov x3, d2
	cbnz x3, 2f
	add x0, x0, #16
	add x1, x1, #16
	subs x9, x9, #1
	b.ne 1b
	b .Lscmp_outer
2:	rbit x3, x3
	clz x3, x3
	lsr x3, x3, #2
	ldrb w4, [x0, x3]
	ldrb w5, [x1, x3]
	sub w0, w4, w5
	ret
#else
1:	ldr x3, [x0]
	ldr x4, [x1]
	cmp x3, x4
	b.ne .Lscmp_bytes
	sub x5, x3, x11
	bic x5, x5, x3
	ands x5, x5, x12
	b.ne .Lscmp_equal
	add x0, x0, #8
	add x1, x1, #8
	subs x9, x9, #1
	b.ne 1b
	b .Lscmp_outer
.Lscmp_equal:
	mov w0, #0
	ret
	/* a difference or a terminator is in these 8 bytes */
.Lscmp_bytes:
	ldrb w4, [x0], #1
	ldrb w5, [x1], #1
	subs w3, w4, w5
	b.ne 2f
	cbnz w5, .Lscmp_bytes
2:	mov w0, w3
	ret
#endif
.Lscmp_byte:
	ldrb w4, [x0], #1
	ldrb w5, [x1], #1
	subs w3, w4, w5
	b.ne 3f
	cbnz w5, .Lscmp_outer
3:	mov w0, w3
	ret
FS_END(strcmp)

#endif /* SPRT_FS_A64_SVE */

#endif /* SPRT_FS_HAVE_ASM && __aarch64__ */

#if defined(__aarch64__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
