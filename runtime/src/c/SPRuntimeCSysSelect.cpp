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

#include <c/sys/__sprt_select.h>
#include <c/__sprt_errno.h>

#include "SPRuntimeLog.h"

#include <sys/select.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(select)(int nfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT readfds,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT writeFds, __SPRT_ID(fd_set) * __SPRT_RESTRICT errorFds,
		__SPRT_TIMEVAL_NAME *__SPRT_RESTRICT __timeout) {
#if !__SPRT_CONFIG_HAVE_SELECT
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SELECT)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	struct timeval nativeTimeout;
	if (__timeout) {
		nativeTimeout.tv_sec = __timeout->tv_sec;
		nativeTimeout.tv_usec = __timeout->tv_usec;
	}

	return ::select(nfds, (fd_set *)readfds, (fd_set *)writeFds, (fd_set *)errorFds,
			__timeout ? &nativeTimeout : nullptr);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(pselect)(int nfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT readfds,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT writeFds, __SPRT_ID(fd_set) * __SPRT_RESTRICT errorFds,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT __timeout,
		const __SPRT_ID(sigset_t) * __SPRT_RESTRICT sigmask) {
#if !__SPRT_CONFIG_HAVE_SELECT
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SELECT)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	struct timespec nativeTimeout;
	if (__timeout) {
		nativeTimeout.tv_sec = __timeout->tv_sec;
		nativeTimeout.tv_nsec = __timeout->tv_nsec;
	}

	return ::pselect(nfds, (fd_set *)readfds, (fd_set *)writeFds, (fd_set *)errorFds,
			__timeout ? &nativeTimeout : nullptr, (sigset_t *)sigmask);
#endif
}


} // namespace sprt
