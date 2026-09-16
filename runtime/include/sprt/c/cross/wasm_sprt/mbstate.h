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

// wasm uses the freestanding libc's own multibyte implementation
// (libc_impl/src/builtin_multibyte.cpp), which stores a partial code point in
// `_Char` and the decoder phase in `_State` — the same layout the Windows
// freestanding build uses. (The Linux target maps these to the system mbstate,
// hence its different opaque fields.)
typedef struct {
	unsigned int _Char;
	unsigned int _State;
} __SPRT_MBSTATE_NAME;
