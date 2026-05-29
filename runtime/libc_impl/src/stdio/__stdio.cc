#include "../../include/__impl_file.h"
#include <unistd.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

static void close_file(FILE *f) {
	if (!f) {
		return;
	}
	sprt_plock_lock(f, 0, nullptr);
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
	}
	if (f->rpos != f->rend) {
		f->seek(f, f->rpos - f->rend, SEEK_CUR);
	}
	sprt_plock_unlock(f, SPRT_LOCK_FLAG_RESOURCE_DIED, nullptr);
}

static size_t __stdio_read(FILE *f, unsigned char *buf, size_t len) {
	struct iovec iov[2] = {{.iov_base = buf, .iov_len = len - !!f->buf_size},
		{.iov_base = f->buf, .iov_len = f->buf_size}};
	ssize_t cnt;

	cnt = iov[0].iov_len ? readv(f->fd, iov, 2) : read(f->fd, iov[1].iov_base, iov[1].iov_len);
	if (cnt <= 0) {
		f->flags |= cnt ? F_ERR : F_EOF;
		return 0;
	}
	if (cnt <= iov[0].iov_len) {
		return cnt;
	}
	cnt -= iov[0].iov_len;
	f->rpos = f->buf;
	f->rend = f->buf + cnt;
	if (f->buf_size) {
		buf[len - 1] = *f->rpos++;
	}
	return len;
}

static size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len) {
	struct iovec iovs[2] = {{.iov_base = f->wbase, .iov_len = size_t(f->wpos - f->wbase)},
		{.iov_base = (void *)buf, .iov_len = len}};
	struct iovec *iov = iovs;
	size_t rem = iov[0].iov_len + iov[1].iov_len;
	int iovcnt = 2;
	ssize_t cnt;

	if (!iov->iov_len) {
		iov++;
		iovcnt--;
	}
	for (;;) {
		cnt = writev(f->fd, iov, iovcnt);
		if (cnt == rem) {
			f->wend = f->buf + f->buf_size;
			f->wpos = f->wbase = f->buf;
			return len;
		}
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			return iovcnt == 2 ? 0 : len - iov[0].iov_len;
		}
		rem -= cnt;
		if (cnt > iov[0].iov_len) {
			cnt -= iov[0].iov_len;
			iov++;
			iovcnt--;
		}
		iov[0].iov_base = (char *)iov[0].iov_base + cnt;
		iov[0].iov_len -= cnt;
	}
}

static size_t __stdout_write(FILE *f, const unsigned char *buf, size_t len) {
	struct winsize wsz;
	f->write = __stdio_write;
	if (!(f->flags & F_SVB) && ioctl(f->fd, TIOCGWINSZ, intptr_t(&wsz))) {
		f->lbf = -1;
	}
	return __stdio_write(f, buf, len);
}

static off_t __stdio_seek(FILE *f, off_t off, int whence) { return lseek(f->fd, off, whence); }

static int __stdio_close(FILE *f) { return close(f->fd); }

static void __stdio_exit(void) {
	FILE *f;
	for (f = *__ofl_lock(); f; f = f->next) { close_file(f); }
	close_file(__stdin_used);
	close_file(__stdout_used);
	close_file(__stderr_used);
}

static unsigned char __stdin_buf[BUFSIZ + UNGET];
FILE __stdin_FILE = {
	.flags = F_PERM | F_NOWR,
	.close = __stdio_close,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.buf = __stdin_buf + UNGET,
	.buf_size = sizeof __stdin_buf - UNGET,
	.fd = 0,
	.__lock_pid = -1,
};

static unsigned char __stdout_buf[BUFSIZ + UNGET];
FILE __stdout_FILE = {
	.flags = F_PERM | F_NORD,
	.close = __stdio_close,
	.write = __stdout_write,
	.seek = __stdio_seek,
	.buf = __stdout_buf + UNGET,
	.buf_size = sizeof __stdout_buf - UNGET,
	.fd = 1,
	.lbf = '\n',
	.__lock_pid = 0,
};

static unsigned char __stderr_buf[UNGET];
FILE __stderr_FILE = {
	.flags = F_PERM | F_NORD,
	.close = __stdio_close,
	.write = __stdio_write,
	.seek = __stdio_seek,
	.buf = __stderr_buf + UNGET,
	.buf_size = 0,
	.fd = 2,
	.lbf = -1,
	.__lock_pid = -1,
};

__SPRT_C_FUNC FILE *fdopen(int fd, const char *mode) __SPRT_NOEXCEPT {
	FILE *f;
	struct winsize wsz;

	/* Check for valid initial mode character */
	if (!sprt::strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+buffer or fail */
	if (!(f = (FILE *)malloc(sizeof *f + UNGET + BUFSIZ))) {
		return 0;
	}

	/* Zero-fill only the struct, not the buffer */
	sprt::memset(f, 0, sizeof *f);

	/* Impose mode restrictions */
	if (!sprt::strchr(mode, '+')) {
		f->flags = (*mode == 'r') ? F_NOWR : F_NORD;
	}

	/* Apply close-on-exec flag */
	if (sprt::strchr(mode, 'e')) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	}

	/* Set append mode on fd if opened for append */
	if (*mode == 'a') {
		int flags = fcntl(fd, F_GETFL);
		if (!(flags & O_APPEND)) {
			fcntl(fd, F_SETFL, flags | O_APPEND);
		}
		f->flags |= F_APP;
	}

	f->fd = fd;
	f->buf = (unsigned char *)f + sizeof *f + UNGET;
	f->buf_size = BUFSIZ;

	/* Activate line buffered mode for terminals */
	f->lbf = EOF;
	if (!(f->flags & F_NOWR) && !ioctl(fd, TIOCGWINSZ, &wsz)) {
		f->lbf = '\n';
	}

	/* Initialize op ptrs. No problem if some are unneeded. */
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;

	/* Add new FILE to open file list */
	return __ofl_add(f);
}

__SPRT_C_FUNC FILE *const stdin = &__stdin_FILE;
__SPRT_C_FUNC FILE *volatile __stdin_used = &__stdin_FILE;

__SPRT_C_FUNC FILE *const stdout = &__stdout_FILE;
__SPRT_C_FUNC FILE *volatile __stdout_used = &__stdout_FILE;

__SPRT_C_FUNC FILE *const stderr = &__stderr_FILE;
__SPRT_C_FUNC FILE *volatile __stderr_used = &__stderr_FILE;

__SPRT_C_FUNC int vdprintf(int fd, const char *__restrict fmt,
		__builtin_va_list ap) __SPRT_NOEXCEPT {
	FILE f = {
		.write = __stdio_write,
		.buf = (unsigned char *)fmt,
		.buf_size = 0,
		.fd = fd,
		.lbf = EOF,
		.__lock_pid = -1,
	};

	return vfprintf(&f, fmt, ap);
}
