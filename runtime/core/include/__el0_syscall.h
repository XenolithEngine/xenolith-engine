/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef RUNTIME_CORE_INCLUDE___EL0_SYSCALL_H_
#define RUNTIME_CORE_INCLUDE___EL0_SYSCALL_H_

// Typed wrappers over the Embox EL0 syscall boundary.
//
// One wrapper per implemented syscall, with the real argument types. The raw
// __sprt_svcN() take six longs and would take them in any order; a typed wrapper
// is the only thing between "mmap(len, addr, ...)" and a build that succeeds.
//
// Every wrapper is named __el0_<syscall> and exists ONLY if the platform header
// declares that number. So the set here cannot drift ahead of what the kernel
// implements: adding one before its __SPRT_SYSCALL_<x> lands is a compile error
// in this file, and xenolith-os scripts/check-abi.py refuses a number the
// dispatcher does not handle.
//
// Two return conventions, and they are not interchangeable:
//
//   __el0_<x>()      the RAW kernel answer -- negative errno on failure. Use it
//                    when the caller wants to inspect the error without touching
//                    errno, or when the call cannot fail (brk).
//   __el0_ret(v)     the POSIX answer -- sets errno and returns -1 on failure.
//                    Everything in libc goes through this.
//
// The error test is `(unsigned long)v >= -4095UL`, not `v < 0`. mmap legitimately
// returns addresses with the top bits set, and a plain sign test would read one
// of them as a failure.

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_EMBOX_USER

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/cross/__sprt_syscall.h>
#include <sprt/c/cross/embox_user_sprt/aarch64_sprt/svc.h>

#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/__sprt_uint32_t.h>

__SPRT_BEGIN_DECL

// The kernel never returns a value in [-4095, -1] that is not an error: the ABI
// (section 1.2) reserves the whole range. 4095 is the largest errno the
// translation table can produce plus headroom, and matches what every Linux libc
// assumes -- keeping it identical is the point of decision D1.
#define __SPRT_EL0_ERRNO_MAX 4095UL

SPRT_FORCEINLINE int __el0_is_err(long __v) {
	return (unsigned long)__v >= (unsigned long)-(long)__SPRT_EL0_ERRNO_MAX;
}

// Turn a raw kernel answer into the POSIX one. Takes and returns long so that
// callers returning a pointer-sized value (mmap) or an off_t (lseek) do not lose
// bits on the way through.
SPRT_FORCEINLINE long __el0_ret(long __v) {
	if (__el0_is_err(__v)) {
		__sprt_errno = (int)-__v;
		return -1;
	}
	return __v;
}

// --- I/O --------------------------------------------------------------------

SPRT_FORCEINLINE long __el0_read(int __fd, void *__buf, __SPRT_ID(size_t) __n) {
	return __sprt_svc3(__SPRT_SYSCALL_read, __fd, (long)__buf, (long)__n);
}

SPRT_FORCEINLINE long __el0_write(int __fd, const void *__buf, __SPRT_ID(size_t) __n) {
	return __sprt_svc3(__SPRT_SYSCALL_write, __fd, (long)__buf, (long)__n);
}

// iovcnt is `int` on the wire even though the kernel widens it; passing a size_t
// here would silently truncate differently than the kernel expects.
SPRT_FORCEINLINE long __el0_readv(int __fd, const void *__iov, int __cnt) {
	return __sprt_svc3(__SPRT_SYSCALL_readv, __fd, (long)__iov, __cnt);
}

SPRT_FORCEINLINE long __el0_writev(int __fd, const void *__iov, int __cnt) {
	return __sprt_svc3(__SPRT_SYSCALL_writev, __fd, (long)__iov, __cnt);
}

// Since M2 a dirfd other than AT_FDCWD works: Embox has no dirfd-relative
// lookup, so the kernel remembers the path behind every directory descriptor it
// hands out and joins the relative name to it.
//
// O_DIRECTORY is served entirely inside the kernel and never reaches Embox,
// whose open() ASSERTS on the bit. What comes back is a descriptor out of a
// reserved high range rather than the ordinary small integer Linux gives -- it
// is fine to close, fstat, getdents64 and dup, and it is not fine to assume it
// fits in a short.
SPRT_FORCEINLINE long __el0_openat(int __dirfd, const char *__path, int __flags,
		unsigned int __mode) {
	return __sprt_svc4(__SPRT_SYSCALL_openat, __dirfd, (long)__path, __flags, (long)__mode);
}

