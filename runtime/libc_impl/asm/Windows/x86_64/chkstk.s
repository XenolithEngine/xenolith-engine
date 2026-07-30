# Original source: https://github.com/skeeto/w64devkit
// _alloca_probe is the same routine under its older MSVC name: clang emits __chkstk, but
// object code produced elsewhere (LLVM's own, for one) still calls _alloca_probe. On x64
// they are one and the same entry, so alias rather than duplicate.
.globl __chkstk
.globl _alloca_probe
_alloca_probe:
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
