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

/* riscv64 LP64: memcpy memmove memset memcmp bcmp memchr strlen strcmp.
 *
 * The general-register level makes aligned word accesses only: a misaligned access
 * may trap, or be emulated by the firmware, on many cores. A copy whose source is
 * not aligned with its destination merges aligned source words by shifts. An
 * aligned word never crosses a page, so a search may read a whole word past the
 * end of its object. Zbb (orc.b, ctz) speeds up the searches where it is enabled.
 *
 * The vector level (RVV 1.0) is vector-length agnostic; the searches use
 * fault-only-first loads, which stop at the first element that would fault, so
 * they need no page boundary checks. */

#include "../../include/sprt_fs_config.h"

#if SPRT_FS_HAVE_ASM && defined(__riscv) && __riscv_xlen == 64

#define FS_FUNC(name) \
	.text; \
	.p2align 2; \
	.global SPRT_FS_NAME(name); \
	.type SPRT_FS_NAME(name), @function; \
	SPRT_FS_NAME(name):

#define FS_END(name) .size SPRT_FS_NAME(name), .- SPRT_FS_NAME(name)

#if SPRT_FS_RV_ZBB
	.option arch, +zbb
#endif
#if SPRT_FS_ASM_SIMD
	.option arch, +v
#endif

#if SPRT_FS_ASM_SIMD

/* ---- memcpy (a0 dst, a1 src, a2 n) -> a0 dst */

FS_FUNC(memcpy)
	mv t6, a0
.Lcpy_body:
1:	vsetvli t0, a2, e8, m8, ta, ma
	vle8.v v0, (a1)
	vse8.v v0, (t6)
	sub a2, a2, t0
	add a1, a1, t0
	add t6, t6, t0
	bnez a2, 1b
	ret
FS_END(memcpy)

/* ---- memmove (a0 dst, a1 src, a2 n) -> a0 dst */

FS_FUNC(memmove)
	mv t6, a0
	sub t0, a0, a1
	bgeu t0, a2, .Lcpy_body /* dst below src, or no overlap: forward */
	/* backward, a whole vector loaded before it is stored */
	add a1, a1, a2
	add t6, a0, a2
1:	vsetvli t0, a2, e8, m8, ta, ma
	sub a1, a1, t0
	sub t6, t6, t0
	vle8.v v0, (a1)
	vse8.v v0, (t6)
	sub a2, a2, t0
	bnez a2, 1b
	ret
FS_END(memmove)

/* ---- memset (a0 dst, a1 c, a2 n) -> a0 dst */

FS_FUNC(memset)
	mv t6, a0
	vsetvli t0, zero, e8, m8, ta, ma
	vmv.v.x v0, a1
1:	vsetvli t0, a2, e8, m8, ta, ma
	vse8.v v0, (t6)
	sub a2, a2, t0
	add t6, t6, t0
	bnez a2, 1b
	ret
FS_END(memset)

/* ---- memcmp, bcmp (a0 a, a1 b, a2 n) -> a0 */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), @function
SPRT_FS_NAME(bcmp):
1:	beqz a2, 3f
	vsetvli t0, a2, e8, m8, ta, ma
	vle8.v v0, (a0)
	vle8.v v8, (a1)
	vmsne.vv v16, v0, v8
	vfirst.m t1, v16
	bgez t1, 2f
	add a0, a0, t0
	add a1, a1, t0
	sub a2, a2, t0
	j 1b
2:	add a0, a0, t1
	add a1, a1, t1
	lbu t2, 0(a0)
	lbu t3, 0(a1)
	sub a0, t2, t3
	ret
3:	li a0, 0
	ret
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (a0 s, a1 c, a2 n) -> a0 */

FS_FUNC(memchr)
	beqz a2, 3f
1:	vsetvli t0, a2, e8, m8, ta, ma
	vle8ff.v v0, (a0)
	csrr t0, vl /* what really loaded */
	vmseq.vx v16, v0, a1
	vfirst.m t1, v16
	bgez t1, 2f
	add a0, a0, t0
	sub a2, a2, t0
	bnez a2, 1b