SPRT_FORCEINLINE long __el0_close(int __fd) {
	return __sprt_svc1(__SPRT_SYSCALL_close, __fd);
}

SPRT_FORCEINLINE long __el0_lseek(int __fd, long __off, int __whence) {
	return __sprt_svc3(__SPRT_SYSCALL_lseek, __fd, __off, __whence);
}

// `arg` is whatever the command says it is; the kernel validates it against a
// table of known commands rather than guessing a size (ABI doc section 4.4).
SPRT_FORCEINLINE long __el0_ioctl(int __fd, unsigned long __cmd, void *__arg) {
	return __sprt_svc3(__SPRT_SYSCALL_ioctl, __fd, (long)__cmd, (long)__arg);
}

// --- stat -------------------------------------------------------------------
//
// Both fill a `struct xl_kstat` (128 bytes, ABI doc section 4.1), NOT the libc's
// struct stat. The backend converts.

SPRT_FORCEINLINE long __el0_fstat(int __fd, void *__kstat) {
	return __sprt_svc2(__SPRT_SYSCALL_fstat, __fd, (long)__kstat);
}

SPRT_FORCEINLINE long __el0_newfstatat(int __dirfd, const char *__path, void *__kstat,
		int __flags) {
	return __sprt_svc4(__SPRT_SYSCALL_newfstatat, __dirfd, (long)__path, (long)__kstat, __flags);
}

SPRT_FORCEINLINE long __el0_getdents64(int __fd, void *__buf, __SPRT_ID(size_t) __n) {
	return __sprt_svc3(__SPRT_SYSCALL_getdents64, __fd, (long)__buf, (long)__n);
}

// --- paths ------------------------------------------------------------------
//
// All five are built on the plain calls inside the kernel. Two answer
// differently than a Linux caller may expect and nothing here can detect it, so
// it is said instead: renameat is copy-then-delete and therefore NOT atomic,
// and readlinkat is always EINVAL on a path that exists, because no filesystem
// in this image has symbolic links.

SPRT_FORCEINLINE long __el0_mkdirat(int __dirfd, const char *__path, unsigned int __mode) {
	return __sprt_svc3(__SPRT_SYSCALL_mkdirat, __dirfd, (long)__path, (long)__mode);
}

SPRT_FORCEINLINE long __el0_unlinkat(int __dirfd, const char *__path, int __flags) {
	return __sprt_svc3(__SPRT_SYSCALL_unlinkat, __dirfd, (long)__path, __flags);
}

SPRT_FORCEINLINE long __el0_renameat(int __olddirfd, const char *__old, int __newdirfd,
		const char *__new) {
	return __sprt_svc4(__SPRT_SYSCALL_renameat, __olddirfd, (long)__old, __newdirfd, (long)__new);
}

// There is no user model on this system, so R_OK and W_OK are answered by the
// path existing -- which is what a caller running as root would get anyway --
// and only X_OK can fail.
SPRT_FORCEINLINE long __el0_faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	return __sprt_svc4(__SPRT_SYSCALL_faccessat, __dirfd, (long)__path, __mode, __flags);
}

SPRT_FORCEINLINE long __el0_readlinkat(int __dirfd, const char *__path, char *__buf,
		__SPRT_ID(size_t) __n) {
	return __sprt_svc4(__SPRT_SYSCALL_readlinkat, __dirfd, (long)__path, (long)__buf, (long)__n);
}

// The raw syscall answers with the length INCLUDING the terminator, which is
// not what the libc function of the same name returns.
SPRT_FORCEINLINE long __el0_getcwd(char *__buf, __SPRT_ID(size_t) __n) {
	return __sprt_svc2(__SPRT_SYSCALL_getcwd, (long)__buf, (long)__n);
}

