
// Embox EL0 unistd backend.
//
// read/write/close/dup and the rest of the descriptor family are generic
// (builtin_unistd.cpp over __fd_ops); this unit brings the libc internals into
// scope for that body and supplies what needs a syscall or a platform answer:
// identity, the program break, and the sysconf/pathconf constants.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

// Pulls in __libc, StringView, the fd dispatch tables, and (via sys/stat.h) the
// utimensat declaration builtin_unistd.cpp's utime() forwards to.
#include "../../include/__impl_libc.h"

#include "../../../core/include/__el0_syscall.h"

// --- identity ---------------------------------------------------------------
//
// getpid(172) and gettid(178) are real. The rest have no syscall AND no state
// behind them: an Embox task has no parent, no owner and no group. Fixed zeros
// are not placeholders here, they are the whole of the model -- see the note in
// libc.cc about why root is the honest uid.

extern "C" __SPRT_ID(pid_t) getpid(void) __SPRT_NOEXCEPT {
	return (__SPRT_ID(pid_t))__el0_getpid();
}

extern "C" __SPRT_ID(pid_t) getppid(void) __SPRT_NOEXCEPT { return 0; }

extern "C" __SPRT_ID(uid_t) getuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(uid_t) geteuid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getgid(void) __SPRT_NOEXCEPT { return 0; }
extern "C" __SPRT_ID(gid_t) getegid(void) __SPRT_NOEXCEPT { return 0; }

extern "C" int usleep(__SPRT_ID(time_t) useconds) __SPRT_NOEXCEPT {
	struct __SPRT_TIMESPEC_NAME ts;
	ts.tv_sec = (__SPRT_ID(time_t))(useconds / 1'000'000);
	ts.tv_nsec = (long)((useconds % 1'000'000) * 1'000);
	return __SPRT_ID(nanosleep)(&ts, nullptr);
}

// --- brk / sbrk -------------------------------------------------------------
//
// The kernel's brk answers with the break it ended up with and never reports an
// error (ABI doc section 1.2), so failure is detected by comparing: a request
// that could not be satisfied comes back as the OLD break, which is below what
// was asked for. That is glibc's test, and it works here for the same reason.
//
// One deliberate difference from glibc: the tracked break is what the CALLER
// asked for, not what the kernel returned. Linux's brk stores the byte-exact
// address and rounds only the mapping; this kernel returns the page-aligned end.
// Tracking the kernel's value would make every sbrk() of a few bytes consume a
// whole page, because the next request would start from the rounded address.

namespace {

using __el0_uptr = __UINTPTR_TYPE__;

// 0 means "not yet asked"; the first call learns the base from brk(0).
__el0_uptr s_el0_break = 0;

bool __el0_break_init(void) {
	if (s_el0_break == 0) {
		s_el0_break = (__el0_uptr)__el0_brk(0);
	}
	return s_el0_break != 0;
}

} // namespace

extern "C" int brk(void *addr) __SPRT_NOEXCEPT {
	if (!__el0_break_init()) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	auto want = (__el0_uptr)addr;
	auto got = (__el0_uptr)__el0_brk(want);
	if (got < want) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	s_el0_break = want;
	return 0;
}

extern "C" void *sbrk(__INTPTR_TYPE__ incr) __SPRT_NOEXCEPT {
	if (!__el0_break_init()) {
		__sprt_errno = ENOMEM;
		return (void *)(__INTPTR_TYPE__)-1;
	}
	auto old = s_el0_break;
	if (incr == 0) {
		return (void *)old;
	}
	auto want = old + (__el0_uptr)incr;
	// Overflow in either direction; also catches a negative incr larger than the
	// current break, which would wrap into a huge address.
	if ((incr > 0 && want < old) || (incr < 0 && want > old)) {
		__sprt_errno = ENOMEM;
		return (void *)(__INTPTR_TYPE__)-1;
	}
	if (brk((void *)want) != 0) {
		return (void *)(__INTPTR_TYPE__)-1;
	}
	return (void *)old;
}

// --- limits -----------------------------------------------------------------

extern "C" long sysconf(int name) __SPRT_NOEXCEPT {
	switch (name) {
	case __SPRT_SC_PAGESIZE: return 4'096; // MMU_PAGE_SIZE, 4 KiB granule (K1)
	case __SPRT_SC_NPROCESSORS_CONF:
	case __SPRT_SC_NPROCESSORS_ONLN:
		// Embox runs the aarch64/qemu and Pi 4 templates single-core. When SMP
		// arrives this has to come from the kernel, not from here.
		return 1;
	case __SPRT_SC_OPEN_MAX: return (long)sprt::MAX_FDS;
	default: return -1;
	}
}

namespace {

// One filesystem-independent answer: the values below hold for initfs, ramfs and
// FAT alike on this kernel, so there is nothing to look up per path.
long __el0_pathconf(int name) {
	switch (name) {
	case __SPRT_PC_LINK_MAX: return 1; // no link(2), and no hard links to make
	case __SPRT_PC_MAX_CANON: return 255;
	case __SPRT_PC_MAX_INPUT: return 255;
	case __SPRT_PC_NAME_MAX: return 255;
	case __SPRT_PC_PATH_MAX: return 4'096;
	case __SPRT_PC_PIPE_BUF: return 4'096;
	case __SPRT_PC_CHOWN_RESTRICTED: return 1;
	case __SPRT_PC_NO_TRUNC: return 1;
	case __SPRT_PC_VDISABLE: return 0;
	case __SPRT_PC_SYNC_IO: return 1;
	case __SPRT_PC_FILESIZEBITS: return 64;
	case __SPRT_PC_SYMLINK_MAX: return -1; // no symlinks
	case __SPRT_PC_2_SYMLINKS: return 0;
	default: return -1;
	}
}

} // namespace

extern "C" long pathconf(const char *, int name) __SPRT_NOEXCEPT { return __el0_pathconf(name); }

extern "C" long fpathconf(int fd, int name) __SPRT_NOEXCEPT {
	if (sprt::__libc::get()->get_fd_handle(fd) == nullptr) {
		__sprt_errno = EBADF;
		return -1;
	}
	return __el0_pathconf(name);
}

// --- entries with no syscall and no state behind them -------------------------

// D5: one statically linked ET_EXEC, no loader, no second process. execve is not
// "not yet" -- there is nothing for it to do. Defined rather than left out
// because libsprt references it from a path it never takes, and an undefined
// symbol there is a link error for every application.
extern "C" int execve(const char *, char *const[], char *const[]) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

// POSIX allows ctermid to answer with an empty string when it cannot determine
// the controlling terminal, and that is the truthful answer here: the console
// reached through fd 0/1/2 has no pathname an application could reopen -- the
// kernel handed those descriptors over, and there is no /dev entry behind them.
extern "C" char *ctermid(char *s) __SPRT_NOEXCEPT {
	static char empty[1] = {0};
	if (s) {
		s[0] = 0;
		return s;
	}
	return empty;
}
