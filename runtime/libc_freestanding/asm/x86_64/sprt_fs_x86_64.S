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

/* x86_64 SysV: memcpy memmove memset memcmp bcmp memchr strlen strcmp.
 *
 * Copies load everything a step writes before writing it, so the short paths
 * of memcpy are also correct for an overlapping memmove. Searches read aligned
 * blocks only, which never cross into the next page; strcmp, whose two strings
 * cannot be aligned together, steps byte by byte near the end of a page. */

#include "../../include/sprt_fs_config.h"

#if SPRT_FS_HAVE_ASM && defined(__x86_64__)

#define FS_FUNC(name) \
	.text; \
	.p2align 4; \
	.global SPRT_FS_NAME(name); \
	.type SPRT_FS_NAME(name), @function; \
	SPRT_FS_NAME(name):

#define FS_END(name) .size SPRT_FS_NAME(name), .- SPRT_FS_NAME(name)

/* From these sizes copies and fills use rep movsb / rep stosb. On AMD only up to
 * FS_REP_MAX, past a core's L2 (1 MiB on Zen 4), where the loops are faster; Intel
 * keeps rep at any size. */
#define FS_REP_MAX (1 << 20)
/* The AVX2 loops stay ahead of rep longer: measured on Zen 4 and Coffee Lake, copies
 * cross over between 4 and 16 KiB, fills between 64 KiB and 1 MiB. */
#ifndef FS_REP_COPY_AVX2
#define FS_REP_COPY_AVX2 8192
#endif
#ifndef FS_REP_SET_AVX2
#define FS_REP_SET_AVX2 (256 << 10)
#endif
#if SPRT_FS_ASM_SIMD
#define FS_REP_COPY 1024
#define FS_REP_SET 1024
#define FS_SMALL 64
#else
#define FS_REP_COPY 256
#define FS_REP_SET 256
#define FS_SMALL 32
#endif

/* The CPU, found by the first call that needs it: bit 0 known, bit 1 AVX2 usable
 * (CPUID, and the OS saving the YMM state: XGETBV), bit 2 AMD or Hygon. FS_CPU_LOAD
 * puts it in r11d and clobbers nothing else. */
#define FS_CPU_LOAD \
	movzbl .Lfs_cpu(%rip), %r11d; \
	test %r11d, %r11d; \
	jnz 9f; \
	call .Lfs_cpu_init; \
9:

/* rep movsb is an order of magnitude slower on AMD Zen when dst - src is 1..15
 * modulo 64; such copies stay in the loop. For n >= FS_REP_COPY. */
#define FS_REP_OR_LOOP(loop) \
	FS_CPU_LOAD \
	test $4, %r11d; \
	jz .Lcpy_rep; \
	cmp $FS_REP_MAX, %rdx; \
	ja loop; \
	mov %rdi, %rcx; \
	sub %rsi, %rcx; \
	and $63, %ecx; \
	dec %ecx; \
	cmp $15, %ecx; \
	jae .Lcpy_rep; \
	jmp loop

/* For n >= FS_REP_SET: rep stosb, or the loop that follows. */
#define FS_SET_REP_OR_LOOP \
	FS_CPU_LOAD \
	test $4, %r11d; \
	jz .Lset_rep; \
	cmp $FS_REP_MAX, %rdx; \
	jbe .Lset_rep

/* FS_IF_AVX2(target) jumps to the AVX2 path when it is usable; it clobbers r11
 * only. The AVX2 paths use VEX encodings throughout and leave by vzeroupper. */
#if SPRT_FS_X86_AVX2 == 2
#define FS_IF_AVX2(target) jmp target
#elif SPRT_FS_X86_AVX2 == 1
#define FS_IF_AVX2(target) \
	FS_CPU_LOAD \
	test $2, %r11d; \
	jnz target
#else
#define FS_IF_AVX2(target)
#endif

	.bss
.Lfs_cpu:
	.byte 0

	.text
	.p2align 4
/* Fills .Lfs_cpu and returns it in r11d; every other register is kept. Calls that
 * race here store the same value. */
.Lfs_cpu_init:
	push %rax
	push %rbx
	push %rcx
	push %rdx
	xor %r11d, %r11d
	xor %eax, %eax
	cpuid
	cmp $0x68747541, %ebx /* "Auth"enticAMD */
	je 2f
	cmp $0x6f677948, %ebx /* "Hygo"nGenuine */
	jne 3f
2:	or $4, %r11d
3:	cmp $7, %eax
	jb 1f
	mov $1, %eax
	cpuid
	and $0x18000000, %ecx /* OSXSAVE, AVX */
	cmp $0x18000000, %ecx
	jne 1f
	xor %ecx, %ecx
	xgetbv
	and $6, %eax /* the OS saves XMM and YMM */
	cmp $6, %eax
	jne 1f
	mov $7, %eax
	xor %ecx, %ecx
	cpuid
	test $0x20, %ebx /* AVX2 */
	jz 1f
	or $2, %r11d
1:	or $1, %r11d
	mov %r11b, .Lfs_cpu(%rip)
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	ret

/* ---- memcpy (rdi dst, rsi src, rdx n) -> rax dst */

FS_FUNC(memcpy)
	mov %rdi, %rax
.Lcpy_body:
	cmp $16, %rdx
	ja .Lcpy_gt16
	cmp $8, %rdx
	jb .Lcpy_lt8
	mov (%rsi), %rcx
	mov -8(%rsi,%rdx), %r8
	mov %rcx, (%rdi)
	mov %r8, -8(%rdi,%rdx)
	ret
