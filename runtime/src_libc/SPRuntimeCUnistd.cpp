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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/hash.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/log.h>

#include <limits.h>

#if SPRT_WINDOWS
#include <Windows.h>
#include <corecrt_io.h>
#include <direct.h>
#include <process.h>
#include <stdio.h>

#include "platform/windows/unistd.cc"
#else
#include <unistd.h>
#include <sys/resource.h>
#endif

#include <stdlib.h>

#include "private/SPRTFilename.h"
#include "private/SPRTSpecific.h"

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(access)(const char *path, int __type) __SPRT_NOEXCEPT {
	return access(path, __type);
}

__SPRT_C_FUNC int __SPRT_ID(eaccess)(const char *path, int __type) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	return ::faccessat(-1, path, __type, __SPRT_AT_EACCESS);
#else
	return eaccess(path, __type);
#endif
}

__SPRT_C_FUNC __SPRT_ID(off_t)
		__SPRT_ID(lseek)(int __fd, __SPRT_ID(off_t) __offset, int __whence) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::_lseeki64(__fd, __offset, __whence);
#else
	return ::lseek64(__fd, __offset, __whence);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(close)(int __fd) {
#if SPRT_WINDOWS
	return ::_close(__fd);
#else
	return ::close(__fd);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(read)(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes) {
#if SPRT_WINDOWS
	return ::_read(__fd, __buf, __nbytes);
#else
	return ::read(__fd, __buf, __nbytes);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(write)(int __fd, const void *__buf, __SPRT_ID(size_t) __n) {
#if SPRT_WINDOWS
	return ::_write(__fd, __buf, __n);
#else
	return ::write(__fd, __buf, __n);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pread)(int __fd, void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
	return pread64(__fd, __buf, __count, __offset);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pwrite)(int __fd, const void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
	return pwrite64(__fd, __buf, __count, __offset);
}


__SPRT_C_FUNC unsigned int __SPRT_ID(sleep)(unsigned int __seconds) {
#if SPRT_WINDOWS
	::Sleep(__seconds * 1'000);
	return 0;
#else
	return ::sleep(__seconds);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(usleep)(__SPRT_ID(time_t) __useconds) { return usleep(__useconds); }

__SPRT_C_FUNC int __SPRT_ID(chown)(const char *__file, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CHOWN
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::chown(target, __owner, __group);
	}, -1);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fchown)(int __fd, __SPRT_ID(uid_t) __owner, __SPRT_ID(gid_t) __group) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CHOWN
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::fchown(__fd, __owner, __group);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(chdir)(const char *path) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		::_chdir(target);
		return 0;
	}, -1);
#else
	return ::chdir(path);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fchdir)(int __fd) __SPRT_NOEXCEPT { return fchdir(__fd); }

__SPRT_C_FUNC char *__SPRT_ID(getcwd)(char *__buf, __SPRT_ID(size_t) __size) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	auto ret = ::_getcwd(__buf, __size);
#else
	auto ret = ::getcwd(__buf, __size);
#endif
	if (ret) {
		auto retlen = __sprt_strlen(ret);
		if (!__sprt_fpath_is_posix(ret, retlen)) {
			// convert path in place
			if (__sprt_fpath_to_posix(ret, retlen, ret, retlen + 1) == 0) {
				*__sprt___errno_location() = EINVAL;
				return nullptr;
			}
		}
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(dup)(int __fd) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return ::_dup(__fd);
#else
	return ::dup(__fd);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(dup2)(int __fd, int __fd2) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return ::_dup2(__fd, __fd2);
#else
	return ::dup2(__fd, __fd2);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(dup3)(int __fd, int __fd2, int __flags) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DUP3
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DUP3)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::dup3(__fd, __fd2, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		execve)(const char *__path, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execve(target, _argv, __envp));
	}, -1);
#else
	return ::execve(__path, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fexecve)(int __fd, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_FEXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FEXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return fexecve(__fd, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execv)(const char *__path, char *const _argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execv(target, _argv));
	}, -1);
#else
	return ::execv(__path, _argv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execvp)(const char *__file, char *const _argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvp(target, _argv));
	}, -1);
#else
	return ::execvp(__file, _argv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		execvpe)(const char *__file, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvpe(target, _argv, __envp));
	}, -1);
#else
	return ::execvpe(__file, _argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execle)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
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

#if SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execve(target, argv, envp));
	}, -1);
#else
	return ::execve(__path, argv, envp);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execl)(const char *__path, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
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
	argv[i] = NULL;
	__sprt_va_end(ap);

#if SPRT_WINDOWS
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return int(::_execv(target, argv));
	}, -1);
