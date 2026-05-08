#include "intscan.h"
#include "shgetc.h"
#include <inttypes.h>
#include <limits.h>
#include <wctype.h>
#include <wchar.h>
#include "../../include/__impl_file.h"

/* This read function heavily cheats. It knows:
 *  (1) len will always be 1
 *  (2) non-ascii characters don't matter */

static size_t __wcstol_do_read(FILE *f, unsigned char *buf, size_t len) {
	size_t i;
	const wchar_t *wcs = (const wchar_t *)f->cookie;

	if (!wcs[0]) {
		wcs = L"@";
	}
	for (i = 0; i < f->buf_size && wcs[i]; i++) { f->buf[i] = wcs[i] < 128 ? wcs[i] : '@'; }
	f->rpos = f->buf;
	f->rend = f->buf + i;
	f->cookie = (void *)(wcs + i);

	if (i && len) {
		*buf = *f->rpos++;
		return 1;
	}
	return 0;
}

static unsigned long long wcstox(const wchar_t *s, wchar_t **p, int base, unsigned long long lim) {
	wchar_t *t = (wchar_t *)s;
	unsigned char buf[64];
	FILE f = {0};
	f.flags = 0;
	f.rpos = f.rend = f.buf = buf + 4;
	f.buf_size = sizeof buf - 4;
	f.read = __wcstol_do_read;
	while (iswspace(*t)) { t++; }
	f.cookie = (void *)t;
	shlim(&f, 0);
	unsigned long long y = __intscan(&f, base, 1, lim);
	if (p) {
		size_t cnt = shcnt(&f);
		*p = cnt ? t + cnt : (wchar_t *)s;
	}
	return y;
}

__SPRT_C_FUNC unsigned long long wcstoull(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstox(s, p, base, ULLONG_MAX);
}

__SPRT_C_FUNC long long wcstoll(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstox(s, p, base, LLONG_MIN);
}

__SPRT_C_FUNC unsigned long wcstoul(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstox(s, p, base, ULONG_MAX);
}

__SPRT_C_FUNC long wcstol(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstox(s, p, base, 0UL + LONG_MIN);
}

__SPRT_C_FUNC intmax_t wcstoimax(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstoll(s, p, base);
}

__SPRT_C_FUNC uintmax_t wcstoumax(const wchar_t *__restrict s, wchar_t **__restrict p,
		int base) __SPRT_NOEXCEPT {
	return wcstoull(s, p, base);
}