.Lcpy_lt8:
	cmp $4, %rdx
	jb .Lcpy_lt4
	mov (%rsi), %ecx
	mov -4(%rsi,%rdx), %r8d
	mov %ecx, (%rdi)
	mov %r8d, -4(%rdi,%rdx)
	ret
.Lcpy_lt4:
	test %rdx, %rdx
	jz .Lcpy_ret
	mov %rdx, %r9
	shr $1, %r9
	movzbl (%rsi), %ecx
	movzbl (%rsi,%r9), %r10d
	movzbl -1(%rsi,%rdx), %r8d
	mov %cl, (%rdi)
	mov %r10b, (%rdi,%r9)
	mov %r8b, -1(%rdi,%rdx)
.Lcpy_ret:
	ret

#if SPRT_FS_ASM_SIMD
.Lcpy_gt16:
	cmp $32, %rdx
	ja .Lcpy_gt32
	movdqu (%rsi), %xmm0
	movdqu -16(%rsi,%rdx), %xmm1
	movdqu %xmm0, (%rdi)
	movdqu %xmm1, -16(%rdi,%rdx)
	ret
.Lcpy_gt32:
	cmp $64, %rdx
	ja .Lcpy_gt64
	movdqu (%rsi), %xmm0
	movdqu 16(%rsi), %xmm1
	movdqu -32(%rsi,%rdx), %xmm2
	movdqu -16(%rsi,%rdx), %xmm3
	movdqu %xmm0, (%rdi)
	movdqu %xmm1, 16(%rdi)
	movdqu %xmm2, -32(%rdi,%rdx)
	movdqu %xmm3, -16(%rdi,%rdx)
	ret
.Lcpy_gt64:
	FS_IF_AVX2(.Lcpy_avx2)
	cmp $FS_REP_COPY, %rdx
	jb .Lcpy_loop
	FS_REP_OR_LOOP(.Lcpy_loop)
.Lcpy_loop:
	/* the last 64 bytes are loaded first and stored last */
	movdqu -64(%rsi,%rdx), %xmm4
	movdqu -48(%rsi,%rdx), %xmm5
	movdqu -32(%rsi,%rdx), %xmm6
	movdqu -16(%rsi,%rdx), %xmm7
	lea -64(%rdi,%rdx), %r8
	lea -64(%rdx), %r9
	mov %rdi, %rcx
1:	movdqu (%rsi), %xmm0
	movdqu 16(%rsi), %xmm1
	movdqu 32(%rsi), %xmm2
	movdqu 48(%rsi), %xmm3
	movdqu %xmm0, (%rcx)
	movdqu %xmm1, 16(%rcx)
	movdqu %xmm2, 32(%rcx)
	movdqu %xmm3, 48(%rcx)
	add $64, %rsi
	add $64, %rcx
	sub $64, %r9
	ja 1b
	movdqu %xmm4, (%r8)
	movdqu %xmm5, 16(%r8)
	movdqu %xmm6, 32(%r8)
	movdqu %xmm7, 48(%r8)
	ret

#if SPRT_FS_X86_AVX2
.Lcpy_avx2:
	cmp $128, %rdx
	ja 1f
.Lcpy_avx2_le128:
	vmovdqu (%rsi), %ymm0
	vmovdqu 32(%rsi), %ymm1
	vmovdqu -64(%rsi,%rdx), %ymm2
	vmovdqu -32(%rsi,%rdx), %ymm3
	vmovdqu %ymm0, (%rdi)
	vmovdqu %ymm1, 32(%rdi)
	vmovdqu %ymm2, -64(%rdi,%rdx)
	vmovdqu %ymm3, -32(%rdi,%rdx)
	vzeroupper
	ret
1:	cmp $FS_REP_COPY_AVX2, %rdx
	jb 2f
	FS_REP_OR_LOOP(2f)
	/* the last 128 bytes are loaded first and stored last */
2:	vmovdqu -128(%rsi,%rdx), %ymm4
	vmovdqu -96(%rsi,%rdx), %ymm5
	vmovdqu -64(%rsi,%rdx), %ymm6
	vmovdqu -32(%rsi,%rdx), %ymm7
	lea -128(%rdi,%rdx), %r8
	lea -128(%rdx), %r9
	mov %rdi, %rcx
3:	vmovdqu (%rsi), %ymm0
	vmovdqu 32(%rsi), %ymm1
	vmovdqu 64(%rsi), %ymm2
	vmovdqu 96(%rsi), %ymm3
	vmovdqu %ymm0, (%rcx)
	vmovdqu %ymm1, 32(%rcx)
	vmovdqu %ymm2, 64(%rcx)
	vmovdqu %ymm3, 96(%rcx)
	sub $-128, %rsi
	sub $-128, %rcx
	sub $128, %r9
	ja 3b
	vmovdqu %ymm4, (%r8)
	vmovdqu %ymm5, 32(%r8)
	vmovdqu %ymm6, 64(%r8)
	vmovdqu %ymm7, 96(%r8)
	vzeroupper
	ret
#endif
#else
.Lcpy_gt16:
	cmp $32, %rdx
	ja .Lcpy_gt32
	mov (%rsi), %rcx
	mov 8(%rsi), %r8
	mov -16(%rsi,%rdx), %r9
	mov -8(%rsi,%rdx), %r10
	mov %rcx, (%rdi)
	mov %r8, 8(%rdi)
	mov %r9, -16(%rdi,%rdx)
	mov %r10, -8(%rdi,%rdx)
	ret
