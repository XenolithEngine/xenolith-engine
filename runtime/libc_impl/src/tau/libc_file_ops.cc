// File descriptors of a Tau module are streams of its services (sprt_tau.h); anonymous
// mappings come from libtau's map region.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_ioctl.h>
#include <sprt/c/sys/__sprt_mman.h>
#include <sys/ioctl.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static __sprt_tau_stream *__tau_stream(const __fd_slot *fp) { return static_cast<__sprt_tau_stream *>(fp->handle); }

static ssize_t __tau_result(long r) {
	if (r < 0) {
		__sprt_errno = (int)-r;
		return -1;
	}
	return (ssize_t)r;
}

static unsigned __tau_io_flags(const __fd_slot *fp) {
	return (fp->flags & __SPRT_O_NONBLOCK) ? __SPRT_TAU_IO_NONBLOCK : 0;
}

static ssize_t __file_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *, uint32_t) {
	return __tau_result(__sprt_tau_stream_read(__tau_stream(fp), buf, nbytes, __tau_io_flags(fp)));
}

static ssize_t __file_write(__fd_slot *fp, const void *buf, size_t nbytes, off64_t *, uint32_t) {
	return __tau_result(__sprt_tau_stream_write(__tau_stream(fp), buf, nbytes, __tau_io_flags(fp)));
}

static ssize_t __file_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __file_read(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

// One write for what fits a line buffer: stdout flushes its buffer and the new line in two
// pieces, which the console must see together.
static ssize_t __file_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	char joined[1'024];
	size_t length = 0;
	for (int i = 0; i < iovcnt && length <= sizeof(joined); ++i) {
		length += iov[i].iov_len;
	}
	if (length <= sizeof(joined)) {
		size_t at = 0;
		for (int i = 0; i < iovcnt; ++i) {
			__builtin_memcpy(joined + at, iov[i].iov_base, iov[i].iov_len);
			at += iov[i].iov_len;
		}
		return __file_write(fp, joined, length, nullptr, 0);
	}
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __file_write(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
	}
	return total;
}

static off_t __file_seek(__fd_slot *, off_t, int) {
	__sprt_errno = ESPIPE;
	return -1;
}

static int __file_stat(__fd_slot *, struct __SPRT_STAT_NAME *st) {
	__builtin_memset(st, 0, sizeof(*st));
	st->st_mode = __SPRT_S_IFCHR | 0'666;
	return 0;
}

static int __file_close(__fd_slot *fp) { return (int)__tau_result(__sprt_tau_stream_close(__tau_stream(fp))); }

static int __file_dup(__fd_slot *, int *, uint32_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

// The streams are the module's console, a terminal: stdout then buffers by line, and lines
// of different modules do not break up mid-way.
static int __file_ioctl(__fd_slot *, int, int cmd, intptr_t arg, __fd_ctl_mode) {
	if (cmd == __SPRT_TIOCGWINSZ && arg) {
		auto ws = reinterpret_cast<struct winsize *>(arg);
		*ws = winsize{};
		ws->ws_row = 24;
		ws->ws_col = 80;
		return 0;
	}
	__sprt_errno = ENOTTY;
	return -1;
}

static int __file_chmod(__fd_slot *, mode_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int __file_utimens(__fd_slot *, const struct __SPRT_TIMESPEC_NAME *) {
	__sprt_errno = ENOSYS;
	return -1;
}

static void *__file_mmap(__fd_slot *, void *, size_t, int, int, off_t) {
	__sprt_errno = ENODEV;
	return __SPRT_MAP_FAILED;
}

static int __file_munmap(__fd_slot *, void *, size_t) {
	__sprt_errno = EINVAL;
	return -1;
}

static int __file_msync(__fd_slot *, void *, size_t, int) { return 0; }

void __libc::load_file_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::none;
	ops->fo_read = &__file_read;
	ops->fo_write = &__file_write;
	ops->fo_ioctl = &__file_ioctl;
	ops->fo_dup = &__file_dup;
	ops->fo_close = &__file_close;
	ops->fo_readv = &__file_readv;
	ops->fo_writev = &__file_writev;
	ops->fo_seek = &__file_seek;
	ops->fo_stat = &__file_stat;
	ops->fo_chmod = &__file_chmod;
	ops->fo_utimens = &__file_utimens;
	ops->fo_mmap = &__file_mmap;
	ops->fo_munmap = &__file_munmap;
	ops->fo_msync = &__file_msync;
}

// Zeroed pages anywhere in the map region; there is no fixed placement.
void *__file_mmap_anon(void *addr, size_t length, int, int flags, off_t) {
	if ((flags & __SPRT_MAP_FIXED) && addr) {
		__sprt_errno = ENOTSUP;
		return __SPRT_MAP_FAILED;
	}
	void *p = nullptr;
	int st = __sprt_tau_mem_map(length, 4'096, 0, &p);
	if (st != 0) {
		__sprt_errno = -st;
		return __SPRT_MAP_FAILED;
	}
	return p;
}

int __file_munmap_anon(void *addr, size_t length) { return __sprt_tau_mem_unmap(addr, length); }

} // namespace sprt
