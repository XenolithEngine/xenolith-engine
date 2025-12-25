/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
#define CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_

#ifdef __SPRT_BUILD

#include_next <unistd.h>

#else

#include <c/__sprt_unistd.h>
#include <c/__sprt_stdarg.h>

#ifndef SEEK_SET
#define SEEK_SET __SPRT_SEEK_SET
#define SEEK_CUR __SPRT_SEEK_CUR
#define SEEK_END __SPRT_SEEK_END
#endif

#ifndef R_OK
#define R_OK __SPRT_R_OK
#define W_OK __SPRT_W_OK
#define X_OK __SPRT_X_OK
#define F_OK __SPRT_F_OK
#endif

#ifndef AT_FDCWD
#define AT_FDCWD __SPRT_AT_FDCWD
#define AT_SYMLINK_NOFOLLOW __SPRT_AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_FOLLOW __SPRT_AT_SYMLINK_FOLLOW
#define AT_NO_AUTOMOUNT __SPRT_AT_NO_AUTOMOUNT
#define AT_EMPTY_PATH __SPRT_AT_EMPTY_PATH
#define AT_STATX_SYNC_TYPE __SPRT_AT_STATX_SYNC_TYPE
#define AT_STATX_SYNC_AS_STAT __SPRT_AT_STATX_SYNC_AS_STAT
#define AT_STATX_FORCE_SYNC __SPRT_AT_STATX_FORCE_SYNC
#define AT_STATX_DONT_SYNC __SPRT_AT_STATX_DONT_SYNC
#define AT_RECURSIVE __SPRT_AT_RECURSIVE
#define AT_RENAME_NOREPLACE __SPRT_AT_RENAME_NOREPLACE
#define AT_RENAME_EXCHANGE __SPRT_AT_RENAME_EXCHANGE
#define AT_RENAME_WHITEOUT __SPRT_AT_RENAME_WHITEOUT
#define AT_EACCESS __SPRT_AT_EACCESS
#define AT_REMOVEDIR __SPRT_AT_REMOVEDIR
#define AT_HANDLE_FID __SPRT_AT_HANDLE_FID
#define AT_HANDLE_MNT_ID_UNIQUE __SPRT_AT_HANDLE_MNT_ID_UNIQUE
#endif

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(gid_t) gid_t;
typedef __SPRT_ID(pid_t) pid_t;
typedef __SPRT_ID(intptr_t) intptr_t;

SPRT_FORCEINLINE inline int access(const char *path, int __type) __SPRT_NOEXCEPT {
	return __sprt_access(path, __type);
}

SPRT_FORCEINLINE inline int eaccess(const char *path, int __type) __SPRT_NOEXCEPT {
	return __sprt_eaccess(path, __type);
}

SPRT_FORCEINLINE inline off_t lseek(int __fd, off_t __offset, int __whence) __SPRT_NOEXCEPT {
	return __sprt_lseek(__fd, __offset, __whence);
}

SPRT_FORCEINLINE inline int close(int __fd) { return __sprt_close(__fd); }

SPRT_FORCEINLINE inline ssize_t read(int __fd, void *__buf, size_t __nbytes) {
	return __sprt_read(__fd, __buf, __nbytes);
}

SPRT_FORCEINLINE inline ssize_t write(int __fd, const void *__buf, size_t __n) {
	return __sprt_write(__fd, __buf, __n);
}

SPRT_FORCEINLINE inline ssize_t pread(int __fd, void *__buf, size_t __count, off_t __offset) {
	return __sprt_pread(__fd, __buf, __count, __offset);
}

SPRT_FORCEINLINE inline ssize_t pwrite(int __fd, const void *__buf, size_t __count,
		off_t __offset) {
	return __sprt_pwrite(__fd, __buf, __count, __offset);
}

SPRT_FORCEINLINE inline unsigned int sleep(unsigned int __seconds) {
	return __sprt_sleep(__seconds);
}

SPRT_FORCEINLINE inline int usleep(time_t __useconds) { return __sprt_usleep(__useconds); }

#if __SPRT_CONFIG_HAVE_UNISTD_CHOWN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int chown(const char *__file, uid_t __owner,
		gid_t __group) __SPRT_NOEXCEPT {
	return __sprt_chown(__file, __owner, __group);
}
#endif