.Lcpy_gt32:
	cmp $FS_REP_COPY, %rdx
	jb .Lcpy_loop
	FS_REP_OR_LOOP(.Lcpy_loop)
.Lcpy_loop:
	/* the last 16 bytes are loaded first and stored last; the loop copies 32
	 * bytes while 32 remain, then 16 more if more than 16 do */
	mov -16(%rsi,%rdx), %r8
	mov -8(%rsi,%rdx), %r9
	lea -32(%rsi,%rdx), %rcx
	lea -16(%rdi,%rdx), %rdx
	push %rax
	mov %rdi, %rax
1:	mov (%rsi), %r10
	mov 8(%rsi), %r11
	mov %r10, (%rax)
	mov %r11, 8(%rax)
	mov 16(%rsi), %r10
	mov 24(%rsi), %r11
	mov %r10, 16(%rax)
	mov %r11, 24(%rax)
	add $32, %rsi
	add $32, %rax
	cmp %rcx, %rsi
	jbe 1b
	add $16, %rcx
	cmp %rcx, %rsi
	jae 2f
	mov (%rsi), %r10
	mov 8(%rsi), %r11
	mov %r10, (%rax)
	mov %r11, 8(%rax)
2:	pop %rax
	mov %r8, (%rdx)
	mov %r9, 8(%rdx)
	ret
#endif

.Lcpy_rep:
	mov %rdx, %rcx
	rep movsb
	ret
FS_END(memcpy)

/* ---- memmove (rdi dst, rsi src, rdx n) -> rax dst */

FS_FUNC(memmove)
	mov %rdi, %rax
	mov %rdi, %rcx
	sub %rsi, %rcx
	cmp %rdx, %rcx
	jae .Lcpy_body /* dst below src, or no overlap: forward */
	cmp $FS_SMALL, %rdx
	jbe .Lcpy_body /* short: everything is loaded before anything is stored */

	/* backward: the first bytes are loaded first and stored last */
#if SPRT_FS_ASM_SIMD
	FS_IF_AVX2(.Lmove_avx2)
	movdqu (%rsi), %xmm4
	movdqu 16(%rsi), %xmm5
	movdqu 32(%rsi), %xmm6
	movdqu 48(%rsi), %xmm7
	add %rdx, %rsi
	lea (%rdi,%rdx), %rcx
	lea -64(%rdx), %r9
1:	sub $64, %rsi
	sub $64, %rcx
	movdqu (%rsi), %xmm0
	movdqu 16(%rsi), %xmm1
	movdqu 32(%rsi), %xmm2
	movdqu 48(%rsi), %xmm3
	movdqu %xmm0, (%rcx)
	movdqu %xmm1, 16(%rcx)
	movdqu %xmm2, 32(%rcx)
	movdqu %xmm3, 48(%rcx)
	sub $64, %r9
	ja 1b
	movdqu %xmm4, (%rdi)
	movdqu %xmm5, 16(%rdi)
	movdqu %xmm6, 32(%rdi)
	movdqu %xmm7, 48(%rdi)
	ret

#if SPRT_FS_X86_AVX2
.Lmove_avx2:
	cmp $128, %rdx
	jbe .Lcpy_avx2_le128
	vmovdqu (%rsi), %ymm4
	vmovdqu 32(%rsi), %ymm5
	vmovdqu 64(%rsi), %ymm6
	vmovdqu 96(%rsi), %ymm7
	add %rdx, %rsi
	lea (%rdi,%rdx), %rcx
	lea -128(%rdx), %r9
1:	add $-128, %rsi
	add $-128, %rcx
	vmovdqu (%rsi), %ymm0
	vmovdqu 32(%rsi), %ymm1
	vmovdqu 64(%rsi), %ymm2
	vmovdqu 96(%rsi), %ymm3
	vmovdqu %ymm0, (%rcx)
	vmovdqu %ymm1, 32(%rcx)
	vmovdqu %ymm2, 64(%rcx)
	vmovdqu %ymm3, 96(%rcx)
	sub $128, %r9
	ja 1b
	vmovdqu %ymm4, (%rdi)
	vmovdqu %ymm5, 32(%rdi)
	vmovdqu %ymm6, 64(%rdi)
	vmovdqu %ymm7, 96(%rdi)
	vzeroupper
	ret
#endif
#else
	mov (%rsi), %r8
	mov 8(%rsi), %r9
	add %rdx, %rsi
	lea (%rdi,%rdx), %rcx
	sub $16, %rdx
1:	sub $16, %rsi
	sub $16, %rcx
	mov 8(%rsi), %r10
	mov (%rsi), %r11
	mov %r10, 8(%rcx)
	mov %r11, (%rcx)
	sub $16, %rdx
	ja 1b
	mov %r8, (%rdi)
	mov %r9, 8(%rdi)
	ret
#endif
FS_END(memmove)

/* ---- memset (rdi dst, esi c, rdx n) -> rax dst */

FS_FUNC(memset)
	mov %rdi, %rax
	cmp $4, %rdx
	jb .Lset_lt4
	movzbl %sil, %ecx
	movabs $0x0101010101010101, %r8
	imul %rcx, %r8
	cmp $16, %rdx
	ja .Lset_gt16
	cmp $8, %rdx
	jb .Lset_lt8
	mov %r8, (%rdi)
	mov %r8, -8(%rdi,%rdx)
	ret
.Lset_lt8:
	mov %r8d, (%rdi)
	mov %r8d, -4(%rdi,%rdx)
	ret
