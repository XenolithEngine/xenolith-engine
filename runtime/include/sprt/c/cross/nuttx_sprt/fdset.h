// NuttX's fd_set is `struct fd_set_s { uint32_t arr[(FD_SETSIZE + 31) / 32]; }`
// with FD_SETSIZE == OPEN_MAX, i.e. a 32-bit word array sized by the kconfig -
// not glibc's 1024-descriptor array of unsigned long. select() writes the result
// set back through the same pointer the wrapper casts, so both the word size and
// the total size have to match.

#include <sprt/c/bits/__sprt_uint32_t.h>

// FD_SETSIZE is OPEN_MAX, which is CONFIG_LIBC_OPEN_MAX (floored at _POSIX_OPEN_MAX
// == 16 by NuttX's <limits.h>). Read the kconfig directly rather than <limits.h>:
// <limits.h> would need CONFIG_LIBC_OPEN_MAX anyway, and a silently defaulted value
// here would produce an fd_set that compiles but does not match the libc.
#include <nuttx/config.h>

#ifndef CONFIG_LIBC_OPEN_MAX
#error "NuttX CONFIG_LIBC_OPEN_MAX is not visible - fd_set would get the wrong size"
#endif

// clang-format off
#if CONFIG_LIBC_OPEN_MAX < 16
#define __SPRT_FD_SETSIZE 16 // _POSIX_OPEN_MAX floor, as in NuttX <limits.h>
#else
#define __SPRT_FD_SETSIZE CONFIG_LIBC_OPEN_MAX
#endif

#define __SPRT_FD_NUINT32 ((__SPRT_FD_SETSIZE + 31) / 32)
// clang-format on

typedef __SPRT_ID(uint32_t) __SPRT_ID(fd_mask);

typedef struct {
	__SPRT_ID(uint32_t) arr[__SPRT_FD_NUINT32];
} __SPRT_ID(fd_set);

// NuttX indexes by (fd >> 5) / (fd & 0x1f); keep the same arithmetic so a set built
// through these macros is readable by the libc and vice versa.
#define __SPRT_FD_ZERO(s) do { int __i; __SPRT_ID(uint32_t) *__b=(s)->arr;\
	for(__i=__SPRT_FD_NUINT32; __i; __i--) *__b++=0; } while(0)
#define __SPRT_FD_SET(d, s)   ((s)->arr[(d)>>5] |= ((__SPRT_ID(uint32_t))1<<((d)&0x1f)))
#define __SPRT_FD_CLR(d, s)   ((s)->arr[(d)>>5] &= ~((__SPRT_ID(uint32_t))1<<((d)&0x1f)))
#define __SPRT_FD_ISSET(d, s) !!((s)->arr[(d)>>5] & ((__SPRT_ID(uint32_t))1<<((d)&0x1f)))