SPRT_FORCEINLINE long __el0_chdir(const char *__path) {
	return __sprt_svc1(__SPRT_SYSCALL_chdir, (long)__path);
}

SPRT_FORCEINLINE long __el0_ftruncate(int __fd, long __len) {
	return __sprt_svc2(__SPRT_SYSCALL_ftruncate, __fd, __len);
}

// Succeeds without doing anything, and that is not a lie here: no filesystem in
// this image has a write-back cache, so a write that returned is already where
// fsync would put it. A descriptor that does not exist is still EBADF.
SPRT_FORCEINLINE long __el0_fsync(int __fd) {
	return __sprt_svc1(__SPRT_SYSCALL_fsync, __fd);
}

// --- descriptors ------------------------------------------------------------

SPRT_FORCEINLINE long __el0_dup(int __fd) { return __sprt_svc1(__SPRT_SYSCALL_dup, __fd); }

SPRT_FORCEINLINE long __el0_dup3(int __old, int __new, int __flags) {
	return __sprt_svc3(__SPRT_SYSCALL_dup3, __old, __new, __flags);
}

// `arg` is an integer for every command the kernel implements -- the record-lock
// commands, which would take a pointer, are refused with EINVAL rather than
// forwarded to something that answers about ioctls.
SPRT_FORCEINLINE long __el0_fcntl(int __fd, int __cmd, long __arg) {
	return __sprt_svc3(__SPRT_SYSCALL_fcntl, __fd, __cmd, __arg);
}

// --- pipes and poll ---------------------------------------------------------

SPRT_FORCEINLINE long __el0_pipe2(int *__fds, int __flags) {
	return __sprt_svc2(__SPRT_SYSCALL_pipe2, (long)__fds, __flags);
}

// sigmask MUST be null: there are no signals on this system, so the atomicity
// ppoll exists for cannot be provided, and the kernel refuses rather than
// pretending. A null timeout waits forever.
SPRT_FORCEINLINE long __el0_ppoll(void *__fds, unsigned long __nfds, const void *__timeout,
		const void *__sigmask) {
	return __sprt_svc5(__SPRT_SYSCALL_ppoll, (long)__fds, (long)__nfds, (long)__timeout,
			(long)__sigmask, 8L);
}

// --- randomness -------------------------------------------------------------
//
// Over /dev/urandom, which on this board is a linear congruential generator
// stirred with the clock. It is not a CSPRNG and there is no entropy pool
// behind it; GRND_RANDOM is refused rather than served by the source that is
// not it.
SPRT_FORCEINLINE long __el0_getrandom(void *__buf, __SPRT_ID(size_t) __n, unsigned int __flags) {
	return __sprt_svc3(__SPRT_SYSCALL_getrandom, (long)__buf, (long)__n, (long)__flags);
}

// --- memory -----------------------------------------------------------------

// brk cannot fail: it answers with the break it ended up with, and the caller
// compares against what it asked for (ABI doc section 1.2). Hence no __el0_ret.
SPRT_FORCEINLINE unsigned long __el0_brk(unsigned long __addr) {
	return (unsigned long)__sprt_svc1(__SPRT_SYSCALL_brk, (long)__addr);
}

// offset is in BYTES (the generic ABI's mmap, not the legacy page-granular
// mmap2). Only MAP_ANONYMOUS|MAP_PRIVATE is implemented today; fd >= 0 gets
// ENOSYS until the framebuffer mapping lands (K5).
SPRT_FORCEINLINE long __el0_mmap(void *__addr, __SPRT_ID(size_t) __len, int __prot, int __flags,
		int __fd, long __off) {
	return __sprt_svc6(__SPRT_SYSCALL_mmap, (long)__addr, (long)__len, __prot, __flags, __fd,
			__off);
}

SPRT_FORCEINLINE long __el0_munmap(void *__addr, __SPRT_ID(size_t) __len) {
	return __sprt_svc2(__SPRT_SYSCALL_munmap, (long)__addr, (long)__len);
}

