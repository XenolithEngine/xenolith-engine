// Embox's fd_set is `struct { long fds_bits[4]; }` (src/compat/posix/include/
// sys/select.h), i.e. a fixed four-word array - 256 descriptors on LP64 - not
// glibc's 1024-descriptor one. select() writes the result set back through the
// same pointer the wrapper casts, so the total size has to match.
//
// The word type and the indexing arithmetic ARE glibc's (long words, fd/LONG_BIT
// and 1L << (fd % LONG_BIT)), so only the size differs.

// clang-format off
#define __SPRT_FD_SETWORDS 4 // Embox _FDSETWORDS
#define __SPRT_FD_SETSIZE  (__SPRT_FD_SETWORDS * 8 * (int)sizeof(long))
// clang-format on

typedef unsigned long __SPRT_ID(fd_mask);

typedef struct {
	unsigned long fds_bits[__SPRT_FD_SETWORDS];
} __SPRT_ID(fd_set);

#define __SPRT_FD_ZERO(s) \
	do { \
		int __i; \
		unsigned long *__b = (s)->fds_bits; \
		for (__i = __SPRT_FD_SETWORDS; __i; __i--) *__b++ = 0; \
	} while (0)
#define __SPRT_FD_SET(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] |= (1UL<<((d)%(8*sizeof(long)))))
#define __SPRT_FD_CLR(d, s)   ((s)->fds_bits[(d)/(8*sizeof(long))] &= ~(1UL<<((d)%(8*sizeof(long)))))
#define __SPRT_FD_ISSET(d, s) !!((s)->fds_bits[(d)/(8*sizeof(long))] & (1UL<<((d)%(8*sizeof(long)))))
