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

#include <c/__sprt_errno.h>
#include <c/__sprt_unistd.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_string.h>
#include <c/__sprt_stdarg.h>

#include "SPRuntimePlatform.h"
#include "SPRuntimeLog.h"
#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"

#include <sys/resource.h>
#include <unistd.h>

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(access)(const char *path, int __type) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::access(target, __type);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(eaccess)(const char *path, int __type) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	return ::faccessat(-1, path, __type, __SPRT_AT_EACCESS);
#else
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::eaccess(target, __type);
	}, -1);
#endif
}

__SPRT_C_FUNC __SPRT_ID(off_t)
		__SPRT_ID(lseek)(int __fd, __SPRT_ID(off_t) __offset, int __whence) __SPRT_NOEXCEPT {
	return ::lseek64(__fd, __offset, __whence);
}

__SPRT_C_FUNC int __SPRT_ID(close)(int __fd) { return ::close(__fd); }

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(read)(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes) {
	return ::read(__fd, __buf, __nbytes);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(write)(int __fd, const void *__buf, __SPRT_ID(size_t) __n) {
	return ::write(__fd, __buf, __n);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pread)(int __fd, void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
	return ::pread64(__fd, __buf, __count, __offset);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		pwrite)(int __fd, const void *__buf, __SPRT_ID(size_t) __count, __SPRT_ID(off_t) __offset) {
	return ::pwrite64(__fd, __buf, __count, __offset);
}

__SPRT_C_FUNC unsigned int __SPRT_ID(sleep)(unsigned int __seconds) { return ::sleep(__seconds); }

__SPRT_C_FUNC int __SPRT_ID(usleep)(__SPRT_ID(time_t) __useconds) { return ::usleep(__useconds); }

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
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::chdir(target);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(fchdir)(int __fd) __SPRT_NOEXCEPT { return ::fchdir(__fd); }

__SPRT_C_FUNC char *__SPRT_ID(getcwd)(char *__buf, __SPRT_ID(size_t) __size) __SPRT_NOEXCEPT {
	auto ret = ::getcwd(__buf, __size);
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
		execve)(const char *__path, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::execve(target, __argv, __envp);
	}, -1);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fexecve)(int __fd, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_FEXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_FEXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::fexecve(__fd, __argv, __envp);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execv)(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::execv(target, __argv);
	}, -1);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(execvp)(const char *__file, char *const __argv[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::execvp(target, __argv);
	}, -1);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		execvpe)(const char *__file, char *const __argv[], char *const __envp[]) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_EXEC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_EXEC)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::execvpe(target, __argv, __envp);
	}, -1);
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

	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::execve(target, argv, envp);
	}, -1);
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

	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::execv(target, argv);
	}, -1);
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

	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::execvp(target, argv);
	}, -1);
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
		return (long int)::pathconf(target, __name);
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
	return ::fpathconf(__fd, __name);
#endif
}

__SPRT_C_FUNC long int __SPRT_ID(sysconf)(int __name) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_CONF
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_CONF)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::sysconf(__name);
#endif
}

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getpid)(void) __SPRT_NOEXCEPT { return ::getpid(); }

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(getppid)(void) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_GETPPID
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_GETPPID)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::getppid();
#endif
}

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(getuid)(void) __SPRT_NOEXCEPT { return ::getuid(); }

__SPRT_C_FUNC __SPRT_ID(uid_t) __SPRT_ID(geteuid)(void) __SPRT_NOEXCEPT { return ::geteuid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getgid)(void) __SPRT_NOEXCEPT { return ::getgid(); }

__SPRT_C_FUNC __SPRT_ID(gid_t) __SPRT_ID(getegid)(void) __SPRT_NOEXCEPT { return ::getegid(); }

__SPRT_C_FUNC int __SPRT_ID(getgroups)(int __size, __SPRT_ID(gid_t) __list[]) __SPRT_NOEXCEPT {
	return ::getgroups(__size, __list);
}

