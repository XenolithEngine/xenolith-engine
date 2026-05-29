#include "../../include/__impl_file.h"
#include "../../include/__impl_libc.h"
#include <wchar.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

__SPRT_C_FUNC FILE *const stdin;

static wint_t __fgetwc_unlocked_internal(FILE *f) {
	wchar_t wc;
	int c;
	size_t l;

	/* Convert character from buffer if possible */
	if (f->rpos != f->rend) {
		l = mbtowc(&wc, (const char *)f->rpos, f->rend - f->rpos);
		if (l + 1 >= 1) {
			f->rpos += l + !l; /* l==0 means 1 byte, null */
			return wc;
		}
	}

	/* Convert character byte-by-byte */
	mbstate_t st = {0};
	unsigned char b;
	int first = 1;
	do {
		b = c = getc_unlocked(f);
		if (c < 0) {
			if (!first) {
				f->flags |= F_ERR;
				errno = EILSEQ;
			}
			return WEOF;
		}
		l = mbrtowc(&wc, (const char *)&b, 1, &st);
		if (l == -1) {
			if (!first) {
				f->flags |= F_ERR;
				ungetc(b, f);
			}
			return WEOF;
		}
		first = 0;
	} while (l == -2);

	return wc;
}

__SPRT_C_FUNC wint_t fgetwc_unlocked(FILE *f) __SPRT_NOEXCEPT {
	if (f->mode <= 0) {
		fwide(f, 1);
	}
	auto old = uselocale(f->locale);
	wchar_t wc = __fgetwc_unlocked_internal(f);
	uselocale(old);
	return wc;
}

__SPRT_C_FUNC wint_t fgetwc(FILE *f) __SPRT_NOEXCEPT {
	wint_t c;
	FLOCK(f);
	c = fgetwc_unlocked(f);
	FUNLOCK(f);
	return c;
}

__SPRT_C_FUNC wint_t getwc(FILE *f) __SPRT_NOEXCEPT { return fgetwc(f); }

__SPRT_C_FUNC wint_t getwchar(void) __SPRT_NOEXCEPT { return fgetwc(stdin); }

weak_alias(getwchar, getwchar_unlocked);
weak_alias(fgetwc_unlocked, getwc_unlocked);

__SPRT_C_FUNC wchar_t *fgetws(wchar_t *__restrict s, int n, FILE *__restrict f) __SPRT_NOEXCEPT {
	wchar_t *p = s;

	if (!n--) {
		return s;
	}

	FLOCK(f);

	for (; n; n--) {
		wint_t c = fgetwc_unlocked(f);
		if (c == WEOF) {
			break;
		}
		*p++ = c;
		if (c == '\n') {
			break;
		}
	}
	*p = 0;
	if (ferror(f)) {
		p = s;
	}

	FUNLOCK(f);

	return (p == s) ? NULL : s;
}

weak_alias(fgetws, fgetws_unlocked);

__SPRT_C_FUNC wint_t ungetwc(wint_t c, FILE *f) __SPRT_NOEXCEPT {
	unsigned char mbc[6];
	int l;

	FLOCK(f);

	if (f->mode <= 0) {
		fwide(f, 1);
	}

	auto old = uselocale(f->locale);

	if (!f->rpos) {
		__toread(f);
	}
	if (!f->rpos || c == WEOF || (l = wcrtomb((char *)mbc, c, 0)) < 0
			|| f->rpos < f->buf - UNGET + l) {
		FUNLOCK(f);
		uselocale(old);
		return WEOF;
	}

	if (isascii(c)) {
		*--f->rpos = c;
	} else {
		sprt::memcpy(f->rpos -= l, mbc, l);
	}

	f->flags &= ~F_EOF;

	FUNLOCK(f);
	uselocale(old);
	return c;
}
