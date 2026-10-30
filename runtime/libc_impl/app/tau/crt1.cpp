// The entry of a Tau module on the shared runtime (RT-A-SharedRuntime-1): root enters it with the
// StartInfo in x0/rdi; the startup in libsprt.so gets it with the module's main.

extern "C" int main(int argc, char **argv, char **envp);
extern "C" [[noreturn]] void __tau_start_shared(const void *startInfo, int (*entry)(int, char **, char **));

#if __x86_64__
__asm__(".text\n"
		".globl _start\n"
		".type _start, @function\n"
		"_start:\n"
		"	xor	%ebp, %ebp\n"
		"	and	$-16, %rsp\n"
		"	lea	main(%rip), %rsi\n"
		"	call	__tau_start_shared@PLT\n"
		"	ud2\n"
		".size _start, . - _start\n");
#else
__asm__(".text\n"
		".globl _start\n"
		".type _start, %function\n"
		"_start:\n"
		"	mov	x29, #0\n"
		"	mov	x30, #0\n"
		"	adrp	x1, main\n"
		"	add	x1, x1, :lo12:main\n"
		"	b	__tau_start_shared\n"
		".size _start, . - _start\n");
#endif
