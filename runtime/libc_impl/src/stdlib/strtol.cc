#include "intscan.h"
#include <locale.h>
#include "shgetc.h"
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include "../../include/__impl_file.h"

static unsigned long long strtox(const char *s, char **p, int base, unsigned long long lim) {
	FILE f;
	sh_fromstring(&f, s);
	shlim(&f, 0);
	unsigned long long y = __intscan(&f, base, 1, lim);
	if (p) {
		size_t cnt = shcnt(&f);
		*p = (char *)s + cnt;
	}
	return y;
}

__SPRT_C_FUNC unsigned long long strtoull(const char *__restrict s, char **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return strtox(s, p, base, ULLONG_MAX);
}

__SPRT_C_FUNC long long strtoll(const char *__restrict s, char **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return strtox(s, p, base, LLONG_MIN);
}

__SPRT_C_FUNC unsigned long strtoul(const char *__restrict s, char **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return strtox(s, p, base, ULONG_MAX);
}

__SPRT_C_FUNC long strtol(const char *__restrict s, char **__restrict p, int base) __SPRT_NOEXCEPT {
	return strtox(s, p, base, 0UL + LONG_MIN);
}

__SPRT_C_FUNC intmax_t strtoimax(const char *__restrict s, char **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return strtoll(s, p, base);
}

__SPRT_C_FUNC uintmax_t strtoumax(const char *__restrict s, char **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return strtoull(s, p, base);
}


__SPRT_C_FUNC unsigned long long strtoull_l(const char *__restrict s, char **__restrict p, int base,
		locale_t) __SPRT_NOEXCEPT {
	return strtoull(s, p, base);
}

__SPRT_C_FUNC long long strtoll_l(const char *__restrict s, char **__restrict p, int base,
		locale_t) __SPRT_NOEXCEPT {
	return strtoll(s, p, base);
}

__SPRT_C_FUNC unsigned long strtoul_l(const char *__restrict s, char **__restrict p, int base,
		locale_t) __SPRT_NOEXCEPT {
	return strtoul(s, p, base);
}

__SPRT_C_FUNC long strtol_l(const char *__restrict s, char **__restrict p, int base,
		locale_t) __SPRT_NOEXCEPT {
	return strtol(s, p, base);
}
