#ifndef CORE_RUNTIME_INCLUDE_C_BITS_FDSET_H_
#define CORE_RUNTIME_INCLUDE_C_BITS_FDSET_H_

#include <c/bits/__sprt_def.h>

#define __SPRT_FD_SETSIZE 1'024

typedef unsigned long __SPRT_ID(fd_mask);

typedef struct {
	unsigned long fds_bits[__SPRT_FD_SETSIZE / 8 / sizeof(long)];
} __SPRT_ID(fd_set);

#define __SPRT_FD_ZERO(s) do { int __i; unsigned long *__b=(s)->fds_bits;\
	for(__i=sizeof (__SPRT_ID(fd_set))/sizeof (long); __i; __i--) *__b++=0; } while(0)
#define __SPRT_FD_SET(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] |= (1UL<<((d)%(8*sizeof(long)))))
#define __SPRT_FD_CLR(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] &= ~(1UL<<((d)%(8*sizeof(long)))))
#define __SPRT_FD_ISSET(d, s) !!((s)->fds_bits[(d)/(8*sizeof(long))] & (1UL<<((d)%(8*sizeof(long)))))

#endif
