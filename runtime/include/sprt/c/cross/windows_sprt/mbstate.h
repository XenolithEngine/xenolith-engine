#define __SPRT_MBSTATE_NAME mbstate_t

#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffff
#endif

typedef unsigned short __SPRT_ID(wctype_t);

typedef struct {
	unsigned long _Wchar;
	unsigned short _Byte;
	unsigned short _State;
} __SPRT_MBSTATE_NAME;
