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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_limits.h>
#include <sprt/c/sys/__sprt_epoll.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/cross/__sprt_syscall.h>
#include <sprt/runtime/log.h>

#if __SPRT_CONFIG_HAVE_EPOLL
#include <sys/epoll.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

namespace sprt {

#if __SPRT_CONFIG_HAVE_EPOLL

__SPRT_C_FUNC int __SPRT_ID(epoll_create)(int flags) { return ::epoll_create(flags); }

__SPRT_C_FUNC int __SPRT_ID(epoll_create1)(int flags) { return ::epoll_create1(flags); }

__SPRT_C_FUNC int __SPRT_ID(
		epoll_ctl)(int efd, int op, int fd, struct __SPRT_EPOLL_EVENT_NAME *ev) {
	return ::epoll_ctl(efd, op, fd, (struct epoll_event *)ev);
}

__SPRT_C_FUNC int __SPRT_ID(
		epoll_wait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents, int timeout) {
	return ::epoll_wait(efd, (struct epoll_event *)ev, maxevents, timeout);
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents,
		int timeout, const __SPRT_ID(sigset_t) * sig) {
	return ::epoll_pwait(efd, (struct epoll_event *)ev, maxevents, timeout, (const sigset_t *)sig);
}

// epoll_pwait2(2) exists from Linux 5.10 only. On older Android kernels
// (API 26 images run 5.4) the seccomp policy KILLS the process for the
// unknown syscall number - bionic's SIGSYS handler aborts before errno can
// say ENOSYS - so the syscall must never be issued there at all. AND the
// seccomp app allowlist is per-ANDROID-RELEASE, not per-kernel: the API 33
// image runs kernel 5.15 (so uname says "fine") yet its app seccomp policy
// still disallows syscall 441 - apps may issue it only from API 34+. Both
// gates are probed once per process (uname and __system_property_get are
// seccomp-safe).
extern "C" int __system_property_get(const char *, char *);

static bool s_haveEpollPwait2() {
	static const bool have = [] {
#if defined(__ANDROID__)
		char sdk[8] = {0};
		if (__system_property_get("ro.build.version.sdk", sdk) <= 0 || ::atoi(sdk) < 34) {
			return false;
		}
		struct utsname u;
		if (::uname(&u) != 0 || u.release[0] == '\0') {
			return false;
		}
		int a = 0, b = 0;
		if (sscanf(u.release, "%d.%d", &a, &b) != 2) {
			return false;
		}
		return a > 5 || (a == 5 && b >= 10);
#elif defined(__linux__)
		struct utsname u;
		if (::uname(&u) != 0 || u.release[0] == '\0') {
			return false;
		}
		int a = 0, b = 0;
		if (sscanf(u.release, "%d.%d", &a, &b) != 2) {
			return false;
		}
		return a > 5 || (a == 5 && b >= 10);
#else
		return false;
#endif
	}();
	return have;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait2)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev,
		int maxevents, const struct __SPRT_TIMESPEC_NAME *tv, const __SPRT_ID(sigset_t) * sig) {
	// Pre-5.10 kernels: never issue the syscall (seccomp kills, see above);
	// a null timeout also does not need it.
	if (tv == nullptr || !s_haveEpollPwait2()) {
		long long millis64 = 0;
		if (tv != nullptr) {
			millis64 = (long long)tv->tv_sec * 1'000 + (long long)tv->tv_nsec / 1'000'000;
		}
		if (millis64 < 1 && tv != nullptr) {
			millis64 = 1; // at least 1 millisecond to wait
		} else if (millis64 > __SPRT_INT_MAX) {
			millis64 = __SPRT_INT_MAX; // clamp to epoll_pwait's int timeout
		}

		return ::epoll_pwait(efd, (struct epoll_event *)ev, maxevents, (int)millis64,
				(const sigset_t *)sig);
	}

	auto ret = syscall(__SPRT_SYSCALL_epoll_pwait2, efd, (struct epoll_event *)ev, maxevents, tv,
			sig, __SPRT__NSIG / 8);
	if (ret == -1 && *__sprt___errno_location() == ENOSYS) {
		// if there is no epoll_pwait2 - call epoll_pwait as fallback
		long long millis64 =
				(long long)tv->tv_sec * 1'000 + (long long)tv->tv_nsec / 1'000'000;
		if (millis64 < 1) {
			millis64 = 1; // at least 1 millisecond to wait
		} else if (millis64 > __SPRT_INT_MAX) {
			millis64 = __SPRT_INT_MAX; // clamp to epoll_pwait's int timeout
		}

		return ::epoll_pwait(efd, (struct epoll_event *)ev, maxevents, (int)millis64,
				(const sigset_t *)sig);
	}
	return ret;
}

#else

__SPRT_C_FUNC int __SPRT_ID(epoll_create)(int flags) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_create1)(int flags) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(
		epoll_ctl)(int efd, int op, int fd, struct __SPRT_EPOLL_EVENT_NAME *ev) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(
		epoll_wait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents, int timeout) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents,
		int timeout, const __SPRT_ID(sigset_t) * sig) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait2)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev,
		int maxevents, const struct __SPRT_TIMESPEC_NAME *tv, const __SPRT_ID(sigset_t) * sig) {
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
