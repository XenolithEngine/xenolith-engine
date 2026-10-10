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

// glibc's wctype_t is void* in the 128-bit pointer mode (bits/wctype-wchar.h);
// the hosted wrappers pass it through untranslated, so the sprt ABI mirrors it
#if defined(__e2k__) && defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 8
typedef void *__SPRT_ID(wctype_t);
#else
typedef unsigned long __SPRT_ID(wctype_t);
#endif

#ifdef __cplusplus
typedef wchar_t __SPRT_ID(wchar_t);
#else
typedef __WCHAR_TYPE__ __SPRT_ID(wchar_t);
#endif

typedef struct {
	unsigned __opaque1;
	unsigned __opaque2;
} __SPRT_MBSTATE_NAME;
