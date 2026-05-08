#include "../../include/__impl_file.h"
#include <string.h>

size_t __fwritex(const unsigned char *__restrict s, size_t l, FILE *__restrict f) {
	size_t i = 0;

	if (!f->wend && __towrite(f)) {
		return 0;
	}

	if (l > f->wend - f->wpos) {
		return f->write(f, s, l);
	}

	if (f->lbf >= 0) {
		/* Match /^(.*\n|)/ */
		for (i = l; i && s[i - 1] != '\n'; i--);
		if (i) {
			size_t n = f->write(f, s, i);
			if (n < i) {
				return n;
			}
			s += i;
			l -= i;
		}
	}

	memcpy(f->wpos, s, l);
	f->wpos += l;
	return l + i;
}

__SPRT_C_FUNC size_t fwrite(const void *__restrict src, size_t size, size_t nmemb,
		FILE *__restrict f) __SPRT_NOEXCEPT {
	size_t k, l = size * nmemb;
	if (!size) {
		nmemb = 0;
	}
	FLOCK(f);
	k = __fwritex((const unsigned char *)src, l, f);
	FUNLOCK(f);
	return k == l ? nmemb : k / size;
}

weak_alias(fwrite, fwrite_unlocked);
