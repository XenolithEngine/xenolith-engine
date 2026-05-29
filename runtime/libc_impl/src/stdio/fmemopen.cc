#include "../../include/__impl_file.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
#include "libc.h"

struct fmemopen_cookie {
	size_t pos, len, size;
	unsigned char *buf;
	int mode;
};

struct mem_FILE {
	FILE f;
	struct fmemopen_cookie c;
	unsigned char buf[UNGET + BUFSIZ], buf2[];
};

static off_t mseek(FILE *f, off_t off, int whence) {
	ssize_t base;
	struct fmemopen_cookie *c = (fmemopen_cookie *)f->cookie;
	if (whence > 2U) {
fail:
		errno = EINVAL;
		return -1;
	}
	base = (size_t[3]){0, c->pos, c->len}[whence];
	if (off < -base || off > (ssize_t)c->size - base) {
		goto fail;
	}
	return c->pos = base + off;
}

static size_t mread(FILE *f, unsigned char *buf, size_t len) {
	struct fmemopen_cookie *c = (fmemopen_cookie *)f->cookie;
	size_t rem = c->len - c->pos;
	if (c->pos > c->len) {
		rem = 0;
	}
	if (len > rem) {
		len = rem;
		f->flags |= F_EOF;
	}
	sprt::memcpy(buf, c->buf + c->pos, len);
	c->pos += len;
	rem -= len;
	if (rem > f->buf_size) {
		rem = f->buf_size;
	}
	f->rpos = f->buf;
	f->rend = f->buf + rem;
	sprt::memcpy(f->rpos, c->buf + c->pos, rem);
	c->pos += rem;
	return len;
}

static size_t mwrite(FILE *f, const unsigned char *buf, size_t len) {
	struct fmemopen_cookie *c = (fmemopen_cookie *)f->cookie;
	size_t rem;
	size_t len2 = f->wpos - f->wbase;
	if (len2) {
		f->wpos = f->wbase;
		if (mwrite(f, f->wpos, len2) < len2) {
			return 0;
		}
	}
	if (c->mode == 'a') {
		c->pos = c->len;
	}
	rem = c->size - c->pos;
	if (len > rem) {
		len = rem;
	}
	sprt::memcpy(c->buf + c->pos, buf, len);
	c->pos += len;
	if (c->pos > c->len) {
		c->len = c->pos;
		if (c->len < c->size) {
			c->buf[c->len] = 0;
		} else if ((f->flags & F_NORD) && c->size) {
			c->buf[c->size - 1] = 0;
		}
	}
	return len;
}

static int mclose(FILE *m) { return 0; }

__SPRT_C_FUNC FILE *fmemopen(void *__restrict buf, size_t size,
		const char *__restrict mode) __SPRT_NOEXCEPT {
	struct mem_FILE *f;
	int plus = !!sprt::strchr(mode, '+');

	if (!sprt::strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	if (!buf && size > PTRDIFF_MAX) {
		errno = ENOMEM;
		return 0;
	}

	f = (mem_FILE *)malloc(sizeof *f + (buf ? 0 : size));
	if (!f) {
		return 0;
	}
	sprt::memset(f, 0, offsetof(struct mem_FILE, buf));
	f->f.cookie = &f->c;
	f->f.fd = -1;
	f->f.lbf = EOF;
	f->f.buf = f->buf + UNGET;
	f->f.buf_size = sizeof f->buf - UNGET;
	if (!buf) {
		buf = f->buf2;
		sprt::memset(buf, 0, size);
	}

	f->c.buf = (unsigned char *)buf;
	f->c.size = size;
	f->c.mode = *mode;

	if (!plus) {
		f->f.flags = (*mode == 'r') ? F_NOWR : F_NORD;
	}
	if (*mode == 'r') {
		f->c.len = size;
	} else if (*mode == 'a') {
		f->c.len = f->c.pos = sprt::strnlen((const char *)buf, size);
	} else if (plus) {
		*f->c.buf = 0;
	}

	f->f.read = mread;
	f->f.write = mwrite;
	f->f.seek = mseek;
	f->f.close = mclose;

	return __ofl_add(&f->f);
}
