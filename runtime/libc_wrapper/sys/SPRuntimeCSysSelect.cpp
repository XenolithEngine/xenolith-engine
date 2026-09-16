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

#include <sprt/c/sys/__sprt_select.h>
#include <sprt/c/sys/__sprt_poll.h>
#include <sprt/c/__sprt_errno.h>

#include <sprt/runtime/log.h>

#if SPRT_WINDOWS
// select() on Windows is winsock's (ws2_32); its fd_set matches the SPRT winsock fd_set.
#include <sprt/wrappers/windows/winsock.h>
#else
#include <sys/select.h>
#endif

#if SPRT_WINDOWS
#include <stdlib.h>
#else
// sigset_t for the pselect() forwarder below - needed on every non-Windows target,
// including wasm where __SPRT_CONFIG_HAVE_POLL is 0 (so it must sit outside that guard).
#include <signal.h>
#if __SPRT_CONFIG_HAVE_POLL
#include <poll.h>
#endif
#endif

// ---------------------------------------------------------------------------
// ABI validation. The SPRT fd_set (<sprt/c/cross/.../fdset.h>) matches the native one
// per-platform, so the wrapper forwards by a plain cast. struct timeval / timespec are
// translated field-wise into the native (or winsock 32-bit-long) layout, since SPRT's
// tv_sec/tv_usec are uniformly 64-bit.
// ---------------------------------------------------------------------------

#if __STDC_HOSTED__ == 1

static_assert(sizeof(struct __SPRT_ID(pollfd)) == sizeof(struct ::pollfd),
		"pollfd size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(pollfd), fd)
				== __builtin_offsetof(struct ::pollfd, fd),
		"pollfd.fd offset differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(pollfd), events)
				== __builtin_offsetof(struct ::pollfd, events),
		"pollfd.events offset differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(pollfd), revents)
				== __builtin_offsetof(struct ::pollfd, revents),
		"pollfd.revents offset differs from native");
static_assert(sizeof(__SPRT_ID(nfds_t)) == sizeof(::nfds_t), "nfds_t size differs from native");

static_assert(__SPRT_POLLIN == POLLIN && __SPRT_POLLPRI == POLLPRI && __SPRT_POLLOUT == POLLOUT
				&& __SPRT_POLLERR == POLLERR && __SPRT_POLLHUP == POLLHUP
				&& __SPRT_POLLNVAL == POLLNVAL && __SPRT_POLLRDNORM == POLLRDNORM
				&& __SPRT_POLLRDBAND == POLLRDBAND && __SPRT_POLLWRNORM == POLLWRNORM
				&& __SPRT_POLLWRBAND == POLLWRBAND,
		"POLL* flags differ from native");
#if defined(POLLMSG) && defined(__SPRT_POLLMSG)
static_assert(__SPRT_POLLMSG == POLLMSG, "POLLMSG differs from native");
#endif
#if defined(POLLRDHUP) && defined(__SPRT_POLLRDHUP)
static_assert(__SPRT_POLLRDHUP == POLLRDHUP, "POLLRDHUP differs from native");
#endif

static_assert(sizeof(__SPRT_ID(fd_set)) == sizeof(::fd_set), "fd_set size differs from native");
static_assert(__SPRT_FD_SETSIZE == FD_SETSIZE, "FD_SETSIZE differs from native");

#elif SPRT_WINDOWS

// The SPRT POLL* (cross windows values) must stay in lock-step with winsock's, since
// the wrapper passes events/revents through to WSAPoll without a flag remap.
static_assert(__SPRT_POLLRDNORM == POLLRDNORM && __SPRT_POLLRDBAND == POLLRDBAND
				&& __SPRT_POLLIN == POLLIN && __SPRT_POLLPRI == POLLPRI
				&& __SPRT_POLLWRNORM == POLLWRNORM && __SPRT_POLLOUT == POLLOUT
				&& __SPRT_POLLWRBAND == POLLWRBAND && __SPRT_POLLERR == POLLERR
				&& __SPRT_POLLHUP == POLLHUP && __SPRT_POLLNVAL == POLLNVAL,
		"SPRT POLL* differ from winsock POLL*");

#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(select)(int nfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT readfds,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT writeFds, __SPRT_ID(fd_set) * __SPRT_RESTRICT errorFds,
		const __SPRT_TIMEVAL_NAME *__SPRT_RESTRICT __timeout) {
#if SPRT_WINDOWS
	return ::select(nfds, (fd_set *)readfds, (fd_set *)writeFds, (fd_set *)errorFds, __timeout);
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
#if SPRT_WINDOWS || SPRT_EMBOX
	// winsock / Embox have no pselect(); emulate with select(). The signal mask
	// is not applied.
	struct timeval nativeTimeout;
	if (__timeout) {
		nativeTimeout.tv_sec = (long)__timeout->tv_sec;
		nativeTimeout.tv_usec = (long)(__timeout->tv_nsec / 1'000);
	}
	(void)sigmask;
	return ::select(nfds, (fd_set *)readfds, (fd_set *)writeFds, (fd_set *)errorFds,
			__timeout ? &nativeTimeout : nullptr);
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

__SPRT_C_FUNC int __SPRT_ID(
		poll)(struct __SPRT_ID(pollfd) * __fds, __SPRT_ID(nfds_t) __nfds, int __timeout) {
#if !__SPRT_CONFIG_HAVE_POLL
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_POLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS
	return ::WSAPoll(__fds, (ULONG)__nfds, __timeout);
#else
	return ::poll((struct ::pollfd *)__fds, (::nfds_t)__nfds, __timeout);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(ppoll)(struct __SPRT_ID(pollfd) * __fds, __SPRT_ID(nfds_t) __nfds,
		const struct __SPRT_TIMESPEC_NAME *__timeout, const __SPRT_ID(sigset_t) * __sigmask) {
#if !__SPRT_CONFIG_HAVE_POLL
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_POLL)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_WINDOWS || SPRT_APPLE || SPRT_EMBOX
	// No native ppoll (winsock; older macOS ship none): emulate with poll() and a
	// millisecond timeout. The signal mask is not applied - Windows has no POSIX
	// signals, and the atomic mask swap is unavailable here; use pselect if required.
	int __ms = -1;
	if (__timeout) {
		long long __m =
				(long long)__timeout->tv_sec * 1'000 + (long long)__timeout->tv_nsec / 1'000'000;
		if (__m < 0) {
			__m = 0;
		} else if (__m > __SPRT_INT_MAX) {
			__m = __SPRT_INT_MAX;
		}
		__ms = (int)__m;
	}
	(void)__sigmask;
	return __SPRT_ID(poll)(__fds, __nfds, __ms);
#else
	struct timespec __ts;
	if (__timeout) {
		__ts.tv_sec = __timeout->tv_sec;
		__ts.tv_nsec = __timeout->tv_nsec;
	}
	return ::ppoll((struct ::pollfd *)__fds, (::nfds_t)__nfds, __timeout ? &__ts : nullptr,
			(const ::sigset_t *)__sigmask);
#endif
}

} // namespace sprt
