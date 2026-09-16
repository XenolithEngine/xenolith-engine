/**
 * @file clone.s
 * @brief Start a thread at EL0: the one place where parent and child are the
 *        same instruction stream (phase L3b).
 *
 * The kernel's clone(220) returns in BOTH threads at the instruction after the
 * `svc`, the way Linux does: the child with x0 = 0 on the stack it was given,
 * the parent with the child's tid. That is why this is assembly -- a C function
 * cannot be entered twice on two stacks, and the child must not touch a frame
 * the parent owns.
 *
 * long __el0_clone_thread(void *stack_top, void *tls, int *ctid,
 *                         void *(*entry)(void *), void *arg);
 *
 * The child runs entry(arg) and then exit(93) -- ITS thread, not the process:
 * the other threads of the program keep running, and the kernel zeroes *ctid
 * and wakes a futex on it once this thread is gone, which is how the joiner
 * learns the stack may be unmapped.
 */
	.text
	.globl	__el0_clone_thread
	.type	__el0_clone_thread, %function
	.balign	4

__el0_clone_thread:
	/* The child's first frame, written by the parent: entry and arg sit at the
	 * top of the child's stack, which is where the child picks them up. */
	sub	x0, x0, #16
	stp	x3, x4, [x0]

	mov	x5, x0			/* child stack */
	mov	x6, x1			/* tls */
	mov	x7, x2			/* ctid */

	/* CLONE_VM|FS|FILES|SIGHAND|THREAD|SETTLS|CHILD_CLEARTID = 0x290f00 */
	movz	x0, #0x0f00
	movk	x0, #0x0029, lsl #16
	mov	x1, x5
	mov	x2, xzr			/* ptid: unused, the parent has the return value */
	mov	x3, x6
	mov	x4, x7
	mov	x8, #220		/* __NR_clone */
	svc	#0

	cbz	x0, 1f
	ret				/* parent: the child's tid, or -errno */

1:
	/* Child. SP is the frame written above; nothing else of the parent's state
	 * is ours. */
	ldp	x1, x0, [sp]		/* x1 = entry, x0 = arg */
	add	sp, sp, #16
	blr	x1

	mov	x0, xzr
	mov	x8, #93			/* __NR_exit -- this thread only */
	svc	#0
2:
	b	2b			/* unreachable: exit does not return */

	.size	__el0_clone_thread, . - __el0_clone_thread
