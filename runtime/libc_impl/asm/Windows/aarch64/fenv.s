// Ported from musl-libc src/fenv/aarch64/fenv.s
// Windows ARM64 uses AAPCS64 (args in x0-x7) like Linux, so the musl bodies
// apply verbatim; only the GAS .type/.hidden directives are dropped (COFF).

// int fegetround(void);
.global fegetround
fegetround:
	mrs x0, fpcr
	and w0, w0, #0xc00000
	ret

// int __fesetround(int r);
.global __fesetround
__fesetround:
	mrs x1, fpcr
	bic w1, w1, #0xc00000
	orr w1, w1, w0
	msr fpcr, x1
	mov w0, #0
	ret

// int fetestexcept(int mask);
.global fetestexcept
fetestexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	and w0, w0, w1
	ret

// int feclearexcept(int mask);
.global feclearexcept
feclearexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	bic w1, w1, w0
	msr fpsr, x1
	mov w0, #0
	ret

// int feraiseexcept(int mask);
.global feraiseexcept
feraiseexcept:
	and w0, w0, #0x1f
	mrs x1, fpsr
	orr w1, w1, w0
	msr fpsr, x1
	mov w0, #0
	ret

// int fegetenv(fenv_t *envp); -- fenv_t = { unsigned fpcr; unsigned fpsr; }
.global fegetenv
fegetenv:
	mrs x1, fpcr
	mrs x2, fpsr
	stp w1, w2, [x0]
	mov w0, #0
	ret

// int fesetenv(const fenv_t *envp);
.global fesetenv
fesetenv:
	mov x1, #0
	mov x2, #0
	cmn x0, #1
	b.eq 1f
	ldp w1, w2, [x0]
1:	msr fpcr, x1
	msr fpsr, x2
	mov w0, #0
	ret
