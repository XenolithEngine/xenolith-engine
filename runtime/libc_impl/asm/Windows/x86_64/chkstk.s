# Original source: https://github.com/skeeto/w64devkit
.globl __chkstk
__chkstk:
	push %rax
	push %rcx
	mov  %gs:(0x10), %rcx	// rcx = stack low address
	neg  %rax		// rax = frame low address
	add  %rsp, %rax		// "
	jb   1f			// frame low address overflow?
	xor  %eax, %eax		// overflowed: frame low address = null
0:	sub  $0x1000, %rcx	// extend stack into guard page
	test %eax, (%rcx)	// commit page (two instruction bytes)
1:	cmp  %rax, %rcx
	ja   0b
	pop  %rcx
	pop  %rax
	ret