SPRT_FORCEINLINE inline int fchown(int __fd, uid_t __owner, gid_t __group) __SPRT_NOEXCEPT {
	return __sprt_fchown(__fd, __owner, __group);
}

SPRT_FORCEINLINE inline int chdir(const char *path) __SPRT_NOEXCEPT { return __sprt_chdir(path); }

SPRT_FORCEINLINE inline int fchdir(int __fd) __SPRT_NOEXCEPT { return __sprt_fchdir(__fd); }

SPRT_FORCEINLINE inline char *getcwd(char *__buf, size_t __size) __SPRT_NOEXCEPT {
	return __sprt_getcwd(__buf, __size);
}

#if __SPRT_CONFIG_HAVE_UNISTD_DUP || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int dup(int __fd) __SPRT_NOEXCEPT { return __sprt_dup(__fd); }

SPRT_FORCEINLINE inline int dup2(int __fd, int __fd2) __SPRT_NOEXCEPT {
	return __sprt_dup2(__fd, __fd2);
}
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_DUP3 || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int dup3(int __fd, int __fd2, int __flags) __SPRT_NOEXCEPT {
	return __sprt_dup3(__fd, __fd2, __flags);
}
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_EXEC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_FORCEINLINE inline int execve(const char *__path, char *const __argv[],
		char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_execve(__path, __argv, __envp);
}

SPRT_FORCEINLINE inline int fexecve(int __fd, char *const __argv[],
		char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_fexecve(__fd, __argv, __envp);
}

