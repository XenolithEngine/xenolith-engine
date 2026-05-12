#include "../../include/__impl_file.h"

/* The behavior of this function is undefined except when it is the first
 * operation on the stream, so the presence or absence of locking is not
 * observable in a program whose behavior is defined. Thus no locking is
 * performed here. No allocation of buffers is performed, but a buffer
 * provided by the caller is used as long as it is suitably sized. */

__SPRT_C_FUNC int setvbuf(FILE *__restrict f, char *__restrict buf, int type,
		size_t size) __SPRT_NOEXCEPT {
	f->lbf = EOF;

	if (type == _IONBF) {
		f->buf_size = 0;
	} else if (type == _IOLBF || type == _IOFBF) {
		if (buf && size >= UNGET) {
			f->buf = (unsigned char *)(buf + UNGET);
			f->buf_size = size - UNGET;
		}
		if (type == _IOLBF && f->buf_size) {
			f->lbf = '\n';
		}
	} else {
		return -1;
	}

	f->flags |= F_SVB;

	return 0;
}

__SPRT_C_FUNC void setbuf(FILE *__restrict f, char *__restrict buf) __SPRT_NOEXCEPT {
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}
