#define __SPRT_MBSTATE_NAME __SPRT_ID(__mbstate_t)

#ifdef __LP64__
typedef __SPRT_ID(uint32_t) __SPRT_ID(wctype_t);
#else
typedef unsigned long __SPRT_ID(wctype_t);
#endif

typedef union {
	char __mbstate8[128];
	long long _mbstateL; /* for alignment */
} __SPRT_ID(__mbstate_t);