SPRT_FORCEINLINE inline int execv(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT {
	return __sprt_execv(__path, __argv);
}

SPRT_FORCEINLINE inline int execvp(const char *__file, char *const __argv[]) __SPRT_NOEXCEPT {
	return __sprt_execvp(__file, __argv);
}

SPRT_FORCEINLINE inline int execvpe(const char *__file, char *const __argv[],
		char *const __envp[]) __SPRT_NOEXCEPT {
	return __sprt_execvpe(__file, __argv, __envp);
}


SPRT_FORCEINLINE inline int execle(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {

	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	char **envp;
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i <= argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	envp = __sprt_va_arg(ap, char **);
	__sprt_va_end(ap);
	return __sprt_execve(__path, argv, envp);
}

SPRT_FORCEINLINE inline int execl(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i < argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	argv[i] = nullptr;
	__sprt_va_end(ap);
	return __sprt_execv(__path, argv);
}

SPRT_FORCEINLINE inline int execlp(const char *__file, const char *__arg, ...) __SPRT_NOEXCEPT {
	int argc;
	__sprt_va_list ap;
	__sprt_va_start(ap, __arg);
	for (argc = 1; __sprt_va_arg(ap, const char *); argc++);
	__sprt_va_end(ap);

	int i;
	char *argv[argc + 1];
	__sprt_va_start(ap, __arg);
	argv[0] = (char *)__arg;
	for (i = 1; i < argc; i++) { argv[i] = __sprt_va_arg(ap, char *); }
	argv[i] = nullptr;
	__sprt_va_end(ap);
	return __sprt_execvp(__file, argv);
}

#endif

#if __SPRT_CONFIG_HAVE_UNISTD_NICE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int nice(int __inc) __SPRT_NOEXCEPT { return __sprt_nice(__inc); }
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_CONF || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline long int pathconf(const char *__path, int __name) __SPRT_NOEXCEPT {
	return __sprt_pathconf(__path, __name);
}

SPRT_FORCEINLINE inline long int fpathconf(int __fd, int __name) __SPRT_NOEXCEPT {
	return __sprt_fpathconf(__fd, __name);
}

SPRT_FORCEINLINE inline long int sysconf(int __name) __SPRT_NOEXCEPT {
	return __sprt_sysconf(__name);
}
#endif

SPRT_FORCEINLINE inline pid_t getpid(void) __SPRT_NOEXCEPT { return __sprt_getpid(); }

#if __SPRT_CONFIG_HAVE_UNISTD_GETPPID || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline pid_t getppid(void) __SPRT_NOEXCEPT { return __sprt_getppid(); }
#endif

SPRT_FORCEINLINE inline uid_t getuid(void) __SPRT_NOEXCEPT { return __sprt_getuid(); }

SPRT_FORCEINLINE inline uid_t geteuid(void) __SPRT_NOEXCEPT { return __sprt_geteuid(); }

SPRT_FORCEINLINE inline gid_t getgid(void) __SPRT_NOEXCEPT { return __sprt_getgid(); }

SPRT_FORCEINLINE inline gid_t getegid(void) __SPRT_NOEXCEPT { return __sprt_getegid(); }

SPRT_FORCEINLINE inline int getgroups(int __size, gid_t __list[]) __SPRT_NOEXCEPT {
	return __sprt_getgroups(__size, __list);
}

SPRT_FORCEINLINE inline int setuid(uid_t __uid) __SPRT_NOEXCEPT { return __sprt_setuid(__uid); }
SPRT_FORCEINLINE inline int setreuid(uid_t __ruid, uid_t __euid) __SPRT_NOEXCEPT {
	return __sprt_setreuid(__ruid, __euid);
}
SPRT_FORCEINLINE inline int seteuid(uid_t __uid) __SPRT_NOEXCEPT { return __sprt_seteuid(__uid); }
SPRT_FORCEINLINE inline int setgid(gid_t __gid) __SPRT_NOEXCEPT { return __sprt_setgid(__gid); }
SPRT_FORCEINLINE inline int setregid(gid_t __rgid, gid_t __egid) __SPRT_NOEXCEPT {
	return __sprt_setregid(__rgid, __egid);
}
SPRT_FORCEINLINE inline int setegid(gid_t __gid) __SPRT_NOEXCEPT { return __sprt_setegid(__gid); }
SPRT_FORCEINLINE inline int getresuid(uid_t *__ruid, uid_t *__euid, uid_t *__suid) __SPRT_NOEXCEPT {
	return __sprt_getresuid(__ruid, __euid, __suid);
}
SPRT_FORCEINLINE inline int getresgid(gid_t *__rgid, gid_t *__egid, gid_t *__sgid) __SPRT_NOEXCEPT {
	return __sprt_getresgid(__rgid, __egid, __sgid);
}
SPRT_FORCEINLINE inline int setresuid(uid_t __ruid, uid_t __euid, uid_t __suid) __SPRT_NOEXCEPT {
	return __sprt_setresuid(__ruid, __euid, __suid);
}
SPRT_FORCEINLINE inline int setresgid(gid_t __rgid, gid_t __egid, gid_t __sgid) __SPRT_NOEXCEPT {
	return __sprt_setresgid(__rgid, __egid, __sgid);
}
SPRT_FORCEINLINE inline pid_t fork(void) __SPRT_NOEXCEPT { return __sprt_fork(); }
SPRT_FORCEINLINE inline pid_t vfork(void) __SPRT_NOEXCEPT { return __sprt_vfork(); }
SPRT_FORCEINLINE inline char *ttyname(int __fd) __SPRT_NOEXCEPT { return __sprt_ttyname(__fd); }
SPRT_FORCEINLINE inline int ttyname_r(int __fd, char *__buf, size_t __buflen) __SPRT_NOEXCEPT {
	return __sprt_ttyname_r(__fd, __buf, __buflen);
}
SPRT_FORCEINLINE inline int isatty(int __fd) __SPRT_NOEXCEPT { return __sprt_isatty(__fd); }

SPRT_FORCEINLINE inline int link(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return __sprt_link(__from, __to);
}
SPRT_FORCEINLINE inline int symlink(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return __sprt_symlink(__from, __to);
}
SPRT_FORCEINLINE inline ssize_t readlink(const char *__SPRT_RESTRICT __path,
		char *__SPRT_RESTRICT __buf, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_readlink(__path, __buf, __len);
}
SPRT_FORCEINLINE inline int unlink(const char *__name) __SPRT_NOEXCEPT {
	return __sprt_unlink(__name);
}
SPRT_FORCEINLINE inline int rmdir(const char *__path) __SPRT_NOEXCEPT {
	return __sprt_rmdir(__path);
}
SPRT_FORCEINLINE inline char *getlogin(void) { return __sprt_getlogin(); }
SPRT_FORCEINLINE inline int getlogin_r(char *__name, size_t __name_len) {
	return __sprt_getlogin_r(__name, __name_len);
}

SPRT_FORCEINLINE inline int setlogin(const char *__name) __SPRT_NOEXCEPT {
	return __sprt_setlogin(__name);
}

SPRT_FORCEINLINE inline int gethostname(char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_gethostname(__name, __len);
}
SPRT_FORCEINLINE inline int sethostname(const char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_sethostname(__name, __len);
}

#if __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline int getdomainname(char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_getdomainname(__name, __len);
}
SPRT_FORCEINLINE inline int setdomainname(const char *__name, size_t __len) __SPRT_NOEXCEPT {
	return __sprt_setdomainname(__name, __len);
}
#endif

SPRT_FORCEINLINE inline int fsync(int __fd) { return __sprt_fsync(__fd); }
SPRT_FORCEINLINE inline void sync(void) __SPRT_NOEXCEPT { return __sprt_sync(); }
SPRT_FORCEINLINE inline int getpagesize(void) __SPRT_NOEXCEPT { return __sprt_getpagesize(); }
SPRT_FORCEINLINE inline int getdtablesize(void) __SPRT_NOEXCEPT { return __sprt_getdtablesize(); }
SPRT_FORCEINLINE inline int truncate(const char *__file, off_t length) __SPRT_NOEXCEPT {
	return __sprt_truncate(__file, length);
}
SPRT_FORCEINLINE inline int ftruncate(int __fd, off_t length) __SPRT_NOEXCEPT {
	return __sprt_ftruncate(__fd, length);
}
SPRT_FORCEINLINE inline int brk(void *__addr) __SPRT_NOEXCEPT { return __sprt_brk(__addr); }
SPRT_FORCEINLINE inline void *sbrk(intptr_t __delta) __SPRT_NOEXCEPT {
	return __sprt_sbrk(__delta);
}
SPRT_FORCEINLINE inline int lockf(int __fd, int __cmd, off_t len) {
	return __sprt_lockf(__fd, __cmd, len);
}
SPRT_FORCEINLINE inline ssize_t copy_file_range(int __infd, off_t *__pinoff, int __outfd,
		off_t *__poutoff, size_t __length, unsigned int __flags) {
	return __sprt_copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
}
SPRT_FORCEINLINE inline pid_t gettid(void) { return __sprt_gettid(); }
SPRT_FORCEINLINE inline int fdatasync(int __fildes) { return __sprt_fdatasync(__fildes); }

SPRT_FORCEINLINE inline void swab(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		ssize_t __n) __SPRT_NOEXCEPT {
	return __sprt_swab(__from, __to, __n);
}
SPRT_FORCEINLINE inline int getentropy(void *__buffer, size_t __length) {
	return __sprt_getentropy(__buffer, __length);
}

SPRT_FORCEINLINE inline int symlinkat(const char *__old_path, int __new_dir_fd,
		const char *__new_path) {
	return __sprt_symlinkat(__old_path, __new_dir_fd, __new_path);
}

SPRT_FORCEINLINE inline ssize_t readlinkat(int __dir_fd, const char *__path, char *__buf,
		size_t __buf_size) {
	return __sprt_readlinkat(__dir_fd, __path, __buf, __buf_size);
}

SPRT_FORCEINLINE inline int fchownat(int __dir_fd, const char *__path, uid_t __owner, gid_t __group,
		int __flags) {
	return __sprt_fchownat(__dir_fd, __path, __owner, __group, __flags);
}

SPRT_FORCEINLINE inline int faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	return __sprt_faccessat(__dirfd, __path, __mode, __flags);
}

SPRT_FORCEINLINE inline int linkat(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	return __sprt_linkat(__old_dir_fd, __old_path, __new_dir_fd, __new_path, __flags);
}
SPRT_FORCEINLINE inline int unlinkat(int __dirfd, const char *__path, int __flags) {
	return __sprt_unlinkat(__dirfd, __path, __flags);
}

SPRT_FORCEINLINE inline long gethostid(void) { return __sprt_gethostid(); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