3:	li a0, 0
	ret
2:	add a0, a0, t1
	ret
FS_END(memchr)

/* ---- strlen (a0 s) -> a0 */

FS_FUNC(strlen)
	mv t2, a0
1:	vsetvli t0, zero, e8, m8, ta, ma
	vle8ff.v v0, (t2)
	csrr t0, vl
	vmseq.vi v16, v0, 0
	vfirst.m t1, v16
	bgez t1, 2f
	add t2, t2, t0
	j 1b
2:	add t2, t2, t1
	sub a0, t2, a0
	ret
FS_END(strlen)

/* ---- strcmp (a0 a, a1 b) -> a0 */

FS_FUNC(strcmp)
1:	vsetvli t0, zero, e8, m8, ta, ma
	vle8ff.v v0, (a0)
	vle8ff.v v8, (a1) /* under the vl the first load left */
	csrr t0, vl
	vmseq.vi v16, v0, 0
	vmsne.vv v17, v0, v8
	vmor.mm v16, v16, v17 /* ends a, or differs */
	vfirst.m t1, v16
	bgez t1, 2f
	add a0, a0, t0
	add a1, a1, t0
	j 1b
2:	add a0, a0, t1
	add a1, a1, t1
	lbu t2, 0(a0)
	lbu t3, 0(a1)
	sub a0, t2, t3
	ret
FS_END(strcmp)

#else /* SPRT_FS_ASM_SIMD */

/* FS_ZERO(r, x, t): r gets a flag in the lowest byte of x that is zero, and in no
 * byte below it (r is 0 when x has none). FS_FIRST(r, t) turns that flag into the
 * byte index. a4 = 0x0101..01 and a5 = 0x8080..80 without Zbb. */
#if SPRT_FS_RV_ZBB
#define FS_ZERO(r, x, t) \
	orc.b r, x; \
	not r, r
#define FS_FIRST(r, t) \
	ctz r, r; \
	srli r, r, 3
#define FS_ZERO_CONSTS
#else
#define FS_ZERO(r, x, t) \
	sub r, x, a4; \
	not t, x; \
	and r, r, t; \
	and r, r, a5
#define FS_FIRST(r, t) \
	li t, 0; \
8:	andi a7, r, 0x80; \
	bnez a7, 9f; \
	srli r, r, 8; \
	addi t, t, 1; \
	j 8b; \
9:	mv r, t
#define FS_ZERO_CONSTS \
	li a4, 0x0101010101010101; \
	li a5, 0x8080808080808080
#endif

/* ---- memcpy (a0 dst, a1 src, a2 n) -> a0 dst */

FS_FUNC(memcpy)
	mv t6, a0
.Lcpy_body:
	li t0, 16
	bltu a2, t0, .Lcpy_bytes
	/* align dst */
	andi t1, t6, 7
	beqz t1, 2f
	li t2, 8
	sub t1, t2, t1
	sub a2, a2, t1
1:	lbu t2, 0(a1)
	sb t2, 0(t6)
	addi a1, a1, 1
	addi t6, t6, 1
	addi t1, t1, -1
	bnez t1, 1b
2:	andi t1, a1, 7
	bnez t1, .Lcpy_shift
	/* src aligned too: 32 bytes a step, then words */
	li t0, 32
	bltu a2, t0, 4f
3:	ld t1, 0(a1)
	ld t2, 8(a1)
	ld t3, 16(a1)
	ld t4, 24(a1)
	sd t1, 0(t6)
	sd t2, 8(t6)
	sd t3, 16(t6)
	sd t4, 24(t6)
	addi a1, a1, 32
	addi t6, t6, 32
	addi a2, a2, -32
	bgeu a2, t0, 3b
4:	li t0, 8
	bltu a2, t0, .Lcpy_bytes
5:	ld t1, 0(a1)
	sd t1, 0(t6)
	addi a1, a1, 8
	addi t6, t6, 8
	addi a2, a2, -8
	bgeu a2, t0, 5b
	j .Lcpy_bytes
	/* src t1 bytes past a word: each stored word merges two aligned source words */