#else
	return ::execv(__path, argv);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execlp)(const char *__file, const char *__arg, ...) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else

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
	argv[i] = NULL;
	__sprt_va_end(ap);

#if SPRT_WINDOWS
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return int(::_execvp(target, argv));
	}, -1);
#else
	return ::execvp(__file, argv);
#endif
#endif
}

__SPRT_C_FUNC int __SPRT_ID(nice)(int __inc) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_NICE
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_NICE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::nice(__inc);
#endif
}

__SPRT_C_FUNC long int __SPRT_ID(pathconf)(const char *__path, int __name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CONF
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CONF)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return (long int)pathconf(target, __name);
	}, (long int)-1);
#endif
}

__SPRT_C_FUNC long int __SPRT_ID(fpathconf)(int __fd, int __name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CONF
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CONF)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return fpathconf(__fd, __name);
#endif
}

__SPRT_C_FUNC long int __SPRT_ID(sysconf)(int __name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CONF
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CONF)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return sysconf(__name);
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getpid)(void) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return GetCurrentProcessId();
#else
	return ::getpid();
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getppid)(void) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_GETPPID
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_GETPPID)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getppid();
#endif
}

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(getuid)(void) __SPRT_NOEXCEPT { return getuid(); }

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(geteuid)(void) __SPRT_NOEXCEPT { return geteuid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getgid)(void) __SPRT_NOEXCEPT { return getgid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getegid)(void) __SPRT_NOEXCEPT { return getegid(); }

__SPRT_C_FUNC int __SPRT_ID(getgroups)(int __size, __SPRT_ID(gid_t) __list[]) __SPRT_NOEXCEPT {
	return getgroups(__size, __list);
}

