#include "../../include/__impl_file.h"
#include "../../include/__impl_libc.h"
#include <wchar.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

__SPRT_C_FUNC FILE *const stdout;

__SPRT_C_FUNC wint_t fputwc_unlocked(wchar_t c, FILE *f) __SPRT_NOEXCEPT {
	char mbc[MB_LEN_MAX];
	int l;

	if (f->mode <= 0) {
		fwide(f, 1);
	}

	auto old = uselocale(f->locale);

	if (isascii(c)) {
		c = putc_unlocked(c, f);
	} else if (f->wpos + MB_LEN_MAX < f->wend) {
		l = wctomb((char *)f->wpos, c);
		if (l < 0) {
			c = WEOF;
		} else {
			f->wpos += l;
		}
	} else {
		l = wctomb(mbc, c);
		if (l != 0) {
			if (l < 0 || __fwritex((const unsigned char *)mbc, l, f) < l) {
				c = WEOF;
			}
		}
	}
	if (c == WEOF) {
		f->flags |= F_ERR;
	}

	uselocale(old);

	return c;
}

__SPRT_C_FUNC wint_t fputwc(wchar_t c, FILE *f) __SPRT_NOEXCEPT {
	FLOCK(f);
	c = fputwc_unlocked(c, f);
	FUNLOCK(f);
	return c;
}

__SPRT_C_FUNC wint_t putwc(wchar_t c, FILE *f) __SPRT_NOEXCEPT { return fputwc(c, f); }

__SPRT_C_FUNC int putw(int x, FILE *f) __SPRT_NOEXCEPT {
	return (int)fwrite(&x, sizeof x, 1, f) - 1;
}

__SPRT_C_FUNC wint_t putwchar(wchar_t c) __SPRT_NOEXCEPT { return fputwc(c, stdout); }

weak_alias(putwchar, putwchar_unlocked);
weak_alias(fputwc_unlocked, putwc_unlocked);

__SPRT_C_FUNC int fputws(const wchar_t *__restrict ws, FILE *__restrict f) __SPRT_NOEXCEPT {
	unsigned char buf[BUFSIZ];
	size_t l = 0;

	FLOCK(f);

	fwide(f, 1);
	auto old = uselocale(f->locale);

	while (ws && (l = wcsrtombs((char *)buf, (const wchar_t **)&ws, sizeof buf, 0)) + 1 > 1) {
		if (__fwritex(buf, l, f) < l) {
			FUNLOCK(f);
			uselocale(old);
			return -1;
		}
	}

	FUNLOCK(f);

	uselocale(old);
	return l; /* 0 or -1 */
}

weak_alias(fputws, fputws_unlocked);