__SPRT_C_FUNC int __SPRT_ID(setuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
	return ::setuid(__uid);
}
__SPRT_C_FUNC int __SPRT_ID(
		setreuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid) __SPRT_NOEXCEPT {
	return ::setreuid(__ruid, __euid);
}
__SPRT_C_FUNC int __SPRT_ID(seteuid)(__SPRT_ID(uid_t) __uid) __SPRT_NOEXCEPT {
	return ::seteuid(__uid);
}
__SPRT_C_FUNC int __SPRT_ID(setgid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
	return ::setgid(__gid);
}
__SPRT_C_FUNC int __SPRT_ID(
		setregid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid) __SPRT_NOEXCEPT {
	return ::setregid(__rgid, __egid);
}
__SPRT_C_FUNC int __SPRT_ID(setegid)(__SPRT_ID(gid_t) __gid) __SPRT_NOEXCEPT {
	return ::setegid(__gid);
}
__SPRT_C_FUNC int __SPRT_ID(getresuid)(__SPRT_ID(uid_t) * __ruid, __SPRT_ID(uid_t) * __euid,
		__SPRT_ID(uid_t) * __suid) __SPRT_NOEXCEPT {
	return ::getresuid(__ruid, __euid, __suid);
}
__SPRT_C_FUNC int __SPRT_ID(getresgid)(__SPRT_ID(gid_t) * __rgid, __SPRT_ID(gid_t) * __egid,
		__SPRT_ID(gid_t) * __sgid) __SPRT_NOEXCEPT {
	return ::getresgid(__rgid, __egid, __sgid);
}
__SPRT_C_FUNC int __SPRT_ID(setresuid)(__SPRT_ID(uid_t) __ruid, __SPRT_ID(uid_t) __euid,
		__SPRT_ID(uid_t) __suid) __SPRT_NOEXCEPT {
	return ::setresuid(__ruid, __euid, __suid);
}
__SPRT_C_FUNC int __SPRT_ID(setresgid)(__SPRT_ID(gid_t) __rgid, __SPRT_ID(gid_t) __egid,
		__SPRT_ID(gid_t) __sgid) __SPRT_NOEXCEPT {
	return ::setresgid(__rgid, __egid, __sgid);
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(fork)(void) __SPRT_NOEXCEPT { return ::fork(); }
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(vfork)(void) __SPRT_NOEXCEPT { return ::vfork(); }
__SPRT_C_FUNC char *__SPRT_ID(ttyname)(int __fd) __SPRT_NOEXCEPT { return ::ttyname(__fd); }
__SPRT_C_FUNC int __SPRT_ID(
		ttyname_r)(int __fd, char *__buf, __SPRT_ID(size_t) __buflen) __SPRT_NOEXCEPT {
	return ::ttyname_r(__fd, __buf, __buflen);
}
__SPRT_C_FUNC int __SPRT_ID(isatty)(int __fd) __SPRT_NOEXCEPT { return ::isatty(__fd); }

__SPRT_C_FUNC int __SPRT_ID(link)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__from, [&](const char *f) {
		// call with native path
		return internal::performWithNativePath(__to, [&](const char *t) {
			// call with native path
			return ::link(f, t);
		}, -1);
	}, -1);
}
__SPRT_C_FUNC int __SPRT_ID(symlink)(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__from, [&](const char *f) {
		// call with native path
		return internal::performWithNativePath(__to, [&](const char *t) {
			// call with native path
			return ::symlink(f, t);
		}, -1);
	}, -1);
}
__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(readlink)(const char *__SPRT_RESTRICT __path,
		char *__SPRT_RESTRICT __buf, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		auto retlen = ::readlink(target, __buf, __len);
		if (retlen > 0) {
			if (!__sprt_fpath_is_posix(__buf, retlen)) {
				// convert path in place
				if (__sprt_fpath_to_posix(__buf, retlen, __buf, __len) == 0) {
					*__sprt___errno_location() = EINVAL;
					return (__SPRT_ID(ssize_t)) - 1;
				}
			}
		}
		return retlen;
	}, (__SPRT_ID(ssize_t)) - 1);
}
__SPRT_C_FUNC int __SPRT_ID(unlink)(const char *__name) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__name, [&](const char *target) {
		// call with native path
		return ::unlink(target);
	}, -1);
}
__SPRT_C_FUNC int __SPRT_ID(rmdir)(const char *__path) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::rmdir(target);
	}, -1);
}
__SPRT_C_FUNC char *__SPRT_ID(getlogin)(void) { return ::getlogin(); }
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
	return ::getlogin_r(__name, __name_len);
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
	return ::sethostname(__name, __len);
}
__SPRT_C_FUNC int __SPRT_ID(getdomainname)(char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::getdomainname(__name, __len);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(
		setdomainname)(const char *__name, __SPRT_ID(size_t) __len) __SPRT_NOEXCEPT {
#if !__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::setdomainname(__name, __len);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(fsync)(int __fd) { return ::fsync(__fd); }
__SPRT_C_FUNC void __SPRT_ID(sync)(void) __SPRT_NOEXCEPT { return ::sync(); }
__SPRT_C_FUNC int __SPRT_ID(getpagesize)(void) __SPRT_NOEXCEPT { return ::getpagesize(); }
__SPRT_C_FUNC int __SPRT_ID(getdtablesize)(void) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	struct rlimit rlim;

	if (::getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
		return rlim.rlim_cur;
	} else {
		return -1;
	}
#else
	return ::getdtablesize();
#endif
}
__SPRT_C_FUNC int __SPRT_ID(truncate)(const char *__file, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__file, [&](const char *target) {
		// call with native path
		return ::truncate64(target, length);
	}, -1);
}
__SPRT_C_FUNC int __SPRT_ID(ftruncate)(int __fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	return ::ftruncate64(__fd, length);
}
__SPRT_C_FUNC int __SPRT_ID(brk)(void *__addr) __SPRT_NOEXCEPT { return ::brk(__addr); }
__SPRT_C_FUNC void *__SPRT_ID(sbrk)(__SPRT_ID(intptr_t) __delta) __SPRT_NOEXCEPT {
	return ::sbrk(__delta);
}
__SPRT_C_FUNC int __SPRT_ID(lockf)(int __fd, int __cmd, __SPRT_ID(off_t) len) {
	return ::lockf64(__fd, __cmd, len);
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
	return ::copy_file_range(__infd, __pinoff, __outfd, __poutoff, __length, __flags);
#endif
}
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(gettid)(void) { return ::gettid(); }
__SPRT_C_FUNC int __SPRT_ID(fdatasync)(int __fildes) { return ::fdatasync(__fildes); }