.Lcpy_shift:
	slli t3, t1, 3
	li t4, 64
	sub t4, t4, t3
	andi t5, a1, -8
	ld t0, 0(t5)
	li a3, 8
6:	ld a4, 8(t5)
	srl t0, t0, t3
	sll a5, a4, t4
	or t0, t0, a5
	sd t0, 0(t6)
	mv t0, a4
	addi t5, t5, 8
	addi t6, t6, 8
	addi a1, a1, 8
	addi a2, a2, -8
	bgeu a2, a3, 6b
.Lcpy_bytes:
	beqz a2, 8f
7:	lbu t1, 0(a1)
	sb t1, 0(t6)
	addi a1, a1, 1
	addi t6, t6, 1
	addi a2, a2, -1
	bnez a2, 7b
8:	ret
FS_END(memcpy)

/* ---- memmove (a0 dst, a1 src, a2 n) -> a0 dst */

FS_FUNC(memmove)
	mv t6, a0
	sub t0, a0, a1
	bgeu t0, a2, .Lcpy_body /* dst below src, or no overlap: forward */
	/* backward from the ends: words when dst - src is a multiple of 8 (and so at
	 * least 8: a stored word never reaches one still to load), bytes otherwise */
	add a1, a1, a2
	add t6, a0, a2
	andi t1, t0, 7
	bnez t1, 3f
	li t2, 16
	bltu a2, t2, 3f
1:	andi t1, t6, 7
	beqz t1, 2f
	addi a1, a1, -1
	addi t6, t6, -1
	lbu t2, 0(a1)
	sb t2, 0(t6)
	addi a2, a2, -1
	j 1b
2:	li t2, 8
4:	bltu a2, t2, 3f
	addi a1, a1, -8
	addi t6, t6, -8
	ld t1, 0(a1)
	sd t1, 0(t6)
	addi a2, a2, -8
	j 4b
3:	beqz a2, 5f
6:	addi a1, a1, -1
	addi t6, t6, -1
	lbu t1, 0(a1)
	sb t1, 0(t6)
	addi a2, a2, -1
	bnez a2, 6b
5:	ret
FS_END(memmove)

/* ---- memset (a0 dst, a1 c, a2 n) -> a0 dst */

FS_FUNC(memset)
	mv t6, a0
	andi a1, a1, 0xff
	li t0, 16
	bltu a2, t0, 5f
	li t1, 0x0101010101010101
	mul t2, a1, t1
1:	andi t1, t6, 7
	beqz t1, 2f
	sb a1, 0(t6)
	addi t6, t6, 1
	addi a2, a2, -1
	j 1b
2:	li t0, 32
	bltu a2, t0, 4f
3:	sd t2, 0(t6)
	sd t2, 8(t6)
	sd t2, 16(t6)
	sd t2, 24(t6)
	addi t6, t6, 32
	addi a2, a2, -32
	bgeu a2, t0, 3b
4:	li t0, 8
	bltu a2, t0, 5f
	sd t2, 0(t6)
	addi t6, t6, 8
	addi a2, a2, -8
	j 4b
5:	beqz a2, 7f
6:	sb a1, 0(t6)
	addi t6, t6, 1
	addi a2, a2, -1
	bnez a2, 6b
7:	ret
FS_END(memset)

/* ---- memcmp, bcmp (a0 a, a1 b, a2 n) -> a0 */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), @function
SPRT_FS_NAME(bcmp):
	/* words when a and b are aligned alike, bytes otherwise */
	li t0, 16
	bltu a2, t0, .Lcmp_bytes
	xor t1, a0, a1
	andi t1, t1, 7
	bnez t1, .Lcmp_bytes
1:	andi t1, a0, 7
	beqz t1, 2f
	lbu t2, 0(a0)
	lbu t3, 0(a1)
	bne t2, t3, .Lcmp_diff
	addi a0, a0, 1
	addi a1, a1, 1
	addi a2, a2, -1
	j 1b
