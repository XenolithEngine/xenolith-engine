#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME __mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 0

#ifdef __LP64__
typedef __SPRT_ID(uint32_t) __SPRT_ID(wctype_t);
#else
typedef unsigned long __SPRT_ID(wctype_t);
#endif

// Darwin's wctrans_t is a plain int (glibc uses const int *); the wctype.h
// bridge forwards SPRT handles to the platform libc, so the ABI must match.
typedef int __SPRT_ID(wctrans_t);
#define __SPRT_WCTRANS_T_DEFINED 1

typedef union {
	char __mbstate8[128];
	long long _mbstateL; /* for alignment */
} __SPRT_MBSTATE_NAME;
