// NuttX's wide-character types do not match glibc's: mbstate_t is a 24-byte
// `struct mbstate_s { int __fill[6]; }`, wctype_t is an int rather than an
// unsigned long, and wctrans_t is an int rather than a `const int *`. The wchar
// wrappers hand these straight to the NuttX libc (and outside __SPRT_BUILD they
// are the types the application declares), so the shapes have to agree.

#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#define __SPRT_MBSTATE_DIRECT 0

// clang-format off
#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffffffffU // NuttX spells this ((wint_t)-1); wint_t is a 32-bit int
#endif
// clang-format on

typedef int __SPRT_ID(wctype_t);

// Like Darwin, NuttX's wctrans_t is a plain int (glibc uses const int *); the
// wctype.h bridge forwards SPRT handles to the platform libc, so the ABI must match.
typedef int __SPRT_ID(wctrans_t);
#define __SPRT_WCTRANS_T_DEFINED 1

#ifdef __cplusplus
typedef wchar_t __SPRT_ID(wchar_t);
#else
typedef __WCHAR_TYPE__ __SPRT_ID(wchar_t);
#endif

typedef struct {
	int __fill[6];
} __SPRT_MBSTATE_NAME;
