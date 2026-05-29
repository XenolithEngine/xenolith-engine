#include "../../include/__impl_file.h"
#include <errno.h>

int __fseeko_unlocked(FILE *f, off_t off, int whence) {
	/* Fail immediately for invalid whence argument. */
	if (whence != SEEK_CUR && whence != SEEK_SET && whence != SEEK_END) {
		errno = EINVAL;
		return -1;
	}

	/* Adjust relative offset for unread data in buffer, if any. */
	if (whence == SEEK_CUR && f->rend) {
		off -= f->rend - f->rpos;
	}

	/* Flush write buffer, and report error on failure. */
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) {
			return -1;
		}
	}

	/* Leave writing mode */
	f->wpos = f->wbase = f->wend = 0;

	/* Perform the underlying seek. */
	if (f->seek(f, off, whence) < 0) {
		return -1;
	}

	/* If seek succeeded, file is seekable and we discard read buffer. */
	f->rpos = f->rend = 0;
	f->flags &= ~F_EOF;

	return 0;
}

__SPRT_C_FUNC int fseeko(FILE *f, off_t off, int whence) __SPRT_NOEXCEPT {
	int result;
	FLOCK(f);
	result = __fseeko_unlocked(f, off, whence);
	FUNLOCK(f);
	return result;
}

__SPRT_C_FUNC int fseek(FILE *f, off_t off, int whence) __SPRT_NOEXCEPT {
	return fseeko(f, off, whence);
}

off_t __ftello_unlocked(FILE *f) {
	off_t pos = f->seek(f, 0, (f->flags & F_APP) && f->wpos != f->wbase ? SEEK_END : SEEK_CUR);
	if (pos < 0) {
		return pos;
	}

	/* Adjust for data in buffer. */
	if (f->rend) {
		pos += f->rpos - f->rend;
	} else if (f->wbase) {
		pos += f->wpos - f->wbase;
	}
	return pos;
}

__SPRT_C_FUNC off_t ftello(FILE *f) __SPRT_NOEXCEPT {
	off_t pos;
	FLOCK(f);
	pos = __ftello_unlocked(f);
	FUNLOCK(f);
	return pos;
}

__SPRT_C_FUNC off_t ftell(FILE *f) __SPRT_NOEXCEPT { return ftello(f); }

__SPRT_C_FUNC void rewind(FILE *f) __SPRT_NOEXCEPT {
	FLOCK(f);
	__fseeko_unlocked(f, 0, SEEK_SET);
	f->flags &= ~F_ERR;
	FUNLOCK(f);
}
