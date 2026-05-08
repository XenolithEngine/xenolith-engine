.global memcpy
memcpy:
	push %rbp
	mov %rsp,%rbp
	sub $0x20,%rsp
	push %rsi
	push %rdi
	mov %rcx,%rax
	mov %rcx,%rdi
	mov %rdx,%rsi
	cmp $8,%r8
	jc 1f
	test $7,%edi
	jz 1f

2:	movsb
	dec %r8
	test $7,%edi
	jnz 2b

1:	mov %r8,%rcx
	shr $3,%rcx
	rep
	movsq
	and $7,%r8d
	jz 1f

2:	movsb
	dec %r8d
	jnz 2b

1:	pop %rdi
	pop %rsi
	mov %rbp,%rsp
	pop %rbp
	ret