.Lset_lt4:
	test %rdx, %rdx
	jz .Lset_ret
	mov %sil, (%rdi)
	mov %sil, -1(%rdi,%rdx)
	cmp $2, %rdx
	jb .Lset_ret
	mov %sil, 1(%rdi)
.Lset_ret:
	ret

#if SPRT_FS_ASM_SIMD
.Lset_gt16:
	movq %r8, %xmm0
	punpcklqdq %xmm0, %xmm0
	cmp $32, %rdx
	ja .Lset_gt32
	movdqu %xmm0, (%rdi)
	movdqu %xmm0, -16(%rdi,%rdx)
	ret
.Lset_gt32:
	cmp $64, %rdx
	ja .Lset_gt64
	movdqu %xmm0, (%rdi)
	movdqu %xmm0, 16(%rdi)
	movdqu %xmm0, -32(%rdi,%rdx)
	movdqu %xmm0, -16(%rdi,%rdx)
	ret
.Lset_gt64:
	FS_IF_AVX2(.Lset_avx2)
	cmp $FS_REP_SET, %rdx
	jb 2f
	FS_SET_REP_OR_LOOP
	/* the first 16 and the last 64 bytes unaligned, aligned stores between */
2:	movdqu %xmm0, -64(%rdi,%rdx)
	movdqu %xmm0, -48(%rdi,%rdx)
	movdqu %xmm0, -32(%rdi,%rdx)
	movdqu %xmm0, -16(%rdi,%rdx)
	movdqu %xmm0, (%rdi)
	lea -64(%rdi,%rdx), %r9
	add $16, %rdi
	and $-16, %rdi
	jmp 3f
1:	movdqa %xmm0, (%rdi)
	movdqa %xmm0, 16(%rdi)
	movdqa %xmm0, 32(%rdi)
	movdqa %xmm0, 48(%rdi)
	add $64, %rdi
3:	cmp %r9, %rdi
	jb 1b
	ret

#if SPRT_FS_X86_AVX2
.Lset_avx2:
	cmp $128, %rdx
	ja 1f
	vpbroadcastq %xmm0, %ymm0
	vmovdqu %ymm0, (%rdi)
	vmovdqu %ymm0, 32(%rdi)
	vmovdqu %ymm0, -64(%rdi,%rdx)
	vmovdqu %ymm0, -32(%rdi,%rdx)
	vzeroupper
	ret
1:	cmp $FS_REP_SET_AVX2, %rdx
	jb 2f
	FS_SET_REP_OR_LOOP
	/* the first 32 and the last 128 bytes unaligned, aligned stores between */
2:	vpbroadcastq %xmm0, %ymm0
	vmovdqu %ymm0, -128(%rdi,%rdx)
	vmovdqu %ymm0, -96(%rdi,%rdx)
	vmovdqu %ymm0, -64(%rdi,%rdx)
	vmovdqu %ymm0, -32(%rdi,%rdx)
	vmovdqu %ymm0, (%rdi)
	lea -128(%rdi,%rdx), %r9
	add $32, %rdi
	and $-32, %rdi
	jmp 3f
4:	vmovdqa %ymm0, (%rdi)
	vmovdqa %ymm0, 32(%rdi)
	vmovdqa %ymm0, 64(%rdi)
	vmovdqa %ymm0, 96(%rdi)
	sub $-128, %rdi
3:	cmp %r9, %rdi
	jb 4b
	vzeroupper
	ret
#endif
#else
.Lset_gt16:
	cmp $32, %rdx
	ja .Lset_gt32
	mov %r8, (%rdi)
	mov %r8, 8(%rdi)
	mov %r8, -16(%rdi,%rdx)
	mov %r8, -8(%rdi,%rdx)
	ret
.Lset_gt32:
	cmp $FS_REP_SET, %rdx
	jb 2f
	FS_SET_REP_OR_LOOP
2:	mov %r8, -32(%rdi,%rdx)
	mov %r8, -24(%rdi,%rdx)
	mov %r8, -16(%rdi,%rdx)
	mov %r8, -8(%rdi,%rdx)
	lea -32(%rdx), %r9
1:	mov %r8, (%rdi)
	mov %r8, 8(%rdi)
	mov %r8, 16(%rdi)
	mov %r8, 24(%rdi)
	add $32, %rdi
	sub $32, %r9
	ja 1b
	ret
#endif

.Lset_rep:
	mov %rdi, %r9
	movzbl %sil, %eax
	mov %rdx, %rcx
	rep stosb
	mov %r9, %rax
	ret
FS_END(memset)

/* ---- memcmp, bcmp (rdi a, rsi b, rdx n) -> eax */

FS_FUNC(memcmp)
	.global SPRT_FS_NAME(bcmp)
	.type SPRT_FS_NAME(bcmp), @function
SPRT_FS_NAME(bcmp):
	cmp $8, %rdx
	jb .Lcmp_lt8
#if SPRT_FS_ASM_SIMD
	cmp $16, %rdx
	jb .Lcmp_words
	xor %ecx, %ecx
	lea -16(%rdx), %r8
	cmp $64, %rdx
	jb .Lcmp_16
	FS_IF_AVX2(.Lcmp_avx2)
.Lcmp_64:
	/* 64 bytes a step while 64 remain */
	lea -64(%rdx), %r9
