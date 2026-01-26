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

#define __SPRT_BUILD 1

#include <sprt/c/sys/__sprt_eventfd.h>
#include <sprt/c/sys/__sprt_signalfd.h>
#include <sprt/c/sys/__sprt_timerfd.h>
#include <sprt/c/sys/__sprt_uring.h>
#include <sprt/c/__sprt_errno.h>

#if __SPRT_CONFIG_HAVE_EVENTFD

#include <sys/eventfd.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(eventfd)(unsigned int count, int flags) {
	return ::eventfd(count, flags);
}

__SPRT_C_FUNC int __SPRT_ID(eventfd_read)(int fd, __SPRT_ID(eventfd_t) * efd) {
	return ::eventfd_read(fd, efd);
}

__SPRT_C_FUNC int __SPRT_ID(eventfd_write)(int fd, __SPRT_ID(eventfd_t) efd) {
	return ::eventfd_write(fd, efd);
}

} // namespace sprt

#else

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(eventfd)(unsigned int count, int flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(eventfd_read)(int fd, __SPRT_ID(eventfd_t) * efd) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(eventfd_write)(int fd, __SPRT_ID(eventfd_t) efd) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

} // namespace sprt

#endif

#if __SPRT_CONFIG_HAVE_SIGNALFD

#include <sys/signalfd.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(signalfd)(int fd, const __SPRT_ID(sigset_t) * sig, int flags) {
	return ::signalfd(fd, (const sigset_t *)sig, flags);
}

} // namespace sprt

#else

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(signalfd)(int fd, const __SPRT_ID(sigset_t) * sig, int flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

} // namespace sprt

#endif


#if __SPRT_CONFIG_HAVE_TIMERFD

#include <sys/timerfd.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(timerfd_create)(int clockid, int flags) {
	return ::timerfd_create(clockid, flags);
}

__SPRT_C_FUNC int __SPRT_ID(timerfd_settime)(int ufd, int flags,
		const struct __SPRT_ITIMERSPEC_NAME *utmr, struct __SPRT_ITIMERSPEC_NAME *otmr) {
	struct itimerspec _utmr;
	if (utmr) {
		_utmr.it_interval.tv_nsec = utmr->it_interval.tv_nsec;
		_utmr.it_interval.tv_sec = utmr->it_interval.tv_sec;
		_utmr.it_value.tv_nsec = utmr->it_value.tv_nsec;
		_utmr.it_value.tv_sec = utmr->it_value.tv_sec;
	}

	struct itimerspec _otmr;

	auto ret = ::timerfd_settime(ufd, flags, utmr ? &_utmr : nullptr, otmr ? &_otmr : nullptr);

	if (otmr) {
		otmr->it_interval.tv_nsec = _otmr.it_interval.tv_nsec;
		otmr->it_interval.tv_sec = _otmr.it_interval.tv_sec;
		otmr->it_value.tv_nsec = _otmr.it_value.tv_nsec;
		otmr->it_value.tv_sec = _otmr.it_value.tv_sec;
	}

	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(timerfd_gettime)(int ufd, struct __SPRT_ITIMERSPEC_NAME *otmr) {
	struct itimerspec _otmr;
	auto ret = ::timerfd_gettime(ufd, otmr ? &_otmr : nullptr);
	if (otmr) {
		otmr->it_interval.tv_nsec = _otmr.it_interval.tv_nsec;
		otmr->it_interval.tv_sec = _otmr.it_interval.tv_sec;
		otmr->it_value.tv_nsec = _otmr.it_value.tv_nsec;
		otmr->it_value.tv_sec = _otmr.it_value.tv_sec;
	}
	return ret;
}

} // namespace sprt

#else

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(timerfd_create)(int clockid, int flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(timerfd_settime)(int ufd, int flags,
		const struct __SPRT_ITIMERSPEC_NAME *utmr, struct __SPRT_ITIMERSPEC_NAME *otmr) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(timerfd_gettime)(int ufd, struct __SPRT_ITIMERSPEC_NAME *otmr) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

} // namespace sprt

#endif
