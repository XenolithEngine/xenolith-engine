// dma-buf and sync_file descriptors of a Tau module (gpufd.h): the handle of a descriptor is
// counted, a dup shares it. A dma-buf's handle holds one reference to its libtau buffer; a
// sync_file's holds its points and the session they belong to, and once seen reached stays so
// when that session is gone. Included by builtin_libc.cpp.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/sys/__sprt_poll.h>
#include <sprt/c/cross/tau_sprt/gpufd.h>

#include <sys/stat.h>

namespace sprt {

struct __tau_dmabuf {
	uint32_t refs;
	__sprt_tau_gpu_buf *buf;
};

struct __tau_syncfile {
	uint32_t refs;
	uint32_t count;
	uint32_t signaled;
	unsigned long long session;
	__sprt_tau_gpu_point points[];
};

static constexpr unsigned TAU_SYNCFILE_MAX = 4'096;

static int __tau_gpu_ret(int r) {
	if (r < 0) {
		__sprt_errno = -r;
		return -1;
	}
	return r;
}

static void __tau_handle_ref(uint32_t *refs) { __atomic_add_fetch(refs, 1, __ATOMIC_RELAXED); }

// True when the last reference went.
static bool __tau_handle_unref(uint32_t *refs) {
	return __atomic_sub_fetch(refs, 1, __ATOMIC_ACQ_REL) == 0;
}

// Puts the handle of `fp` at a new descriptor, or at *target closing what is there.
static int __tau_gpufd_dup(__fd_slot *fp, int *target, uint32_t flags, uint32_t *refs) {
	auto libc = __libc::get();
	uint32_t newFlags = (flags & __SPRT_FD_CLOEXEC) ? (fp->flags | __SPRT_O_CLOEXEC)
													: (fp->flags & ~uint32_t(__SPRT_O_CLOEXEC));
	if (!target) {
		__tau_handle_ref(refs);
		int fd = libc->create_fd(fp->handle, fp->ops, newFlags, fp->mode);
		if (fd < 0) {
			__tau_handle_unref(refs);
		}
		return fd;
	}
	if (*target < 0 || size_t(*target) >= MAX_FDS || !libc->get_fd_slot(*target)) {
		__sprt_errno = EBADF;
		return -1;
	}
	__tau_handle_ref(refs);
	unique_lock lock(libc->fdMutex);
	libc->fdDispatch->bits.set(*target);
	auto slot = libc->get_fd_slot(*target);
	if (slot->handle && slot->ops && slot->ops->fo_close) {
		slot->ops->fo_close(slot);
	}
	*slot = __fd_slot{.handle = fp->handle, .ops = fp->ops, .flags = newFlags, .mode = fp->mode};
	return *target;
}

static int __tau_gpufd_fcntl(__fd_slot *fp, int fd, int cmd, intptr_t arg, uint32_t *refs) {
	switch (cmd) {
	case __SPRT_F_DUPFD: return __tau_gpufd_dup(fp, nullptr, 0, refs);
	case __SPRT_F_DUPFD_CLOEXEC: return __tau_gpufd_dup(fp, nullptr, __SPRT_FD_CLOEXEC, refs);
	case __SPRT_F_GETFL: return int(fp->flags & ~uint32_t(__SPRT_O_CLOEXEC));
	case __SPRT_F_GETFD: return (fp->flags & __SPRT_O_CLOEXEC) ? __SPRT_FD_CLOEXEC : 0;
	case __SPRT_F_SETFD:
		fp->flags = (arg & __SPRT_FD_CLOEXEC) ? (fp->flags | __SPRT_O_CLOEXEC)
											  : (fp->flags & ~uint32_t(__SPRT_O_CLOEXEC));
		return 0;
	default: __sprt_errno = EINVAL; return -1;
	}
}

// dma-buf

static __tau_dmabuf *__tau_dmabuf_of(const __fd_slot *fp) {
	return static_cast<__tau_dmabuf *>(fp->handle);
}

static int __tau_dmabuf_close(__fd_slot *fp) {
	auto d = __tau_dmabuf_of(fp);
	if (__tau_handle_unref(&d->refs)) {
		__sprt_tau_gpu_buf_release(d->buf);
		__sprt_free(d);
	}
	fp->handle = nullptr;
	return 0;
}

static int __tau_dmabuf_dup(__fd_slot *fp, int *target, uint32_t flags) {
	return __tau_gpufd_dup(fp, target, flags, &__tau_dmabuf_of(fp)->refs);
}

static int __tau_dmabuf_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	if (mode == __fd_ctl_mode::fnctl) {
		return __tau_gpufd_fcntl(fp, fd, cmd, arg, &__tau_dmabuf_of(fp)->refs);
	}
	__sprt_errno = ENOTTY;
	return -1;
}

