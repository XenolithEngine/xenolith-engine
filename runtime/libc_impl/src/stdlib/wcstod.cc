#include "shgetc.h"
#include "floatscan.h"
#include <wchar.h>
#include <wctype.h>
#include "../../include/__impl_file.h"

/* This read function heavily cheats. It knows:
 *  (1) len will always be 1
 *  (2) non-ascii characters don't matter */

static size_t __wcstof_do_read(FILE *f, unsigned char *buf, size_t len) {
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

static int iswspace(wint_t wc) {
	static const wchar_t spaces[] = {' ', '\t', '\n', '\r', 11, 12, 0x0085, 0x2000, 0x2001, 0x2002,
		0x2003, 0x2004, 0x2005, 0x2006, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x205f, 0x3000, 0};
	return wc && wcschr(spaces, wc);
}

static long double wcstox(const wchar_t *s, wchar_t **p, int prec) {
	wchar_t *t = (wchar_t *)s;
	unsigned char buf[64];
	FILE f = {0};
	f.flags = 0;
	f.rpos = f.rend = f.buf = buf + 4;
	f.buf_size = sizeof buf - 4;
	f.read = __wcstof_do_read;
	while (iswspace(*t)) { t++; }
	f.cookie = (void *)t;
	shlim(&f, 0);
	long double y = __floatscan(&f, prec, 1);
	if (p) {
		size_t cnt = shcnt(&f);
		*p = cnt ? t + cnt : (wchar_t *)s;
	}
	return y;
}

__SPRT_C_FUNC float wcstof(const wchar_t *__restrict s, wchar_t **__restrict p) __SPRT_NOEXCEPT {
	return wcstox(s, p, 0);
}

__SPRT_C_FUNC double wcstod(const wchar_t *__restrict s, wchar_t **__restrict p) __SPRT_NOEXCEPT {
	return wcstox(s, p, 1);
}

__SPRT_C_FUNC long double wcstold(const wchar_t *__restrict s,
		wchar_t **__restrict p) __SPRT_NOEXCEPT {
	return wcstox(s, p, 2);
}