__SPRT_C_FUNC int __SPRT_ID(setuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setuid(__uid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setreuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setreuid(__ruid, __euid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(seteuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::seteuid(__uid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setgid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setgid(__gid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setregid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setregid(__rgid, __egid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setegid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setegid(__gid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getresuid)(__SPRT_ID(uid_t) * __ruid, __SPRT_ID(uid_t) * __euid,
		__SPRT_ID(uid_t) * __suid) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	auto id = getuid();
	if (__ruid) {
		*__ruid = id;
	}
	if (__euid) {
		*__euid = id;
	}
	if (__suid) {
		*__suid = id;
	}
	return 0;
#else
	return ::getresuid(__ruid, __euid, __suid);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(getresgid)(__SPRT_ID(gid_t) * __rgid, __SPRT_ID(gid_t) * __egid,
		__SPRT_ID(gid_t) * __sgid) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	auto id = getgid();
	if (__rgid) {
		*__rgid = id;
	}
	if (__egid) {
		*__egid = id;
	}
	if (__sgid) {
		*__sgid = id;
	}
	return 0;
#else
	return ::getresgid(__rgid, __egid, __sgid);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setresuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid,
		__SPRT_ID(uid_t) __suid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setresuid(__ruid, __euid, __suid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(setresgid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid,
		__SPRT_ID(gid_t) __sgid) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETUIDGID
	return ::setresgid(__rgid, __egid, __sgid);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETUIDGID)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(fork)(void) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_FORK
	return ::fork();
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FORK)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(vfork)(void) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_FORK
	return ::vfork();
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FORK)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ttyname)(int __fd) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return nullptr;
#else
	return ::ttyname(__fd);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		ttyname_r)(int __fd, char *__buf, __SPRT_ID(size_t) __buflen) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	__sprt_errno = ENOSYS;
	return -1;
#else
	return ::ttyname_r(__fd, __buf, __buflen);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(isatty)(int __fd) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return ::_isatty(__fd);
#else
	return ::isatty(__fd);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(link)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return link(__from, __to);
}

__SPRT_C_FUNC int __SPRT_ID(symlink)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return symlink(__from, __to);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(readlink)(const char *__SPRT_RESTRICT __path,
		char *__SPRT_RESTRICT __buf, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
	return readlink(__path, __buf, __len);
}
__SPRT_C_FUNC int __SPRT_ID(unlink)(const char *__name) __SPRT_NOEXCEPT { return unlink(__name); }

__SPRT_C_FUNC int __SPRT_ID(rmdir)(const char *__path) __SPRT_NOEXCEPT { return rmdir(__path); }

__SPRT_C_FUNC char *__SPRT_ID(getlogin)(void) { return getlogin(); }

__SPRT_C_FUNC int __SPRT_ID(getlogin_r)(char *__name, __SPRT_ID(size_t) __name_len) {
#if SPRT_ANDROID
	if (platform::_getlogin_r) {
		return platform::_getlogin_r(__name, __name_len);
	}
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getlogin_r(__name, __name_len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(setlogin)(const char *__name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETLOGIN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::setlogin(__name);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(gethostname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
	return ::gethostname(__name, __len);
}
__SPRT_C_FUNC int __SPRT_ID(
		sethostname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME
	return ::sethostname(__name, __len);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_SETHOSTNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}
__SPRT_C_FUNC int __SPRT_ID(getdomainname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return getdomainname(__name, __len);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setdomainname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	errno = EPERM; // Operation not permitted
	return -1;
#else
	return ::setdomainname(__name, __len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fsync)(int __fd) { return fsync(__fd); }

__SPRT_C_FUNC void __SPRT_ID(sync)(void) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	_flushall();
#else
	return ::sync();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getpagesize)(void) __SPRT_NOEXCEPT { return getpagesize(); }
__SPRT_C_FUNC int __SPRT_ID(getdtablesize)(void) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	struct rlimit rlim;

	if (::getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
		return rlim.rlim_cur;
	} else {
		return -1;
	}
#else
	return getdtablesize();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(truncate)(const char *__file, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	return truncate64(__file, length);
}

__SPRT_C_FUNC int __SPRT_ID(ftruncate)(int __fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	return ftruncate64(__fd, length);
}

__SPRT_C_FUNC int __SPRT_ID(brk)(void *__addr) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_BRK
	return ::brk(__addr);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_BRK)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC void *__SPRT_ID(sbrk)(__SPRT_ID(intptr_t) __delta) __SPRT_NOEXCEPT {
#if __SPRT_CONFIG_HAVE_UNISTD_BRK
	return ::sbrk(__delta);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_BRK)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(lockf)(int __fd, int __cmd, __SPRT_ID(off_t) len) {
	return lockf64(__fd, __cmd, len);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(copy_file_range)(int __infd, __SPRT_ID(off_t) * __pinoff, int __outfd,
				__SPRT_ID(off_t) * __poutoff, __SPRT_ID(size_t) __length, unsigned int __flags) {
#if SPRT_ANDROID
	if (platform::_copy_file_range) {
		return platform::_copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
	}
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(gettid)(void) {
#if SPRT_WINDOWS
	return GetCurrentThreadId();
#else
	return ::gettid();
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fdatasync)(int __fildes) {
#if SPRT_WINDOWS
	return fsync(__fildes);
#else
	return ::fdatasync(__fildes);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(swab)(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		__SPRT_ID(ssize_t) __n) __SPRT_NOEXCEPT {
#if SPRT_WINDOWS
	return _swab((char *)__from, (char *)__to, __n);
#else
	return ::swab(__from, __to, __n);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getentropy)(void *__buffer, __SPRT_ID(size_t) __length) {
#if SPRT_ANDROID
	if (platform::_getentropy) {
		return platform::_getentropy(__buffer, __length);
	} else {
		arc4random_buf(__buffer, __length);
		return 0;
	}
#else
	return getentropy(__buffer, __length);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		symlinkat)(const char *__old_path, int __new_dir_fd, const char *__new_path) {
	return symlinkat(__old_path, __new_dir_fd, __new_path);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		readlinkat)(int __dir_fd, const char *__path, char *__buf, __SPRT_ID(size_t) __buf_size) {
	return readlinkat(__dir_fd, __path, __buf, __buf_size);
}

__SPRT_C_FUNC int __SPRT_ID(fchownat)(int __dir_fd, const char *__path, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group, int __flags) {
#if __SPRT_CONFIG_HAVE_UNISTD_CHOWN
	return ::fchownat(__dir_fd, __path, __owner, __group, __flags);
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CHOWN)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(faccessat)(int __dirfd, const char *__path, int __mode, int __flags) {
	return faccessat(__dirfd, __path, __mode, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(linkat)(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	return linkat(__old_dir_fd, __old_path, __new_dir_fd, __new_path, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(unlinkat)(int __dirfd, const char *__path, int __flags) {
	return unlinkat(__dirfd, __path, __flags);
}

__SPRT_C_FUNC long __SPRT_ID(gethostid)(void) {
	auto dev = platform::getUniqueDeviceId();

	if constexpr (sizeof(long) == 4) {
		return hash32(dev.data(), dev.size());
	} else {
		return hash64(dev.data(), dev.size());
	}
}

__SPRT_C_FUNC int __SPRT_ID(pipe)(int fds[2]) { return pipe(fds); }
__SPRT_C_FUNC int __SPRT_ID(pipe2)(int fds[2], int flags) { return pipe2(fds, flags); }

} // namespace sprt

#if __clang__
#pragma clang diagnostic pop
#endif
