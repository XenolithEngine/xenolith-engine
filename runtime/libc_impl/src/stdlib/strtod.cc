#include <stdlib.h>
#include "shgetc.h"
#include "floatscan.h"
#include "../../include/__impl_file.h"

static long double strtox(const char *s, char **p, int prec) {
	FILE f;
	sh_fromstring(&f, s);
	shlim(&f, 0);
	long double y = __floatscan(&f, prec, 1);
	off_t cnt = shcnt(&f);
	if (p) {
		*p = cnt ? (char *)s + cnt : (char *)s;
	}
	return y;
}

__SPRT_C_FUNC float strtof(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return strtox(s, p, 0);
}

__SPRT_C_FUNC double strtod(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return strtox(s, p, 1);
}

__SPRT_C_FUNC long double strtold(const char *__restrict s, char **__restrict p) __SPRT_NOEXCEPT {
	return strtox(s, p, 2);
}