3:	movdqu (%rdi,%rcx), %xmm0
	movdqu 16(%rdi,%rcx), %xmm1
	movdqu 32(%rdi,%rcx), %xmm2
	movdqu 48(%rdi,%rcx), %xmm3
	movdqu (%rsi,%rcx), %xmm4
	movdqu 16(%rsi,%rcx), %xmm5
	movdqu 32(%rsi,%rcx), %xmm6
	movdqu 48(%rsi,%rcx), %xmm7
	pcmpeqb %xmm4, %xmm0
	pcmpeqb %xmm5, %xmm1
	pcmpeqb %xmm6, %xmm2
	pcmpeqb %xmm7, %xmm3
	movdqa %xmm0, %xmm4
	pand %xmm1, %xmm4
	pand %xmm2, %xmm4
	pand %xmm3, %xmm4
	pmovmskb %xmm4, %eax
	cmp $0xffff, %eax
	jne 4f
	add $64, %rcx
	cmp %r9, %rcx
	jbe 3b
	jmp .Lcmp_16
4:	pmovmskb %xmm0, %eax
	xor $0xffff, %eax
	jnz .Lcmp_vec_diff
	add $16, %rcx
	pmovmskb %xmm1, %eax
	xor $0xffff, %eax
	jnz .Lcmp_vec_diff
	add $16, %rcx
	pmovmskb %xmm2, %eax
	xor $0xffff, %eax
	jnz .Lcmp_vec_diff
	add $16, %rcx
	pmovmskb %xmm3, %eax
	xor $0xffff, %eax
	jmp .Lcmp_vec_diff
1:	movdqu (%rdi,%rcx), %xmm0
	movdqu (%rsi,%rcx), %xmm1
	pcmpeqb %xmm1, %xmm0
	pmovmskb %xmm0, %eax
	xor $0xffff, %eax
	jnz .Lcmp_vec_diff
	add $16, %rcx
.Lcmp_16:
	cmp %r8, %rcx
	jb 1b
	/* the last 16 bytes, overlapping the ones already compared */
	mov %r8, %rcx
	movdqu (%rdi,%rcx), %xmm0
	movdqu (%rsi,%rcx), %xmm1
	pcmpeqb %xmm1, %xmm0
	pmovmskb %xmm0, %eax
	xor $0xffff, %eax
	jnz .Lcmp_vec_diff
	ret
.Lcmp_vec_diff:
	bsf %eax, %eax
	add %rcx, %rax
	movzbl (%rdi,%rax), %ecx
	movzbl (%rsi,%rax), %edx
	mov %ecx, %eax
	sub %edx, %eax
	ret

#if SPRT_FS_X86_AVX2
	/* 128 bytes a step while 128 remain, then the 16-byte steps */
.Lcmp_avx2:
	cmp $128, %rdx
	jb .Lcmp_64
	lea -128(%rdx), %r9
	jmp 2f
1:	vmovdqu (%rdi,%rcx), %ymm0
	vmovdqu 32(%rdi,%rcx), %ymm1
	vmovdqu 64(%rdi,%rcx), %ymm2
	vmovdqu 96(%rdi,%rcx), %ymm3
	vpcmpeqb (%rsi,%rcx), %ymm0, %ymm0
	vpcmpeqb 32(%rsi,%rcx), %ymm1, %ymm1
	vpcmpeqb 64(%rsi,%rcx), %ymm2, %ymm2
	vpcmpeqb 96(%rsi,%rcx), %ymm3, %ymm3
	vpand %ymm1, %ymm0, %ymm4
	vpand %ymm3, %ymm2, %ymm5
	vpand %ymm5, %ymm4, %ymm4
	vpmovmskb %ymm4, %eax
	cmp $-1, %eax
	jne 3f
	sub $-128, %rcx
2:	cmp %r9, %rcx
	jbe 1b
	vzeroupper
	jmp .Lcmp_16
3:	vpmovmskb %ymm0, %eax
	not %eax
	test %eax, %eax
	jnz 4f
	add $32, %rcx
	vpmovmskb %ymm1, %eax
	not %eax
	test %eax, %eax
	jnz 4f
	add $32, %rcx
	vpmovmskb %ymm2, %eax
	not %eax
	test %eax, %eax
	jnz 4f
	add $32, %rcx
	vpmovmskb %ymm3, %eax
	not %eax
4:	vzeroupper
	jmp .Lcmp_vec_diff
#endif
#endif
.Lcmp_words:
	xor %ecx, %ecx
	lea -8(%rdx), %r8
	jmp 2f
1:	mov (%rdi,%rcx), %r9
	mov (%rsi,%rcx), %r10
	cmp %r10, %r9
	jne .Lcmp_word_diff
	add $8, %rcx
2:	cmp %r8, %rcx
	jb 1b
	mov (%rdi,%r8), %r9
	mov (%rsi,%r8), %r10
	cmp %r10, %r9
	jne .Lcmp_word_diff
	xor %eax, %eax
	ret
.Lcmp_word_diff:
	/* in memory order, the first differing byte decides */
	bswap %r9
	bswap %r10
	cmp %r10, %r9
	sbb %eax, %eax
	or $1, %eax
	ret
.Lcmp_lt8:
	cmp $4, %rdx
	jb .Lcmp_lt4
	mov (%rdi), %r9d
	mov (%rsi), %r10d
	cmp %r10d, %r9d
	jne .Lcmp_dword_diff
	mov -4(%rdi,%rdx), %r9d
	mov -4(%rsi,%rdx), %r10d
	cmp %r10d, %r9d
	jne .Lcmp_dword_diff
	xor %eax, %eax
	ret
.Lcmp_dword_diff:
	bswap %r9d
	bswap %r10d
	cmp %r10d, %r9d
	sbb %eax, %eax
	or $1, %eax
	ret
