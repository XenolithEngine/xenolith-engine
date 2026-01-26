#define __SPRT_SIG_ERR  ((void (*)(int))-1)
#define __SPRT_SIG_DFL  ((void (*)(int)) 0)
#define __SPRT_SIG_IGN  ((void (*)(int)) 1)

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

typedef struct __SPRT_ID(__sigset_t) {
	unsigned long __bits[128 / sizeof(long)];
} __SPRT_ID(sigset_t);