static off_t __tau_dmabuf_seek(__fd_slot *fp, off_t off, int whence) {
	__sprt_tau_gpu_bo_desc desc;
	if (__tau_gpu_ret(__sprt_tau_gpu_buf_desc(__tau_dmabuf_of(fp)->buf, &desc)) < 0) {
		return -1;
	}
	if (off == 0 && whence == __SPRT_SEEK_END) {
		return off_t(desc.size);
	}
	if (off == 0 && whence == __SPRT_SEEK_SET) {
		return 0;
	}
	__sprt_errno = EINVAL;
	return -1;
}

static int __tau_dmabuf_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	__sprt_tau_gpu_bo_desc desc;
	if (__tau_gpu_ret(__sprt_tau_gpu_buf_desc(__tau_dmabuf_of(fp)->buf, &desc)) < 0) {
		return -1;
	}
	__builtin_memset(st, 0, sizeof(*st));
	st->st_mode = __SPRT_S_IFREG | 0'600;
	st->st_size = off_t(desc.size);
	return 0;
}

// The bytes are where the heap is mapped: the mapping is the session's, unmapping keeps it.
static void *__tau_dmabuf_mmap(__fd_slot *fp, void *, size_t length, int, int flags, off_t offset) {
	__sprt_tau_gpu_bo_desc desc;
	if (__tau_gpu_ret(__sprt_tau_gpu_buf_desc(__tau_dmabuf_of(fp)->buf, &desc)) < 0) {
		return __SPRT_MAP_FAILED;
	}
	if (!desc.data) {
		__sprt_errno = ENODEV;
		return __SPRT_MAP_FAILED;
	}
	if (!(flags & __SPRT_MAP_SHARED) || offset < 0 || (flags & __SPRT_MAP_FIXED)
			|| uint64_t(offset) > desc.size || length > desc.size - uint64_t(offset)) {
		__sprt_errno = EINVAL;
		return __SPRT_MAP_FAILED;
	}
	return static_cast<char *>(desc.data) + offset;
}

static int __tau_dmabuf_munmap(__fd_slot *, void *, size_t) { return 0; }

static int __tau_dmabuf_msync(__fd_slot *, void *, size_t, int) { return 0; }

static const __fd_ops *__tau_dmabuf_ops() {
	static __fd_ops ops = [] {
		__fd_ops o;
		o.fo_ioctl = &__tau_dmabuf_ioctl;
		o.fo_dup = &__tau_dmabuf_dup;
		o.fo_close = &__tau_dmabuf_close;
		o.fo_seek = &__tau_dmabuf_seek;
		o.fo_stat = &__tau_dmabuf_stat;
		o.fo_mmap = &__tau_dmabuf_mmap;
		o.fo_munmap = &__tau_dmabuf_munmap;
		o.fo_msync = &__tau_dmabuf_msync;
		return o;
	}();
	return &ops;
}

// sync_file

static __tau_syncfile *__tau_syncfile_of(const __fd_slot *fp) {
	return static_cast<__tau_syncfile *>(fp->handle);
}

static int __tau_syncfile_close(__fd_slot *fp) {
	auto f = __tau_syncfile_of(fp);
	if (__tau_handle_unref(&f->refs)) {
		__sprt_free(f);
	}
	fp->handle = nullptr;
	return 0;
}

static int __tau_syncfile_dup(__fd_slot *fp, int *target, uint32_t flags) {
	return __tau_gpufd_dup(fp, target, flags, &__tau_syncfile_of(fp)->refs);
}

static int __tau_syncfile_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	if (mode == __fd_ctl_mode::fnctl) {
		return __tau_gpufd_fcntl(fp, fd, cmd, arg, &__tau_syncfile_of(fp)->refs);
	}
	__sprt_errno = ENOTTY;
	return -1;
}

