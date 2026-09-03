// Freestanding libc: the multibyte decoder is libc_impl's
// (builtin_multibyte.cpp), which keeps a partial code point in `_Char` and the
// decoder phase in `_State`. mbstate_t is an object the CALLER allocates and
// we write through, so the shape has to be the one our decoder assumes -
// neither Embox's bare int nor glibc's opaque pair.

#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME __mbstate_t
#endif

#define __SPRT_MBSTATE_DIRECT 0

// clang-format off
#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffffffffU
#endif
// clang-format on

typedef unsigned long __SPRT_ID(wctype_t);

#ifdef __cplusplus
typedef wchar_t __SPRT_ID(wchar_t);
#else
typedef __WCHAR_TYPE__ __SPRT_ID(wchar_t);
#endif

typedef struct {
	unsigned int _Char;
	unsigned int _State;
} __SPRT_MBSTATE_NAME;
