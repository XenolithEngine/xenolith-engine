#define __SPRT_SIG_ERR  ((void (*)(int))-1)
#define __SPRT_SIG_DFL  ((void (*)(int)) 0)
#define __SPRT_SIG_IGN  ((void (*)(int)) 1)
// Extra sentinel handlers used by the freestanding signal() implementation
// (libc_impl/src/builtin_signal.cpp): SIG_GET queries the current handler and
// SIG_ACK acknowledges one. Distinct from DFL/IGN/ERR above.
#define __SPRT_SIG_GET  ((void (*)(int)) 2)
#define __SPRT_SIG_ACK  ((void (*)(int)) 4)

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

#define __SPRT_SIGSET_WORDS (128 / sizeof(long))

typedef struct __SPRT_ID(__sigset_t) {
	unsigned long __bits[__SPRT_SIGSET_WORDS];
} __SPRT_ID(sigset_t);
