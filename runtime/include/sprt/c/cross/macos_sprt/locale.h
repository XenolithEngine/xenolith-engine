typedef struct _xlocale *__SPRT_ID(locale_t);

// macOS/BSD numbers the locale categories differently from glibc and uses an
// independent mask-bit assignment. Since the hosted libc_wrapper forwards the
// SPRT category/mask straight to the platform setlocale()/newlocale(), the SPRT
// values must match Darwin's (validated by static_assert in the locale bridge).
// clang-format off
#define __SPRT_LC_ALL      0
#define __SPRT_LC_COLLATE  1
#define __SPRT_LC_CTYPE    2
#define __SPRT_LC_MONETARY 3
#define __SPRT_LC_NUMERIC  4
#define __SPRT_LC_TIME     5
#define __SPRT_LC_MESSAGES 6

#define __SPRT_LC_COLLATE_MASK  (1<<0)
#define __SPRT_LC_CTYPE_MASK    (1<<1)
#define __SPRT_LC_MESSAGES_MASK (1<<2)
#define __SPRT_LC_MONETARY_MASK (1<<3)
#define __SPRT_LC_NUMERIC_MASK  (1<<4)
#define __SPRT_LC_TIME_MASK     (1<<5)
#define __SPRT_LC_ALL_MASK (__SPRT_LC_COLLATE_MASK | __SPRT_LC_CTYPE_MASK \
		| __SPRT_LC_MESSAGES_MASK | __SPRT_LC_MONETARY_MASK | __SPRT_LC_NUMERIC_MASK \
		| __SPRT_LC_TIME_MASK)
// clang-format on

// Darwin's struct lconv orders int_n_cs_precedes before int_p_sep_by_space
// (the reverse of glibc); select that layout for the SPRT lconv.
#define __SPRT_LCONV_BSD_INTL_ORDER 1