static int __tau_syncfile_stat(__fd_slot *, struct __SPRT_STAT_NAME *st) {
	__builtin_memset(st, 0, sizeof(*st));
	st->st_mode = __SPRT_S_IFREG | 0'600;
	return 0;
}

static const __fd_ops *__tau_syncfile_ops() {
	static __fd_ops ops = [] {
		__fd_ops o;
		o.fo_ioctl = &__tau_syncfile_ioctl;
		o.fo_dup = &__tau_syncfile_dup;
		o.fo_close = &__tau_syncfile_close;
		o.fo_stat = &__tau_syncfile_stat;
		return o;
	}();
	return &ops;
}

static __fd_slot *__tau_gpufd_slot(int fd, const __fd_ops *ops) {
	auto slot = __libc::get()->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (slot->ops != ops) {
		__sprt_errno = EINVAL;
		return nullptr;
	}
	return slot;
}

// What poll() sees of a descriptor of this file; -1 when it is not one.
static int __tau_gpufd_ready(const __fd_slot *slot, short events) {
	if (slot->ops == __tau_dmabuf_ops()) {
		return events & (__SPRT_POLLIN | __SPRT_POLLOUT);
	}
	if (slot->ops != __tau_syncfile_ops()) {
		return -1;
	}
	auto f = __tau_syncfile_of(slot);
	if (!f->count || __atomic_load_n(&f->signaled, __ATOMIC_ACQUIRE)) {
		return __SPRT_POLLIN;
	}
	if (__sprt_tau_gpu_session() != f->session) {
		return __SPRT_POLLIN | __SPRT_POLLERR;
	}
	int st = __sprt_tau_gpu_wait(f->points, f->count, 0, 0);
	if (st == -__SPRT_TAU_ETIMEDOUT) {
		return 0;
	}
	if (st == 0) {
		__atomic_store_n(&f->signaled, 1, __ATOMIC_RELEASE);
		return __SPRT_POLLIN;
	}
	return __SPRT_POLLIN | __SPRT_POLLERR;
}

} // namespace sprt

using namespace sprt;

__SPRT_C_FUNC int __sprt_dmabuf_fd(struct __sprt_tau_gpu_buf *buf, int flags) {
	if (!buf || (flags & ~__SPRT_O_CLOEXEC)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	auto d = static_cast<__tau_dmabuf *>(__sprt_malloc(sizeof(__tau_dmabuf)));
	if (!d) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	*d = __tau_dmabuf{1, buf};
	int fd = __libc::get()->create_fd(d, __tau_dmabuf_ops(), __SPRT_O_RDWR | uint32_t(flags), 0);
	if (fd < 0) {
		__sprt_free(d);
	}
	return fd;
}

__SPRT_C_FUNC struct __sprt_tau_gpu_buf *__sprt_dmabuf_buf(int fd) {
	auto slot = __tau_gpufd_slot(fd, __tau_dmabuf_ops());
	return slot ? __tau_dmabuf_of(slot)->buf : nullptr;
}

__SPRT_C_FUNC int __sprt_syncfile_fd(const struct __sprt_tau_gpu_point *points, unsigned count) {
	if ((count && !points) || count > TAU_SYNCFILE_MAX) {
		__sprt_errno = EINVAL;
		return -1;
	}
	auto f = static_cast<__tau_syncfile *>(
			__sprt_malloc(sizeof(__tau_syncfile) + count * sizeof(__sprt_tau_gpu_point)));
	if (!f) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	f->refs = 1;
	f->count = count;
	f->signaled = 0;
	f->session = count ? __sprt_tau_gpu_session() : 0;
	if (count) {
		__builtin_memcpy(f->points, points, count * sizeof(__sprt_tau_gpu_point));
	}
	int fd = __libc::get()->create_fd(f, __tau_syncfile_ops(), __SPRT_O_RDONLY, 0);
	if (fd < 0) {
		__sprt_free(f);
	}
	return fd;
}

__SPRT_C_FUNC int __sprt_syncfile_points(int fd, struct __sprt_tau_gpu_point *out, unsigned max) {
	auto slot = __tau_gpufd_slot(fd, __tau_syncfile_ops());
	if (!slot) {
		return -1;
	}
	auto f = __tau_syncfile_of(slot);
	if (out) {
		__builtin_memcpy(out, f->points,
				(f->count < max ? f->count : max) * sizeof(__sprt_tau_gpu_point));
	}
	return int(f->count);
}
