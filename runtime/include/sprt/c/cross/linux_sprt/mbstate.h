#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME __mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 0

#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffff'ffffU
#endif

typedef unsigned long __SPRT_ID(wctype_t);

typedef struct {
	unsigned __opaque1;
	unsigned __opaque2;
} __SPRT_MBSTATE_NAME;
