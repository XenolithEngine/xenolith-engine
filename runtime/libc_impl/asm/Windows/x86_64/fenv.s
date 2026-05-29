
// int feclearexcept(int);
.global feclearexcept
feclearexcept:
		# maintain exceptions in the sse mxcsr, clear x87 exceptions
	mov %ecx,%edx
	and $0x3f,%edx
	fnstsw %ax
	test %eax,%edx
	jz 1f
	fnclex
1:	stmxcsr -8(%rsp)
	and $0x3f,%eax
	or %eax,-8(%rsp)
	test %edx,-8(%rsp)
	jz 1f
	not %edx
	and %edx,-8(%rsp)
	ldmxcsr -8(%rsp)
1:	xor %eax,%eax
	ret

// int feraiseexcept(int);
.global feraiseexcept
feraiseexcept:
	and $0x3f,%ecx
	stmxcsr -8(%rsp)
	or %ecx,-8(%rsp)
	ldmxcsr -8(%rsp)
	xor %eax,%eax
	ret

// int __fesetround(int r)
.global __fesetround
__fesetround:
	push %rax
	xor %eax,%eax
	fnstcw (%rsp)
	andb $0xf3,1(%rsp)
	or %ch,1(%rsp)
	fldcw (%rsp)
	stmxcsr (%rsp)
	shl $3,%ch
	andb $0x9f,1(%rsp)
	or %ch,1(%rsp)
	ldmxcsr (%rsp)
	pop %r10
	ret
	
// int fegetround()
.global fegetround
fegetround:
	push %rax
	stmxcsr (%rsp)
	pop %rax
	shr $3,%eax
	and $0xc00,%eax
	ret

// int fegetenv(fenv_t *);
.global fegetenv
fegetenv:
	xor %eax,%eax
	fnstenv (%rcx)
	stmxcsr 28(%rcx)
	ret

// int fesetenv(const fenv_t *);
.global fesetenv
fesetenv:
	xor %eax,%eax
	inc %rcx
	jz 1f
	fldenv -1(%rcx)
	ldmxcsr 27(%rcx)
	ret
1:	push %rax
	push %rax
	pushq $0xffff
	pushq $0x37f
	fldenv (%rsp)
	pushq $0x1f80
	ldmxcsr (%rsp)
	add $40,%rsp
	ret

// int fetestexcept(int);
.global fetestexcept
fetestexcept:
	and $0x3f,%ecx
	push %rax
	stmxcsr (%rsp)
	pop %rdx
	fnstsw %ax
	or %edx,%eax
	and %ecx,%eax
	ret
