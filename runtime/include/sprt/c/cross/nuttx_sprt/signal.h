// NuttX's sigset_t is a 2-word bitmap over 64 signals, not glibc's 128-byte one,
// and - more sharply - its SIG_DFL/SIG_IGN are not the Linux 0/1 pair. The wrappers
// forward handlers and sigset_t pointers to the NuttX libc without translating
// them, and outside __SPRT_BUILD these macros are what the application passes to
// signal()/sigaction(), so both have to be NuttX's own.

#include <sprt/c/bits/__sprt_uint32_t.h>

// SIG_DFL is 1 only when CONFIG_SIG_DEFAULT is on; without it NuttX has no default
// actions and SIG_DFL collapses onto SIG_IGN == 0. Read the kconfig rather than
// assuming, the same way dir_ptr.h reads CONFIG_NAME_MAX; nuttx/config.h is
// macro-only.
#include <nuttx/config.h>

// clang-format off
#define __SPRT_SIG_ERR  ((void (*)(int))-1)
#define __SPRT_SIG_IGN  ((void (*)(int)) 0) // Linux: 1
#ifdef CONFIG_SIG_DEFAULT
#define __SPRT_SIG_DFL  ((void (*)(int)) 1) // Linux: 0
#define __SPRT_SIG_HOLD ((void (*)(int)) 2)
#else
#define __SPRT_SIG_DFL  ((void (*)(int)) 0) // no default actions: same as SIG_IGN
#define __SPRT_SIG_HOLD ((void (*)(int)) 1)
#endif
// clang-format on

// NuttX: `typedef volatile int sig_atomic_t`. POSIX allows the qualifier, and the
// wrapper's type-identity assert compares against exactly this.
typedef volatile int __SPRT_ID(sig_atomic_t);

#define __SPRT_SIG_ATOMIC_MIN (-1 - __SPRT_INT_MAX)
#define __SPRT_SIG_ATOMIC_MAX __SPRT_INT_MAX

// struct sigset_s { uint32_t _elem[(_NSIG + 31) >> 5]; } with _NSIG == MAX_SIGNO + 1
// == 64. MAX_SIGNO is fixed in NuttX's <signal.h>, not a kconfig.
#define __SPRT_SIGSET_WORDS 2

typedef struct __SPRT_ID(__sigset_t) {
	__SPRT_ID(uint32_t) __bits[__SPRT_SIGSET_WORDS];
} __SPRT_ID(sigset_t);
