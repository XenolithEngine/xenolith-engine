#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 1

#ifndef __SPRT_WEOF
#define __SPRT_WEOF (__sprt_wint_t)(-1)
#endif

typedef long __SPRT_ID(wctype_t);

#ifdef __cplusplus
typedef wchar_t __SPRT_ID(wchar_t);
#else
typedef unsigned int __SPRT_ID(wchar_t);
#endif

typedef struct {
	unsigned char __seq[4];
#ifdef __LP64__
	unsigned char __reserved[4];
#endif
} __SPRT_MBSTATE_NAME;
