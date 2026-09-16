#include <stdlib.h>
#include <locale.h>
#include "shgetc.h"
#include "floatscan.h"
#include "../../include/__impl_file.h"
#include "../../include/__impl_libc.h"

static long double fstrtox(const char *s, char **p, int prec, int radix) {
	FILE f;
	sh_fromstring(&f, s);
	shlim(&f, 0);
	long double y = __floatscan(&f, prec, 1, radix);
	off_t cnt = shcnt(&f);
	if (p) {
		*p = cnt ? (char *)s + cnt : (char *)s;
	}
	return y;
}

// Radix of an explicit locale handle (the global/unset handle falls back to the
// effective locale), used by the _l conversions.
static int __locale_radix(locale_t loc) {
	if (loc == nullptr || loc == __SPRT_LC_GLOBAL_LOCALE) {
		return sprt::__get_effective_numeric_radix();
	}
	return sprt::__get_numeric_radix(loc->data.cat[__SPRT_LC_NUMERIC]);
}

// The plain conversions honour setlocale()/uselocale() via the effective radix.
__SPRT_C_FUNC float strtof(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 0, sprt::__get_effective_numeric_radix());
}

__SPRT_C_FUNC double strtod(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 1, sprt::__get_effective_numeric_radix());
}

__SPRT_C_FUNC long double strtold(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 2, sprt::__get_effective_numeric_radix());
}

__SPRT_C_FUNC float strtof_l(const char *__restrict s, char **__restrict p,
		locale_t loc) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 0, __locale_radix(loc));
}

__SPRT_C_FUNC double strtod_l(const char *__restrict s, char **__restrict p,
		locale_t loc) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 1, __locale_radix(loc));
}

__SPRT_C_FUNC long double strtold_l(const char *__restrict s, char **__restrict p,
		locale_t loc) __SPRT_NOEXCEPT {
	return fstrtox(s, p, 2, __locale_radix(loc));
}