.Lcmp_lt4:
	/* bytes 0, n/2 and n-1 cover 1..3 bytes in order: compare them as one number */
	xor %eax, %eax
	test %rdx, %rdx
	jz 1f
	mov %rdx, %r8
	shr $1, %r8
	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	shl $16, %eax
	shl $16, %ecx
	movzbl (%rdi,%r8), %r9d
	movzbl (%rsi,%r8), %r10d
	shl $8, %r9d
	shl $8, %r10d
	or %r9d, %eax
	or %r10d, %ecx
	movzbl -1(%rdi,%rdx), %r9d
	movzbl -1(%rsi,%rdx), %r10d
	or %r9d, %eax
	or %r10d, %ecx
	sub %ecx, %eax
1:	ret
FS_END(memcmp)
	.size SPRT_FS_NAME(bcmp), .- SPRT_FS_NAME(bcmp)

/* ---- memchr (rdi s, esi c, rdx n) -> rax */

FS_FUNC(memchr)
	test %rdx, %rdx
	jz .Lchr_null
	/* r8: the end, saturated when s + n wraps */
	mov %rdi, %r8
	add %rdx, %r8
	jnc 1f
	mov $-1, %r8
1:	mov %edi, %ecx
	and $15, %ecx
#if SPRT_FS_ASM_SIMD
	movd %esi, %xmm0
	punpcklbw %xmm0, %xmm0
	punpcklwd %xmm0, %xmm0
	pshufd $0, %xmm0, %xmm0
	and $-16, %rdi
	movdqa (%rdi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shr %cl, %eax /* the bytes before s */
	test %eax, %eax
	jz 2f
	bsf %eax, %eax
	add %rcx, %rax
	add %rdi, %rax
	cmp %r8, %rax
	jae .Lchr_null
	ret
2:	add $16, %rdi
	/* one more block up to a 32-byte boundary: a pair read from there is never
	 * split between pages, so reading past a match cannot fault */
	test $31, %edi
	jz 6f
	cmp %r8, %rdi
	jae .Lchr_null
	movdqa (%rdi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	test %eax, %eax
	jnz .Lchr_found
	add $16, %rdi
6:	FS_IF_AVX2(.Lchr_avx2)
	/* two blocks at a time while both start before the end */
3:	mov %r8, %rdx
	sub %rdi, %rdx
	cmp $32, %rdx
	jbe 4f
	movdqa (%rdi), %xmm1
	movdqa 16(%rdi), %xmm2
	pcmpeqb %xmm0, %xmm1
	pcmpeqb %xmm0, %xmm2
	movdqa %xmm1, %xmm3
	por %xmm2, %xmm3
	pmovmskb %xmm3, %eax
	test %eax, %eax
	jnz 5f
	add $32, %rdi
	jmp 3b
5:	pmovmskb %xmm1, %eax
	test %eax, %eax
	jnz .Lchr_found
	add $16, %rdi
	pmovmskb %xmm2, %eax
	jmp .Lchr_found
4:	cmp %r8, %rdi
	jae .Lchr_null
	movdqa (%rdi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	test %eax, %eax
	jnz .Lchr_found
	add $16, %rdi
	jmp 4b
.Lchr_found:
	bsf %eax, %eax
	add %rdi, %rax
	cmp %r8, %rax
	jae .Lchr_null
	ret

#if SPRT_FS_X86_AVX2
	/* from a 32-byte boundary: one more 32-byte block up to a 64-byte one, then
	 * pairs, which are never split between pages */
.Lchr_avx2:
	vpbroadcastb %xmm0, %ymm0
	test $63, %edi
	jz 1f
	cmp %r8, %rdi
	jae .Lchr_avx2_null
	vpcmpeqb (%rdi), %ymm0, %ymm1
	vpmovmskb %ymm1, %eax
	test %eax, %eax
	jnz .Lchr_avx2_found
	add $32, %rdi
1:	mov %r8, %rdx
	sub %rdi, %rdx
	cmp $64, %rdx
	jbe 2f
	vpcmpeqb (%rdi), %ymm0, %ymm1
	vpcmpeqb 32(%rdi), %ymm0, %ymm2
	vpor %ymm1, %ymm2, %ymm3
	vpmovmskb %ymm3, %eax
	test %eax, %eax
	jnz 3f
	add $64, %rdi
	jmp 1b
3:	vpmovmskb %ymm1, %eax
	test %eax, %eax
	jnz .Lchr_avx2_found
	add $32, %rdi
	vpmovmskb %ymm2, %eax
	jmp .Lchr_avx2_found
2:	cmp %r8, %rdi
	jae .Lchr_avx2_null
	vpcmpeqb (%rdi), %ymm0, %ymm1
	vpmovmskb %ymm1, %eax
	test %eax, %eax
	jnz .Lchr_avx2_found
	add $32, %rdi
	jmp 2b
.Lchr_avx2_found:
	vzeroupper
	bsf %eax, %eax
	add %rdi, %rax
	cmp %r8, %rax
	jae .Lchr_null
	ret
.Lchr_avx2_null:
	vzeroupper
	jmp .Lchr_null
#endif
#else
	movzbl %sil, %esi
	movabs $0x0101010101010101, %r9
	imul %r9, %rsi
	movabs $0x8080808080808080, %r10
	and $7, %ecx
	and $-8, %rdi
	mov (%rdi), %rax
	xor %rsi, %rax
	/* the bytes before s may not match */
	shl $3, %ecx
	mov $1, %r11
	shl %cl, %r11
	dec %r11
	or %r11, %rax
	jmp 2f
1:	add $8, %rdi
	cmp %r8, %rdi
	jae .Lchr_null
	mov (%rdi), %rax
	xor %rsi, %rax
2:	mov %rax, %rdx
	sub %r9, %rdx
	not %rax
	and %rax, %rdx
	and %r10, %rdx
	jz 1b
	/* the lowest flagged byte is exact: a borrow only reaches bytes above a zero */
	bsf %rdx, %rdx
	shr $3, %edx
	lea (%rdi,%rdx), %rax
	cmp %r8, %rax
	jae .Lchr_null
	ret
#endif
.Lchr_null:
	xor %eax, %eax
	ret
FS_END(memchr)

/* ---- strlen (rdi s) -> rax */

FS_FUNC(strlen)
	mov %edi, %ecx
#if SPRT_FS_ASM_SIMD
	and $15, %ecx
	mov %rdi, %rsi
	and $-16, %rsi
	pxor %xmm0, %xmm0
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shr %cl, %eax
	test %eax, %eax
	jz 1f
	bsf %eax, %eax
	ret
	/* single blocks up to a 64-byte boundary, then four at a time: a 64-byte
	 * aligned group is never split between pages */
1:	add $16, %rsi
	test $63, %esi
	jz 6f
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	test %eax, %eax
	jz 1b
	jmp .Llen_found
6:	FS_IF_AVX2(.Llen_avx2)
2:	movdqa (%rsi), %xmm1
	movdqa 16(%rsi), %xmm2
	movdqa 32(%rsi), %xmm3
	movdqa 48(%rsi), %xmm4
	movdqa %xmm1, %xmm5
	pminub %xmm2, %xmm5
	pminub %xmm3, %xmm5
	pminub %xmm4, %xmm5
	pcmpeqb %xmm0, %xmm5
	pmovmskb %xmm5, %eax
	test %eax, %eax
	jnz 3f
	add $64, %rsi
	jmp 2b
3:	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	test %eax, %eax
	jnz .Llen_found
	add $16, %rsi
	pcmpeqb %xmm0, %xmm2
	pmovmskb %xmm2, %eax
	test %eax, %eax
	jnz .Llen_found
	add $16, %rsi
	pcmpeqb %xmm0, %xmm3
	pmovmskb %xmm3, %eax
	test %eax, %eax
	jnz .Llen_found
	add $16, %rsi
	pcmpeqb %xmm0, %xmm4
	pmovmskb %xmm4, %eax
.Llen_found:
	bsf %eax, %eax
	add %rsi, %rax
	sub %rdi, %rax
	ret

#if SPRT_FS_X86_AVX2
	/* from a 64-byte boundary: one more 64-byte group up to a 128-byte one, then
	 * 128 bytes a step */
.Llen_avx2:
	vpxor %xmm0, %xmm0, %xmm0
	test $127, %esi
	jz 1f
	vmovdqa (%rsi), %ymm1
	vmovdqa 32(%rsi), %ymm2
	vpminub %ymm2, %ymm1, %ymm5
	vpcmpeqb %ymm0, %ymm5, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 3f
	add $64, %rsi
1:	vmovdqa (%rsi), %ymm1
	vmovdqa 32(%rsi), %ymm2
	vmovdqa 64(%rsi), %ymm3
	vmovdqa 96(%rsi), %ymm4
	vpminub %ymm2, %ymm1, %ymm5
	vpminub %ymm4, %ymm3, %ymm6
	vpminub %ymm6, %ymm5, %ymm5
	vpcmpeqb %ymm0, %ymm5, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 2f
	sub $-128, %rsi
	jmp 1b
2:	vpcmpeqb %ymm0, %ymm1, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 4f
	add $32, %rsi
	vpcmpeqb %ymm0, %ymm2, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 4f
	add $32, %rsi
	vpcmpeqb %ymm0, %ymm3, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 4f
	add $32, %rsi
	vpcmpeqb %ymm0, %ymm4, %ymm5
	vpmovmskb %ymm5, %eax
	jmp 4f
	/* the zero is in the 64-byte group at rsi */
3:	vpcmpeqb %ymm0, %ymm1, %ymm5
	vpmovmskb %ymm5, %eax
	test %eax, %eax
	jnz 4f
	add $32, %rsi
	vpcmpeqb %ymm0, %ymm2, %ymm5
	vpmovmskb %ymm5, %eax
4:	vzeroupper
	bsf %eax, %eax
	add %rsi, %rax
	sub %rdi, %rax
	ret
#endif
#else
	and $7, %ecx
	mov %rdi, %rsi
	and $-8, %rsi
	movabs $0x0101010101010101, %r9
	movabs $0x8080808080808080, %r10
	mov (%rsi), %rax
	shl $3, %ecx
	mov $1, %r11
	shl %cl, %r11
	dec %r11
	or %r11, %rax
	jmp 2f
1:	add $8, %rsi
	mov (%rsi), %rax
2:	mov %rax, %rdx
	sub %r9, %rdx
	not %rax
	and %rax, %rdx
	and %r10, %rdx
	jz 1b
	bsf %rdx, %rdx
	shr $3, %edx
	lea (%rsi,%rdx), %rax
	sub %rdi, %rax
	ret
#endif
FS_END(strlen)

/* ---- strcmp (rdi a, rsi b) -> eax */

#if SPRT_FS_ASM_SIMD
#define FS_CMP_SHIFT 4
#else
#define FS_CMP_SHIFT 3
#endif

FS_FUNC(strcmp)
#if SPRT_FS_ASM_SIMD
	pxor %xmm2, %xmm2
#if SPRT_FS_X86_AVX2
	/* the first 16 bytes without AVX2, where short strings end: no vzeroupper for
	 * them. Near a page end the SSE2 loop below takes the whole string. */
	mov %edi, %eax
	or $-4096, %eax
	neg %eax
	mov %esi, %ecx
	or $-4096, %ecx
	neg %ecx
	cmp %ecx, %eax
	cmova %ecx, %eax
	cmp $16, %eax
	jb .Lscmp_outer
	movdqu (%rdi), %xmm0
	movdqu (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pcmpeqb %xmm2, %xmm0
	pmovmskb %xmm1, %eax
	pmovmskb %xmm0, %ecx
	xor $0xffff, %eax
	or %ecx, %eax
	jnz .Lscmp_found
	add $16, %rdi
	add $16, %rsi
	FS_IF_AVX2(.Lscmp_avx2)
#endif
#else
	movabs $0x0101010101010101, %r9
	movabs $0x8080808080808080, %r10
#endif
.Lscmp_outer:
	/* r8: how many whole steps both strings take before either reaches the end
	 * of a page; the step that would cross it is taken byte by byte */
	mov %edi, %eax
	or $-4096, %eax
	neg %eax
	mov %esi, %ecx
	or $-4096, %ecx
	neg %ecx
	cmp %ecx, %eax
	cmova %ecx, %eax
	shr $FS_CMP_SHIFT, %eax
	jz .Lscmp_byte
	mov %eax, %r8d
#if SPRT_FS_ASM_SIMD
1:	movdqu (%rdi), %xmm0
	movdqu (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pcmpeqb %xmm2, %xmm0
	pmovmskb %xmm1, %eax
	pmovmskb %xmm0, %ecx
	xor $0xffff, %eax
	or %ecx, %eax
	jnz 2f
	add $16, %rdi
	add $16, %rsi
	dec %r8d
	jnz 1b
	jmp .Lscmp_outer
	/* the first byte that differs or ends a: */
2:
.Lscmp_found:
	bsf %eax, %eax
	movzbl (%rdi,%rax), %ecx
	movzbl (%rsi,%rax), %edx
	mov %ecx, %eax
	sub %edx, %eax
	ret
#else
1:	mov (%rdi), %rax
	mov (%rsi), %rcx
	cmp %rcx, %rax
	jne .Lscmp_bytes
	mov %rax, %rdx
	sub %r9, %rdx
	not %rax
	and %rax, %rdx
	and %r10, %rdx
	jnz .Lscmp_equal
	add $8, %rdi
	add $8, %rsi
	dec %r8d
	jnz 1b
	jmp .Lscmp_outer
.Lscmp_equal:
	xor %eax, %eax
	ret
	/* a difference or a terminator is in these 8 bytes */
.Lscmp_bytes:
	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	sub %ecx, %eax
	jnz 2f
	test %ecx, %ecx
	jz 2f
	inc %rdi
	inc %rsi
	jmp .Lscmp_bytes
2:	ret
#endif
.Lscmp_byte:
	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	sub %ecx, %eax
	jnz 3f
	test %ecx, %ecx
	jz 3f
	inc %rdi
	inc %rsi
	jmp .Lscmp_outer
3:	ret

#if SPRT_FS_X86_AVX2
	/* 32 bytes a step while neither string reaches a page end, 16 (VEX) while one
	 * step of 16 fits, bytes otherwise */
.Lscmp_avx2:
	vpxor %xmm3, %xmm3, %xmm3
.Lscmp_avx2_outer:
	mov %edi, %eax
	or $-4096, %eax
	neg %eax
	mov %esi, %ecx
	or $-4096, %ecx
	neg %ecx
	cmp %ecx, %eax
	cmova %ecx, %eax
	cmp $32, %eax
	jb 5f
	shr $5, %eax
	mov %eax, %r8d
1:	vmovdqu (%rdi), %ymm0
	vpcmpeqb (%rsi), %ymm0, %ymm1
	vpcmpeqb %ymm3, %ymm0, %ymm0
	vpmovmskb %ymm1, %eax
	vpmovmskb %ymm0, %ecx
	not %eax
	or %ecx, %eax
	jnz 2f
	add $32, %rdi
	add $32, %rsi
	dec %r8d
	jnz 1b
	jmp .Lscmp_avx2_outer
2:	vzeroupper
	bsf %eax, %eax
	movzbl (%rdi,%rax), %ecx
	movzbl (%rsi,%rax), %edx
	mov %ecx, %eax
	sub %edx, %eax
	ret
5:	cmp $16, %eax
	jb 6f
	vmovdqu (%rdi), %xmm0
	vpcmpeqb (%rsi), %xmm0, %xmm1
	vpcmpeqb %xmm3, %xmm0, %xmm0
	vpmovmskb %xmm1, %eax
	vpmovmskb %xmm0, %ecx
	xor $0xffff, %eax
	or %ecx, %eax
	jnz 2b
	add $16, %rdi
	add $16, %rsi
	jmp .Lscmp_avx2_outer
6:	movzbl (%rdi), %eax
	movzbl (%rsi), %ecx
	sub %ecx, %eax
	jnz 7f
	test %ecx, %ecx
	jz 7f
	inc %rdi
	inc %rsi
	jmp .Lscmp_avx2_outer
7:	vzeroupper
	ret
#endif
FS_END(strcmp)

#endif /* SPRT_FS_HAVE_ASM && __x86_64__ */

#if defined(__x86_64__) && defined(__ELF__)
.section .note.GNU-stack,"",@progbits
#endif