2:	li t0, 8
3:	bltu a2, t0, .Lcmp_bytes
	ld t2, 0(a0)
	ld t3, 0(a1)
	bne t2, t3, 4f
	addi a0, a0, 8
	addi a1, a1, 8
	addi a2, a2, -8
	j 3b
4:	li a2, 8 /* the difference is in this word */
.Lcmp_bytes:
	beqz a2, 6f
5:	lbu t2, 0(a0)
	lbu t3, 0(a1)
	bne t2, t3, .Lcmp_diff
	addi a0, a0, 1
	addi a1, a1, 1
	addi a2, a2, -1
	bnez a2, 5b
6:	li a0, 0
	ret
.Lcmp_diff:
	sub a0, t2, t3
	ret
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (a0 s, a1 c, a2 n) -> a0 */

FS_FUNC(memchr)
	beqz a2, .Lchr_null
	/* a6: the end, saturated when s + n wraps */
	add a6, a0, a2
	bgeu a6, a0, 1f
	li a6, -1
1:	FS_ZERO_CONSTS
	andi a1, a1, 0xff
	li t0, 0x0101010101010101
	mul a3, a1, t0
	andi t0, a0, 7
	andi t1, a0, -8
	ld t2, 0(t1)
	xor t2, t2, a3
	/* the bytes before s may not match */
	slli t3, t0, 3
	li t4, 1
	sll t4, t4, t3
	addi t4, t4, -1
	or t2, t2, t4
	j 3f
2:	addi t1, t1, 8
	bgeu t1, a6, .Lchr_null
	ld t2, 0(t1)
	xor t2, t2, a3
3:	FS_ZERO(t3, t2, t5)
	beqz t3, 2b
	FS_FIRST(t3, t5)
	add t3, t1, t3
	bgeu t3, a6, .Lchr_null
	mv a0, t3
	ret
.Lchr_null:
	li a0, 0
	ret
FS_END(memchr)

/* ---- strlen (a0 s) -> a0 */

FS_FUNC(strlen)
	FS_ZERO_CONSTS
	andi t0, a0, 7
	andi t1, a0, -8
	ld t2, 0(t1)
	slli t3, t0, 3
	li t4, 1
	sll t4, t4, t3
	addi t4, t4, -1
	or t2, t2, t4
	j 2f
1:	addi t1, t1, 8
	ld t2, 0(t1)
2:	FS_ZERO(t3, t2, t5)
	beqz t3, 1b
	FS_FIRST(t3, t5)
	add t3, t1, t3
	sub a0, t3, a0
	ret
FS_END(strlen)

/* ---- strcmp (a0 a, a1 b) -> a0 */

FS_FUNC(strcmp)
	/* words when a and b are aligned alike, bytes otherwise */
	xor t0, a0, a1
	andi t0, t0, 7
	bnez t0, .Lscmp_bytes
	FS_ZERO_CONSTS
1:	andi t0, a0, 7
	beqz t0, 2f
	lbu t2, 0(a0)
	lbu t3, 0(a1)
	bne t2, t3, .Lscmp_diff
	beqz t2, .Lscmp_diff
	addi a0, a0, 1
	addi a1, a1, 1
	j 1b
2:	ld t2, 0(a0)
	ld t3, 0(a1)
	bne t2, t3, .Lscmp_bytes /* a difference or the end is in this word */
	FS_ZERO(t4, t2, t5)
	bnez t4, .Lscmp_equal
	addi a0, a0, 8
	addi a1, a1, 8
	j 2b
.Lscmp_equal:
	li a0, 0
	ret
.Lscmp_bytes:
	lbu t2, 0(a0)
	lbu t3, 0(a1)
	bne t2, t3, .Lscmp_diff
	beqz t2, .Lscmp_diff
	addi a0, a0, 1
	addi a1, a1, 1
	j .Lscmp_bytes
.Lscmp_diff:
	sub a0, t2, t3
	ret
FS_END(strcmp)

#endif /* SPRT_FS_ASM_SIMD */

#endif /* SPRT_FS_HAVE_ASM && __riscv */

#if defined(__riscv) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
