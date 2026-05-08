#include <stdio.h>
#include <string.h>

#include "../../include/__impl_file.h"

static int locking_getc(FILE *f) {
	auto tok = f->__lock();
	int c = __getc_unlocked(f);
	f->__unlock(tok);
	return c;
}

static inline int do_getc(FILE *f) {
	if (f->__lock_pid < 0 || f->__lock_pid == __sprt_pthread_get_id_np()) {
		return __getc_unlocked(f);
	}
	return locking_getc(f);
}

__SPRT_C_FUNC int getc_unlocked(FILE *f) __SPRT_NOEXCEPT { return __getc_unlocked(f); }

weak_alias(getc_unlocked, fgetc_unlocked);
weak_alias(getc_unlocked, _IO_getc_unlocked);

__SPRT_C_FUNC int fgetc(FILE *f) __SPRT_NOEXCEPT { return do_getc(f); }

__SPRT_C_FUNC char *fgets(char *__restrict s, int n, FILE *__restrict f) __SPRT_NOEXCEPT {
	char *p = s;
	unsigned char *z;
	size_t k;
	int c;

	FLOCK(f);

	if (n <= 1) {
		f->mode |= f->mode - 1;
		FUNLOCK(f);
		if (n < 1) {
			return 0;
		}
		*s = 0;
		return s;
	}
	n--;

	while (n) {
		if (f->rpos != f->rend) {
			z = (unsigned char *)memchr(f->rpos, '\n', f->rend - f->rpos);
			k = z ? z - f->rpos + 1 : f->rend - f->rpos;
			k = sprt::min(k, size_t(n));
			memcpy(p, f->rpos, k);
			f->rpos += k;
			p += k;
			n -= k;
			if (z || !n) {
				break;
			}
		}
		if ((c = getc_unlocked(f)) < 0) {
			if (p == s || !feof(f)) {
				s = 0;
			}
			break;
		}
		n--;
		if ((*p++ = c) == '\n') {
			break;
		}
	}
	if (s) {
		*p = 0;
	}

	FUNLOCK(f);

	return s;
}

weak_alias(fgets, fgets_unlocked);
