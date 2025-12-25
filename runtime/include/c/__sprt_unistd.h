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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_UNISTD_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_UNISTD_H_

#include <c/cross/__sprt_config.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_ssize_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_intptr_t.h>
#include <c/bits/atfile.h>
#include <c/cross/__sprt_sysid.h>
#include <c/bits/seek.h>
#include <c/bits/access.h>

#define __SPRT_F_ULOCK 0	/* Unlock a previously locked region.  */
#define __SPRT_F_LOCK  1	/* Lock a region for exclusive use.  */
#define __SPRT_F_TLOCK 2	/* Test and lock a region for exclusive use.  */
#define __SPRT_F_TEST  3	/* Test a region for other processes locks.  */


#define __SPRT_SC_PAGE_SIZE	30
#define __SPRT_SC_PAGESIZE	30

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(access)(const char *__name, int __type) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(eaccess)(const char *__name, int __type) __SPRT_NOEXCEPT;

SPRT_API __SPRT_ID(off_t)
		__SPRT_ID(lseek)(int __fd, __SPRT_ID(off_t) __offset, int __whence) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(close)(int __fd);

SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(read)(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes);

SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(write)(int __fd, const void *__buf, __SPRT_ID(size_t) __n);

SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(
		pread)(int __fd, void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(
		pwrite)(int __fd, const void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset);

SPRT_API unsigned int __SPRT_ID(sleep)(unsigned int __seconds);

SPRT_API int __SPRT_ID(usleep)(__SPRT_ID(time_t) __useconds);

#if __SPRT_CONFIG_HAVE_UNISTD_CHOWN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(chown)(const char *__file, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(
		fchown)(int __fd, __SPRT_ID(uid_t) __owner, __SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT;
#endif

SPRT_API int __SPRT_ID(chdir)(const char *__path) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(fchdir)(int __fd) __SPRT_NOEXCEPT;

SPRT_API char *__SPRT_ID(getcwd)(char *__buf, __SPRT_ID(size_t) __size) __SPRT_NOEXCEPT;

#if __SPRT_CONFIG_HAVE_UNISTD_DUP || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(dup)(int __fd) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(dup2)(int __fd, int __fd2) __SPRT_NOEXCEPT;
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_DUP3 || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(dup3)(int __fd, int __fd2, int __flags) __SPRT_NOEXCEPT;
#endif


#if __SPRT_CONFIG_HAVE_UNISTD_EXEC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_API int __SPRT_ID(
		execve)(const char *__path, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(execv)(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(execvp)(const char *__file, char *const __argv[]) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(
		execvpe)(const char *__file, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(execle)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(execl)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(execlp)(const char *__file, const char *__arg, ...) __SPRT_NOEXCEPT;

#endif


#if __SPRT_CONFIG_HAVE_UNISTD_FEXEC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_API int __SPRT_ID(
		fexecve)(int __fd, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT;

#endif

#if __SPRT_CONFIG_HAVE_UNISTD_NICE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(nice)(int __inc) __SPRT_NOEXCEPT;
#endif

#if __SPRT_CONFIG_HAVE_UNISTD_CONF || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API long int __SPRT_ID(pathconf)(const char *__path, int __name) __SPRT_NOEXCEPT;

SPRT_API long int __SPRT_ID(fpathconf)(int __fd, int __name) __SPRT_NOEXCEPT;

SPRT_API long int __SPRT_ID(sysconf)(int __name) __SPRT_NOEXCEPT;
#endif

SPRT_API __SPRT_ID(pid_t) __SPRT_ID(getpid)(void) __SPRT_NOEXCEPT;

#if __SPRT_CONFIG_HAVE_UNISTD_GETPPID || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(getppid)(void) __SPRT_NOEXCEPT;
#endif

// On windows, we can use gettokeninformation for process, then get SID, then get RID from it
SPRT_API __SPRT_ID(uid_t) __SPRT_ID(getuid)(void) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(uid_t) __SPRT_ID(geteuid)(void) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(gid_t) __SPRT_ID(getgid)(void) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(gid_t) __SPRT_ID(getegid)(void) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(getgroups)(int __size, __SPRT_ID(gid_t) __list[]) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setreuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(seteuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setgid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setregid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setegid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(getresuid)(__SPRT_ID(uid_t) * __ruid, __SPRT_ID(uid_t) * __euid,
		__SPRT_ID(uid_t) * __suid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(getresgid)(__SPRT_ID(gid_t) * __rgid, __SPRT_ID(gid_t) * __egid,
		__SPRT_ID(gid_t) * __sgid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setresuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid,
		__SPRT_ID(uid_t) __suid) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setresgid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid,
		__SPRT_ID(gid_t) __sgid) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(fork)(void) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(vfork)(void) __SPRT_NOEXCEPT;
SPRT_API char *__SPRT_ID(ttyname)(int __fd) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(
		ttyname_r)(int __fd, char *__buf, __SPRT_ID(size_t) __buflen) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(isatty)(int __fd) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(link)(const char *__from, const char *__to) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(symlink)(const char *__from, const char *__to) __SPRT_NOEXCEPT;
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(readlink)(const char *__SPRT_RESTRICT __path,
		char *__SPRT_RESTRICT __buf, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(unlink)(const char *__name) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(rmdir)(const char *__path) __SPRT_NOEXCEPT;
SPRT_API char *__SPRT_ID(getlogin)(void);
SPRT_API int __SPRT_ID(getlogin_r)(char *__name, __SPRT_ID(size_t) __name_len);

#if __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(setlogin)(const char *__name) __SPRT_NOEXCEPT;
#endif

SPRT_API int __SPRT_ID(gethostname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(sethostname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT;

#if __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(getdomainname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(setdomainname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT;
#endif

SPRT_API int __SPRT_ID(fsync)(int __fd);
SPRT_API void __SPRT_ID(sync)(void) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(getpagesize)(void) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(getdtablesize)(void) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(truncate)(const char *__file, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(ftruncate)(int __fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(brk)(void *__addr) __SPRT_NOEXCEPT;
SPRT_API void *__SPRT_ID(sbrk)(__SPRT_ID(intptr_t) __delta) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(lockf)(int __fd, int __cmd, __SPRT_ID(off_t) len);
SPRT_API __SPRT_ID(ssize_t)
		__SPRT_ID(copy_file_range)(int __infd, __SPRT_ID(off_t) * __pinoff, int __outfd,
				__SPRT_ID(off_t) * __poutoff, __SPRT_ID(size_t) __length, unsigned int __flags);
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(gettid)(void);
SPRT_API int __SPRT_ID(fdatasync)(int __fildes);
SPRT_API void __SPRT_ID(swab)(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		__SPRT_ID(ssize_t) __n) __SPRT_NOEXCEPT;
SPRT_API int __SPRT_ID(getentropy)(void *__buffer, __SPRT_ID(size_t) __length);

SPRT_API int __SPRT_ID(symlinkat)(const char *__old_path, int __new_dir_fd, const char *__new_path);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(
		readlinkat)(int __dir_fd, const char *__path, char *__buf, __SPRT_ID(size_t) __buf_size);
SPRT_API int __SPRT_ID(fchownat)(int __dir_fd, const char *__path, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group, int __flags);
SPRT_API int __SPRT_ID(faccessat)(int __dirfd, const char *__path, int __mode, int __flags);
SPRT_API int __SPRT_ID(linkat)(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags);
SPRT_API int __SPRT_ID(unlinkat)(int __dirfd, const char *__path, int __flags);

SPRT_API long __SPRT_ID(gethostid)(void);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_UNISTD_H_
