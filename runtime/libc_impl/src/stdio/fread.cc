#include "../../include/__impl_file.h"
#include <string.h>

#define MIN(a, b) ((a)<(b) ? (a) : (b))

__SPRT_C_FUNC size_t fread(void *__restrict destv, size_t size, size_t nmemb,
		FILE *__restrict f) __SPRT_NOEXCEPT {
	unsigned char *dest = (unsigned char *)destv;
	size_t len = size * nmemb, l = len, k;
	if (!size) {
		nmemb = 0;
	}

	FLOCK(f);

	f->mode |= f->mode - 1;

	if (f->rpos != f->rend) {
		/* First exhaust the buffer. */
		k = MIN(f->rend - f->rpos, l);
		sprt::memcpy(dest, f->rpos, k);
		f->rpos += k;
		dest += k;
		l -= k;
	}

	/* Read the remainder directly */
	for (; l; l -= k, dest += k) {
		k = __toread(f) ? 0 : f->read(f, dest, l);
		if (!k) {
			FUNLOCK(f);
			return (len - l) / size;
		}
	}

	FUNLOCK(f);
	return nmemb;
}

weak_alias(fread, fread_unlocked);
