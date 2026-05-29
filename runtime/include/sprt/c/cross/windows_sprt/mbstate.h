#define __SPRT_MBSTATE_NAME mbstate_t

#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffff
#endif

#ifdef __cplusplus
typedef wchar_t __SPRT_ID(wchar_t);
#else
typedef unsigned short __SPRT_ID(wchar_t);
#endif

typedef unsigned short __SPRT_ID(wctype_t);

typedef struct {
	unsigned int _Char;
	unsigned int _State;
} __SPRT_MBSTATE_NAME;