__SPRT_C_FUNC void __SPRT_ID(swab)(const void *__SPRT_RESTRICT __from, void *__SPRT_RESTRICT __to,
		__SPRT_ID(ssize_t) __n) __SPRT_NOEXCEPT {
	return ::swab(__from, __to, __n);
}
__SPRT_C_FUNC int __SPRT_ID(getentropy)(void *__buffer, __SPRT_ID(size_t) __length) {
#if SPRT_ANDROID
	if (platform::makeRandomBytes((uint8_t *)__buffer, __length) == __length) {
		return 0;
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return ::getentropy(__buffer, __length);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		symlinkat)(const char *__old_path, int __new_dir_fd, const char *__new_path) {
	return internal::performWithNativePath(__old_path, [&](const char *old) {
		// call with native path
		return internal::performWithNativePath(__new_path, [&](const char *target) {
			// call with native path
			return ::symlinkat(old, __new_dir_fd, target);
		}, -1);
	}, -1);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		readlinkat)(int __dir_fd, const char *__path, char *__buf, __SPRT_ID(size_t) __buf_size) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		auto retlen = ::readlinkat(__dir_fd, target, __buf, __buf_size);
		if (retlen > 0) {
			if (!__sprt_fpath_is_posix(__buf, retlen)) {
				// convert path in place
				if (__sprt_fpath_to_posix(__buf, retlen, __buf, __buf_size) == 0) {
					*__sprt___errno_location() = EINVAL;
					return (__SPRT_ID(ssize_t)) - 1;
				}
			}
		}
		return retlen;
	}, (__SPRT_ID(ssize_t)) - 1);
}

__SPRT_C_FUNC int __SPRT_ID(fchownat)(int __dir_fd, const char *__path, __SPRT_ID(uid_t) __owner,
		__SPRT_ID(gid_t) __group, int __flags) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::fchownat(__dir_fd, target, __owner, __group, __flags);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(faccessat)(int __dirfd, const char *__path, int __mode, int __flags) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::faccessat(__dirfd, target, __mode, __flags);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(linkat)(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	return internal::performWithNativePath(__old_path, [&](const char *old) {
		// call with native path
		return internal::performWithNativePath(__new_path, [&](const char *target) {
			// call with native path
			return ::linkat(__old_dir_fd, old, __new_dir_fd, target, __flags);
		}, -1);
	}, -1);
}
__SPRT_C_FUNC int __SPRT_ID(unlinkat)(int __dirfd, const char *__path, int __flags) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		// call with native path
		return ::unlinkat(__dirfd, target, __flags);
	}, -1);
}

__SPRT_C_FUNC long __SPRT_ID(gethostid)(void) {
#warning TODO
	return 0;
}

} // namespace sprt

#if __clang__
#pragma clang diagnostic pop
#endif
