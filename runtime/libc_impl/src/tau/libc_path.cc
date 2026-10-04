// Paths on Tau: no file system service yet. Every call that would need one fails with
// ENOSYS; the working directory is "/".

#include "dirent.h"
#include "fcntl.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "sys/stat.h"
#include "../../include/__impl_libc.h"
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/cross/__sprt_setjmp.h>

#define __TAU_ENOSYS(Ret, Value) \
	{ \
		__sprt_errno = ENOSYS; \
		return (Ret)(Value); \
	}

__SPRT_C_FUNC int openat(int, const char *, int, ...) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int open(const char *, int, ...) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC FILE *fopen(const char *, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(FILE *, nullptr)
__SPRT_C_FUNC int stat(const char *__SPRT_RESTRICT, struct stat *__SPRT_RESTRICT) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int lstat(const char *__SPRT_RESTRICT, struct stat *__SPRT_RESTRICT) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int fstatat(int, const char *__SPRT_RESTRICT, struct stat *__SPRT_RESTRICT, int) __SPRT_NOEXCEPT
		__TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int faccessat(int, const char *, int, int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int utimensat(int, const char *, const struct __SPRT_TIMESPEC_NAME *, int) __SPRT_NOEXCEPT
		__TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int access(const char *, int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC char *realpath(const char *, char *) __SPRT_NOEXCEPT __TAU_ENOSYS(char *, nullptr)
__SPRT_C_FUNC int mkdirat(int, const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int mkdir(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int unlinkat(int, const char *, int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int unlink(const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int rmdir(const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int remove(const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int renameat(int, const char *, int, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int rename(const char *, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int ftruncate(int, __SPRT_ID(off_t)) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int truncate(const char *, __SPRT_ID(off_t)) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC __SPRT_ID(ssize_t) readlinkat(int, const char *, char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT
		__TAU_ENOSYS(__SPRT_ID(ssize_t), -1)
__SPRT_C_FUNC __SPRT_ID(ssize_t) readlink(const char *, char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT
		__TAU_ENOSYS(__SPRT_ID(ssize_t), -1)
__SPRT_C_FUNC int fsync(int) __SPRT_NOEXCEPT { return 0; }
__SPRT_C_FUNC int fdatasync(int) __SPRT_NOEXCEPT { return 0; }
__SPRT_C_FUNC int chdir(const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int fchdir(int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int link(const char *, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int linkat(int, const char *, int, const char *, int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int symlink(const char *, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int symlinkat(const char *, int, const char *) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int chmod(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC int fchmodat(int, const char *, __SPRT_ID(mode_t), int) __SPRT_NOEXCEPT __TAU_ENOSYS(int, -1)
__SPRT_C_FUNC __SPRT_ID(FILE) * tmpfile(void) __SPRT_NOEXCEPT __TAU_ENOSYS(__SPRT_ID(FILE) *, nullptr)
__SPRT_C_FUNC char *tmpnam(char *) __SPRT_NOEXCEPT __TAU_ENOSYS(char *, nullptr)

#undef __TAU_ENOSYS

__SPRT_C_FUNC char *getcwd(char *buf, __SPRT_ID(size_t) size) __SPRT_NOEXCEPT {
	if (!buf || size < 2) {
		__sprt_errno = buf ? ERANGE : EINVAL;
		return nullptr;
	}
	buf[0] = '/';
	buf[1] = 0;
	return buf;
}

// sigsetjmp is setjmp here (runtime_core_setjmp.cpp): no signal mask to restore.
extern "C" __SPRT_NORETURN void longjmp(__SPRT_ID(native_jmp_buf), int) __SPRT_NOEXCEPT;

extern "C" __SPRT_NORETURN void siglongjmp(__SPRT_ID(native_sigjmp_buf) buf, int ret) __SPRT_NOEXCEPT {
	longjmp(buf, ret);
}
