/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include "sprt/c/__sprt_errno.h"
#define __SPRT_BUILD 1

#include <sprt/c/sys/__sprt_epoll.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/runtime/log.h>

#if __SPRT_CONFIG_HAVE_EPOLL
#include <sys/epoll.h>
#include <unistd.h>
#else
#include "private/SPRTSpecific.h"
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

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait2)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev,
		int maxevents, const struct __SPRT_TIMESPEC_NAME *tv, const __SPRT_ID(sigset_t) * sig) {
	// if timeout was not specified - it's safe to call epoll_pwait
	if (tv == nullptr) {
		return ::epoll_pwait(efd, (struct epoll_event *)ev, maxevents, 0, (const sigset_t *)sig);
	}

	auto ret = syscall(SYS_EPOLL_PWAIT2, efd, (struct epoll_event *)ev, maxevents, tv, sig,
			__SPRT__NSIG / 8);
	if (ret == -1 && *__sprt___errno_location() == ENOSYS) {
		// if there is no epoll_pwait2 - call epoll_pwait as fallback
		unsigned millis = tv->tv_sec * 1'000 + tv->tv_nsec / 1'000'000;
		if (millis == 0) {
			millis = 1; // at least 1 millisecond to wait
		}

		return ::epoll_pwait(efd, (struct epoll_event *)ev, maxevents, millis,
				(const sigset_t *)sig);
	}
	return ret;
}

#else

__SPRT_C_FUNC int __SPRT_ID(epoll_create)(int flags) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_create1)(int flags) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(
		epoll_ctl)(int efd, int op, int fd, struct __SPRT_EPOLL_EVENT_NAME *ev) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(
		epoll_wait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents, int timeout) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev, int maxevents,
		int timeout, const __SPRT_ID(sigset_t) * sig) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(epoll_pwait2)(int efd, struct __SPRT_EPOLL_EVENT_NAME *ev,
		int maxevents, const struct __SPRT_TIMESPEC_NAME *tv, const __SPRT_ID(sigset_t) * sig) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_EPOLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