SPRT_FORCEINLINE long __el0_mprotect(void *__addr, __SPRT_ID(size_t) __len, int __prot) {
	return __sprt_svc3(__SPRT_SYSCALL_mprotect, (long)__addr, (long)__len, __prot);
}

// --- identity and time ------------------------------------------------------

SPRT_FORCEINLINE long __el0_clock_gettime(int __clock, void *__timespec) {
	return __sprt_svc2(__SPRT_SYSCALL_clock_gettime, __clock, (long)__timespec);
}

// Fills a `struct xl_utsname` -- six fixed 65-byte fields, not Embox's six
// pointers (ABI doc section 4.2).
SPRT_FORCEINLINE long __el0_uname(void *__utsname) {
	return __sprt_svc1(__SPRT_SYSCALL_uname, (long)__utsname);
}

SPRT_FORCEINLINE long __el0_getpid(void) { return __sprt_svc0(__SPRT_SYSCALL_getpid); }

// --- futex ------------------------------------------------------------------
//
// Linux argument order: (uaddr, op, val, timeout, uaddr2, val3). The timeout is
// relative for FUTEX_WAIT and absolute CLOCK_MONOTONIC for FUTEX_WAIT_BITSET;
// uaddr2 is unused by the operations the kernel implements.
SPRT_FORCEINLINE long __el0_futex(__SPRT_ID(uint32_t) * __addr, int __op, __SPRT_ID(uint32_t) __val,
		const void *__timespec, __SPRT_ID(uint32_t) __val3) {
	return __sprt_svc6(__SPRT_SYSCALL_futex, (long)__addr, __op, (long)__val, (long)__timespec, 0L,
			(long)__val3);
}

SPRT_FORCEINLINE long __el0_gettid(void) { return __sprt_svc0(__SPRT_SYSCALL_gettid); }

// --- threads ----------------------------------------------------------------
//
// Linux argument order for aarch64: clone(flags, child_stack, parent_tid, tls,
// child_tid). The child returns 0 and resumes at the caller's return address on
// `child_stack`; the parent gets the child's tid. Everything else about the
// child's state is the caller's to arrange -- see the ABI doc, section 3.
SPRT_FORCEINLINE long __el0_clone(unsigned long __flags, void *__child_stack, int *__ptid,
		void *__tls, int *__ctid) {
	return __sprt_svc5(__SPRT_SYSCALL_clone, (long)__flags, (long)__child_stack, (long)__ptid,
			(long)__tls, (long)__ctid);
}

SPRT_FORCEINLINE long __el0_set_tid_address(int *__tid) {
	return __sprt_svc1(__SPRT_SYSCALL_set_tid_address, (long)__tid);
}

// --- time -------------------------------------------------------------------

SPRT_FORCEINLINE long __el0_nanosleep(const void *__req, void *__rem) {
	return __sprt_svc2(__SPRT_SYSCALL_nanosleep, (long)__req, (long)__rem);
}

SPRT_FORCEINLINE long __el0_clock_nanosleep(int __clock, int __flags, const void *__req,
		void *__rem) {
	return __sprt_svc4(__SPRT_SYSCALL_clock_nanosleep, __clock, __flags, (long)__req,
			(long)__rem);
}

SPRT_FORCEINLINE long __el0_sched_yield(void) {
	return __sprt_svc0(__SPRT_SYSCALL_sched_yield);
}

// --- exit -------------------------------------------------------------------
//
// Answered by the trap handler itself, before the dispatcher: they have to
// unwind the EL0 thread rather than return a value to it. Marked noreturn so the
// compiler stops emitting the unreachable tail.

__SPRT_NORETURN SPRT_FORCEINLINE void __el0_exit(int __status) {
	__sprt_svc1(__SPRT_SYSCALL_exit, __status);
	__builtin_unreachable();
}

__SPRT_NORETURN SPRT_FORCEINLINE void __el0_exit_group(int __status) {
	__sprt_svc1(__SPRT_SYSCALL_exit_group, __status);
	__builtin_unreachable();
}

__SPRT_END_DECL

#endif // SPRT_EMBOX_USER

#endif // RUNTIME_CORE_INCLUDE___EL0_SYSCALL_H_
