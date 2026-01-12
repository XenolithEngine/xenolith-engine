typedef struct __SPRT_ID(__jmp_buf_tag) {
	__SPRT_ID(__jmp_buf) __jb;
	unsigned long __fl;
	unsigned long __ss[128 / sizeof(long)];
} __SPRT_ID(jmp_buf)[1];
