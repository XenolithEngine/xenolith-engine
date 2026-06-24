#ifndef __SPRT_CONFIG_HAVE_EPOLL
#define __SPRT_CONFIG_HAVE_EPOLL 1
#endif

#ifndef __SPRT_CONFIG_HAVE_EVENTFD
#define __SPRT_CONFIG_HAVE_EVENTFD 1
#endif

#ifndef __SPRT_CONFIG_HAVE_SIGNALFD
#define __SPRT_CONFIG_HAVE_SIGNALFD 1
#endif

#ifndef __SPRT_CONFIG_HAVE_TIMERFD
#define __SPRT_CONFIG_HAVE_TIMERFD 1
#endif

#ifndef __SPRT_CONFIG_HAVE_URING
#define __SPRT_CONFIG_HAVE_URING 1
#endif

#ifndef __SPRT_CONFIG_HAVE_FUTEX
#define __SPRT_CONFIG_HAVE_FUTEX 1
#endif

// glibc provides the non-restartable <stdlib.h> multibyte family
// (mbstowcs/mbtowc/wctomb/wcstombs/__ctype_get_mb_cur_max), so forward to it
// instead of stubbing those to ENOSYS as the default does.
#ifndef __SPRT_CONFIG_HAVE_STDLIB_MB
#define __SPRT_CONFIG_HAVE_STDLIB_MB 1
#endif
