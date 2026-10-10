#ifdef CONFIG_ARCH_SETJMP_H
struct sigsetjmp_buf_s {
	unsigned long regs[22];
	int mask;
	void *unused;
};
static inline void *__stub_null(void) { return NULL; }
#endif
