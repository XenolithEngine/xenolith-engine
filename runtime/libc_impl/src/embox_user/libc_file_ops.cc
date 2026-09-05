
// Embox EL0 file descriptor ops: the __fd_ops table, straight over syscalls.
//
// Unlike the wasm backend, which carries a whole memfs behind these entry
// points, there is a real kernel on the other side here. Every operation is one
// syscall and the interesting part is the bookkeeping around it.
//
// HANDLE ENCODING. __fd_slot::handle doubles as the "slot in use" flag -- every
// caller in libc_impl tests `!fdSlot->handle` before dispatching -- so a kernel
// fd cannot be stored as-is: fd 0 is perfectly valid and would read as a free
// slot. It is stored as (kfd + 1), and __el0_kfd() takes it back.
//
// The libc's fd numbers are its own (allocate_fd() hands out the lowest free
// bit) and are NOT the kernel's. They coincide for 0/1/2 because libc.cc wires
// them that way; past that they drift the moment the two allocators disagree,
// which is why nothing here passes a libc fd to a syscall.

#include "../../include/__impl_libc.h"
#include "kstat.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_mman.h>

#include "../../../core/include/__el0_syscall.h"

namespace sprt {

// (kfd + 1) so that kernel fd 0 does not encode as a null handle.
void *__el0_handle(int kfd) { return (void *)(__SPRT_ID(intptr_t))(kfd + 1); }

int __el0_kfd(const __fd_slot *fp) { return (int)(__SPRT_ID(intptr_t))fp->handle - 1; }

static ssize_t __file_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	(void)flags;
	if (offset) {
		// pread64(67) is not in the table (M2). Emulating it with
		// lseek/read/lseek would be a different call: it moves the file
		// position, which is the one thing pread promises not to do.
		__sprt_errno = ENOSYS;
		return -1;
	}
	return (ssize_t)__el0_ret(__el0_read(__el0_kfd(fp), buf, nbytes));
}

static ssize_t __file_write(__fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	(void)flags;
	if (offset) {
		__sprt_errno = ENOSYS; // pwrite64(68), same reason as pread above
		return -1;
	}
	return (ssize_t)__el0_ret(__el0_write(__el0_kfd(fp), buf, nbytes));
}

// The wire iovec is two u64s (ABI doc section 4.3). sprt's struct iovec is
// {void *, size_t} on LP64, which is the same 16 bytes in the same order -- but
// "is the same today" is not a contract, so it is copied through an explicit
// wire struct rather than reinterpret_cast'd.
struct __el0_iovec {
	__SPRT_ID(uint64_t) iov_base;
	__SPRT_ID(uint64_t) iov_len;
};

static_assert(sizeof(__el0_iovec) == 16, "struct iovec is 16 bytes on the wire");

// IOV_MAX, the largest iovcnt POSIX requires an implementation to accept.
static constexpr int EL0_IOV_MAX = 1'024;

// How many entries are converted per syscall. The wire array lives on the stack,
// and a full IOV_MAX of them would be 16 KiB of frame -- more than some threads
// have. Chunking is observably equivalent: the kernel walks the iovecs
// sequentially anyway (xl_sys_iov), and POSIX does not promise atomicity for
// readv/writev on anything but a pipe, which this cannot be.
static constexpr int EL0_IOV_CHUNK = 16;

static ssize_t __file_iov(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt, bool write) {
	if (iovcnt < 0 || iovcnt > EL0_IOV_MAX) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (iovcnt == 0) {
		return 0;
	}

	auto kfd = __el0_kfd(fp);
	ssize_t total = 0;
	for (int done = 0; done < iovcnt;) {
		int n = iovcnt - done;
		if (n > EL0_IOV_CHUNK) {
			n = EL0_IOV_CHUNK;
		}
		__el0_iovec wire[EL0_IOV_CHUNK];
		__SPRT_ID(uint64_t) want = 0;
		for (int i = 0; i < n; ++i) {
			wire[i].iov_base = (__SPRT_ID(uint64_t))(__SPRT_ID(uintptr_t))iov[done + i].iov_base;
			wire[i].iov_len = (__SPRT_ID(uint64_t))iov[done + i].iov_len;
			want += wire[i].iov_len;
		}
		auto got = __el0_ret(write ? __el0_writev(kfd, wire, n) : __el0_readv(kfd, wire, n));
		if (got < 0) {
			// A failure after some bytes moved is still a success of that many:
			// reporting -1 would lose them, and the caller cannot recover what
			// it was not told about.
			return total > 0 ? total : -1;
		}
		total += (ssize_t)got;
		if ((__SPRT_ID(uint64_t))got < want) {
			break; // short transfer: end of file, or a full pipe
		}
		done += n;
	}
	return total;
}

