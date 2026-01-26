#define __SPRT_SIG_DFL ((void (*)(int))0)     // default signal action
#define __SPRT_SIG_IGN ((void (*)(int))1)     // ignore signal
#define __SPRT_SIG_GET ((void (*)(int))2)     // return current value
#define __SPRT_SIG_SGE ((void (*)(int))3)     // signal gets error
#define __SPRT_SIG_ACK ((void (*)(int))4)
#define __SPRT_SIG_ERR __SPRT_SIG_SGE

typedef int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN __SPRT_INT_MAX
#define __SPRT_SIG_ATOMIC_MAX (-1-__SPRT_INT_MAX)

typedef struct __SPRT_ID(__sigset_t) {
	unsigned long __bits[1];
} __SPRT_ID(sigset_t);
