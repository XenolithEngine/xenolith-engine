// NuttX's locale_t is a plain `void *` (an opaque handle it never dereferences,
// since it carries only the "C" locale), not glibc's `struct __locale_struct *`.
// The wrapper hands newlocale()/duplocale()/uselocale() results straight through
// to the libc, so the pointee type has to be the one NuttX declares or the bridge
// does not even compile.
typedef void *__SPRT_ID(locale_t);

// Third international-currency field order, alongside glibc's and BSD's: NuttX
// <locale.h> groups the six int_* flags negative-first (int_n_cs_precedes,
// int_n_sep_by_space, int_n_sign_posn, then the int_p_* triple) rather than
// interleaving them. localeconv() casts the platform's `struct lconv *` to the
// SPRT type, so getting this wrong silently returns a different currency flag
// than the caller asked for. See struct __sprt_lconv in <sprt/c/__sprt_locale.h>.
#define __SPRT_LCONV_NUTTX_INTL_ORDER 1