static ssize_t __file_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	return __file_iov(fp, iov, iovcnt, false);
}

static ssize_t __file_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	return __file_iov(fp, iov, iovcnt, true);
}

static off_t __file_seek(__fd_slot *fp, off_t offset, int whence) {
	return (off_t)__el0_ret(__el0_lseek(__el0_kfd(fp), (long)offset, whence));
}

static int __file_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	__el0_kstat ks;
	if (__el0_ret(__el0_fstat(__el0_kfd(fp), &ks)) < 0) {
		return -1;
	}
	__el0_kstat_to_stat(ks, st);
	return 0;
}

static int __file_close(__fd_slot *fp) {
	auto ret = (int)__el0_ret(__el0_close(__el0_kfd(fp)));
	// The slot is released whatever the kernel said. A close that reports an
	// error has still consumed the descriptor -- retrying it would eventually
	// close somebody else's file.
	fp->handle = nullptr;
	return ret;
}

static int __file_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	(void)fd;
	if (mode == __fd_ctl_mode::fnctl) {
		// fcntl(25) is M2. The two commands worth answering locally are
		// answered locally: the flags live in the slot, not in the kernel.
		switch (cmd) {
		case __SPRT_F_GETFL: return (int)fp->flags;
		case __SPRT_F_GETFD: return 0; // no FD_CLOEXEC: there is no exec (D5)
		case __SPRT_F_SETFD: return 0;
		default: __sprt_errno = ENOSYS; return -1;
		}
	}
	return (int)__el0_ret(__el0_ioctl(__el0_kfd(fp), (unsigned long)cmd, (void *)arg));
}

static int __file_dup(__fd_slot *fp, int *target, uint32_t flags) {
	(void)fp;
	(void)target;
	(void)flags;
	__sprt_errno = ENOSYS; // dup(23)/dup3(24) are M2
	return -1;
}

static int __file_chmod(__fd_slot *fp, mode_t mode) {
	(void)fp;
	(void)mode;
	__sprt_errno = ENOSYS; // fchmod(52) is not in the table
	return -1;
}

static int __file_utimens(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) {
	(void)fp;
	(void)times;
	__sprt_errno = ENOSYS; // utimensat(88) is M2
	return -1;
}

// mmap of a descriptor is the framebuffer question, and the kernel answers
// ENOSYS for it today (K5 is done for anonymous memory only). Wired anyway so
// that the day xl_mm_mmap accepts an fd, this file needs no change.
static void *__file_mmap(__fd_slot *fp, void *addr, size_t length, int prot, int flags,
		off_t offset) {
	auto ret = __el0_mmap(addr, length, prot, flags, __el0_kfd(fp), (long)offset);
	if (__el0_is_err(ret)) {
		__sprt_errno = (int)-ret;
		return __SPRT_MAP_FAILED;
	}
	return (void *)ret;
}

static int __file_munmap(__fd_slot *fp, void *addr, size_t length) {
	(void)fp;
	return (int)__el0_ret(__el0_munmap(addr, length));
}

static int __file_msync(__fd_slot *fp, void *addr, size_t length, int flags) {
	(void)fp;
	(void)addr;
	(void)length;
	(void)flags;
	// Every EL0 mapping is shared memory with the kernel, never a page cache
	// copy, so there is nothing to write back -- but msync(227) is also not in
	// the table, and reporting success for a call the kernel never saw would be
	// a promise about durability that nothing here can keep.
	__sprt_errno = ENOSYS;
	return -1;
}

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

// --- anonymous mappings -----------------------------------------------------
//
// What mimalloc runs on. Straight through to mmap(222); the kernel hands back
// zeroed pages (xl_mm.c allocates them and clears them), which is what the
// allocator assumes.

void *__file_mmap_anon(void *addr, size_t length, int prot, int flags, off_t offset) {
	if (length == 0) {
		__sprt_errno = EINVAL;
		return __SPRT_MAP_FAILED;
	}
	// fd -1 with MAP_ANONYMOUS: the kernel rejects any other combination.
	auto ret = __el0_mmap(addr, length, prot, flags | __SPRT_MAP_ANONYMOUS, -1, (long)offset);
	if (__el0_is_err(ret)) {
		__sprt_errno = (int)-ret;
		return __SPRT_MAP_FAILED;
	}
	return (void *)ret;
}

int __file_munmap_anon(void *addr, size_t length) {
	return (int)__el0_ret(__el0_munmap(addr, length));
}

} // namespace sprt
