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
#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/runtime/log.h>

#if SPRT_WASM

// The browser sandbox has no socket layer; every entry point is an ENOSYS stub. See
// libc_impl/src/wasm/socket.cc for the public symbols; here the __sprt_* backing mirrors
// it so freestanding code that references the socket wrappers still links.

#elif SPRT_WINDOWS

// Windows sockets are winsock's (ws2_32). The wrapper translates the POSIX int fd to a
// 64-bit SOCKET, socklen_t (int on Windows) unchanged, and void* buffers to char*.
#include <sprt/wrappers/windows/winsock.h>
#include <stdlib.h>

// A POSIX int fd re-widened to a winsock SOCKET (handles fit in 32 bits per MS).
#define __SPRT_WFD(fd) ((SOCKET)(unsigned)(fd))

#else

// Hosted (Linux / macOS / Android): forward straight to the platform libc, casting the
// SPRT structs to the native ones (validated identical by the static_asserts below).
#include <sys/socket.h>
#include <fcntl.h>
#include <time.h>

#if SPRT_APPLE
// macOS ships no accept4() / SOCK_CLOEXEC / SOCK_NONBLOCK; the emulation maps these flag
// bits (Linux values; the path is unused on hosted macOS, where accept4 is not public).
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 02000000
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 04000
#endif
#endif

#endif

// ---------------------------------------------------------------------------
// ABI validation (hosted). The per-platform cross <sys/socket.h> surface
// (cross/<platform>/socket.h + sockdef.h) is defined to match the native header
// value-for-value and layout-for-layout, so the forwarders below are plain casts.
// ---------------------------------------------------------------------------

#if !SPRT_WASM && !SPRT_WINDOWS

static_assert(sizeof(struct __SPRT_ID(sockaddr)) == sizeof(struct ::sockaddr),
		"sockaddr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr), sa_family)
				== __builtin_offsetof(struct ::sockaddr, sa_family),
		"sockaddr.sa_family offset differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(sockaddr), sa_data)
				== __builtin_offsetof(struct ::sockaddr, sa_data),
		"sockaddr.sa_data offset differs from native");
static_assert(sizeof(__SPRT_ID(socklen_t)) == sizeof(::socklen_t),
		"socklen_t size differs from native");

static_assert(sizeof(struct __SPRT_ID(msghdr)) == sizeof(struct ::msghdr),
		"msghdr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(msghdr), msg_name)
						== __builtin_offsetof(struct ::msghdr, msg_name)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_namelen)
						== __builtin_offsetof(struct ::msghdr, msg_namelen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_iov)
						== __builtin_offsetof(struct ::msghdr, msg_iov)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_iovlen)
						== __builtin_offsetof(struct ::msghdr, msg_iovlen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_control)
						== __builtin_offsetof(struct ::msghdr, msg_control)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_controllen)
						== __builtin_offsetof(struct ::msghdr, msg_controllen)
				&& __builtin_offsetof(struct __SPRT_ID(msghdr), msg_flags)
						== __builtin_offsetof(struct ::msghdr, msg_flags),
		"msghdr layout differs from native");

static_assert(sizeof(struct __SPRT_ID(cmsghdr)) == sizeof(struct ::cmsghdr),
		"cmsghdr size differs from native");
static_assert(__builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_len)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_len)
				&& __builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_level)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_level)
				&& __builtin_offsetof(struct __SPRT_ID(cmsghdr), cmsg_type)
						== __builtin_offsetof(struct ::cmsghdr, cmsg_type),
		"cmsghdr layout differs from native");

// The __SPRT_-prefixed socket constants (cross/<platform>/socket.h) are validated here
// against the native <sys/socket.h>; the plain SOCK_*/AF_*/... alias the __SPRT_ ones.
static_assert(__SPRT_SHUT_RD == SHUT_RD && __SPRT_SHUT_WR == SHUT_WR
				&& __SPRT_SHUT_RDWR == SHUT_RDWR,
		"SHUT_* differ from native");
static_assert(__SPRT_SOCK_STREAM == SOCK_STREAM && __SPRT_SOCK_DGRAM == SOCK_DGRAM
				&& __SPRT_SOCK_RAW == SOCK_RAW && __SPRT_SOCK_SEQPACKET == SOCK_SEQPACKET,
		"SOCK_* type constants differ from native");
#ifdef SOCK_CLOEXEC
static_assert(__SPRT_SOCK_CLOEXEC == SOCK_CLOEXEC && __SPRT_SOCK_NONBLOCK == SOCK_NONBLOCK,
		"SOCK_CLOEXEC/NONBLOCK differ from native");
#endif
static_assert(__SPRT_AF_UNSPEC == AF_UNSPEC && __SPRT_AF_UNIX == AF_UNIX
				&& __SPRT_AF_INET == AF_INET && __SPRT_AF_INET6 == AF_INET6,
		"AF_* differ from native");
static_assert(__SPRT_SOL_SOCKET == SOL_SOCKET, "SOL_SOCKET differs from native");
static_assert(__SPRT_SO_REUSEADDR == SO_REUSEADDR && __SPRT_SO_TYPE == SO_TYPE
				&& __SPRT_SO_ERROR == SO_ERROR && __SPRT_SO_DONTROUTE == SO_DONTROUTE
				&& __SPRT_SO_BROADCAST == SO_BROADCAST && __SPRT_SO_SNDBUF == SO_SNDBUF
				&& __SPRT_SO_RCVBUF == SO_RCVBUF && __SPRT_SO_KEEPALIVE == SO_KEEPALIVE
				&& __SPRT_SO_OOBINLINE == SO_OOBINLINE && __SPRT_SO_LINGER == SO_LINGER,
		"SO_* differ from native");
#ifdef SO_REUSEPORT
static_assert(__SPRT_SO_REUSEPORT == SO_REUSEPORT, "SO_REUSEPORT differs from native");
#endif
static_assert(__SPRT_MSG_OOB == MSG_OOB && __SPRT_MSG_PEEK == MSG_PEEK
				&& __SPRT_MSG_DONTROUTE == MSG_DONTROUTE && __SPRT_MSG_CTRUNC == MSG_CTRUNC
				&& __SPRT_MSG_TRUNC == MSG_TRUNC && __SPRT_MSG_DONTWAIT == MSG_DONTWAIT
				&& __SPRT_MSG_EOR == MSG_EOR && __SPRT_MSG_WAITALL == MSG_WAITALL,
		"MSG_* differ from native");
#ifdef MSG_NOSIGNAL
static_assert(__SPRT_MSG_NOSIGNAL == MSG_NOSIGNAL, "MSG_NOSIGNAL differs from native");
#endif
// SOMAXCONN is a soft backlog cap, not an ABI value (glibc raised it 128 -> 4096), so it
// is defined for the API but deliberately not asserted against the native header.

#endif // hosted

namespace sprt {

#if SPRT_WASM
#define __SPRT_SOCK_ENOSYS() \
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__, \
			" not available on this platform"); \
	*__sprt___errno_location() = ENOSYS; \
	return -1
#endif

__SPRT_C_FUNC int __SPRT_ID(socket)(int __domain, int __type, int __protocol) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	SOCKET __s = ::socket(__domain, __type, __protocol);
	return (__s == INVALID_SOCKET) ? -1 : (int)__s;
#else
	return ::socket(__domain, __type, __protocol);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(socketpair)(int __domain, int __type, int __protocol, int __sv[2]) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	*__sprt___errno_location() = ENOSYS; // winsock has no socketpair()
	return -1;
#else
	return ::socketpair(__domain, __type, __protocol, __sv);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		bind)(int __fd, const struct __SPRT_ID(sockaddr) * __addr, __SPRT_ID(socklen_t) __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::bind(__SPRT_WFD(__fd), __addr, (int)__len);
#else
	return ::bind(__fd, (const ::sockaddr *)__addr, (::socklen_t)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		connect)(int __fd, const struct __SPRT_ID(sockaddr) * __addr, __SPRT_ID(socklen_t) __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::connect(__SPRT_WFD(__fd), __addr, (int)__len);
#else
	return ::connect(__fd, (const ::sockaddr *)__addr, (::socklen_t)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(listen)(int __fd, int __backlog) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::listen(__SPRT_WFD(__fd), __backlog);
#else
	return ::listen(__fd, __backlog);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(accept)(int __fd, struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	SOCKET __s = ::accept(__SPRT_WFD(__fd), __addr, (int *)__len);
	return (__s == INVALID_SOCKET) ? -1 : (int)__s;
#else
	return ::accept(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(accept4)(int __fd, struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	// winsock has no accept4(): accept() then map SOCK_NONBLOCK to FIONBIO (CLOEXEC is
	// a no-op on Windows).
	SOCKET __s = ::accept(__SPRT_WFD(__fd), __addr, (int *)__len);
	if (__s == INVALID_SOCKET) {
		return -1;
	}
	if (__flags & SOCK_NONBLOCK) {
		u_long __nb = 1;
		::ioctlsocket(__s, (long)FIONBIO, &__nb);
	}
	return (int)__s;
#elif SPRT_APPLE
	// macOS ships no accept4(): emulate with accept() + fcntl() for CLOEXEC/NONBLOCK.
	int __s = ::accept(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
	if (__s < 0) {
		return -1;
	}
	if (__flags & SOCK_CLOEXEC) {
		::fcntl(__s, F_SETFD, ::fcntl(__s, F_GETFD, 0) | FD_CLOEXEC);
	}
	if (__flags & SOCK_NONBLOCK) {
		::fcntl(__s, F_SETFL, ::fcntl(__s, F_GETFL, 0) | O_NONBLOCK);
	}
	return __s;
#else
	return ::accept4(__fd, (::sockaddr *)__addr, (::socklen_t *)__len, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getsockname)(int __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getsockname(__SPRT_WFD(__fd), __addr, (int *)__len);
#else
	return ::getsockname(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getpeername)(int __fd,
		struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getpeername(__SPRT_WFD(__fd), __addr, (int *)__len);
#else
	return ::getpeername(__fd, (::sockaddr *)__addr, (::socklen_t *)__len);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(shutdown)(int __fd, int __how) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::shutdown(__SPRT_WFD(__fd), __how);
#else
	return ::shutdown(__fd, __how);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(getsockopt)(int __fd, int __level, int __optname,
		void *__SPRT_RESTRICT __optval, __SPRT_ID(socklen_t) * __SPRT_RESTRICT __optlen) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::getsockopt(__SPRT_WFD(__fd), __level, __optname, (char *)__optval, (int *)__optlen);
#else
	return ::getsockopt(__fd, __level, __optname, __optval, (::socklen_t *)__optlen);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(setsockopt)(int __fd, int __level, int __optname, const void *__optval,
		__SPRT_ID(socklen_t) __optlen) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return ::setsockopt(__SPRT_WFD(__fd), __level, __optname, (const char *)__optval,
			(int)__optlen);
#else
	return ::setsockopt(__fd, __level, __optname, __optval, (::socklen_t)__optlen);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(send)(int __fd, const void *__buf, __SPRT_ID(size_t) __n, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return (__SPRT_ID(ssize_t))::send(__SPRT_WFD(__fd), (const char *)__buf, (int)__n, __flags);
#else
	return ::send(__fd, __buf, __n, __flags);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(recv)(int __fd, void *__buf, __SPRT_ID(size_t) __n, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return (__SPRT_ID(ssize_t))::recv(__SPRT_WFD(__fd), (char *)__buf, (int)__n, __flags);
#else
	return ::recv(__fd, __buf, __n, __flags);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(sendto)(int __fd, const void *__buf, __SPRT_ID(size_t) __n, int __flags,
				const struct __SPRT_ID(sockaddr) * __addr, __SPRT_ID(socklen_t) __addr_len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return (__SPRT_ID(ssize_t))::sendto(__SPRT_WFD(__fd), (const char *)__buf, (int)__n, __flags,
			__addr, (int)__addr_len);
#else
	return ::sendto(__fd, __buf, __n, __flags, (const ::sockaddr *)__addr, (::socklen_t)__addr_len);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(recvfrom)(int __fd, void *__SPRT_RESTRICT __buf,
		__SPRT_ID(size_t) __n, int __flags, struct __SPRT_ID(sockaddr) * __SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __addr_len) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	return (__SPRT_ID(ssize_t))::recvfrom(__SPRT_WFD(__fd), (char *)__buf, (int)__n, __flags,
			__addr, (int *)__addr_len);
#else
	return ::recvfrom(__fd, __buf, __n, __flags, (::sockaddr *)__addr, (::socklen_t *)__addr_len);
#endif
}

#if SPRT_WINDOWS
// Shared iovec -> WSABUF gather for the msg wrappers. Returns the WSABUF array (either
// the caller's stack buffer or a malloc'd one; *__owned is set when it must be freed).
static WSABUF *__sprt_win_gather(const struct __SPRT_ID(iovec) * __iov, unsigned int __n,
		WSABUF *__stack, unsigned int __stackn, bool *__owned) {
	WSABUF *__bufs =
			__n <= __stackn ? __stack : (WSABUF *)::malloc((__SPRT_ID(size_t))__n * sizeof(WSABUF));
	*__owned = (__bufs != __stack);
	if (__bufs) {
		for (unsigned int __i = 0; __i < __n; ++__i) {
			__bufs[__i].len = (ULONG)__iov[__i].iov_len;
			__bufs[__i].buf = (CHAR *)__iov[__i].iov_base;
		}
	}
	return __bufs;
}
#endif

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(sendmsg)(int __fd, const struct __SPRT_ID(msghdr) * __message, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	// winsock has no sendmsg(); WSASendTo carries the scatter/gather iovec (ancillary
	// data in msg_control is not forwarded). A null msg_name behaves like WSASend.
	WSABUF __stack[16];
	bool __owned = false;
	unsigned int __n = (unsigned int)__message->msg_iovlen;
	WSABUF *__bufs = __sprt_win_gather(__message->msg_iov, __n, __stack, 16, &__owned);
	if (__bufs == nullptr) {
		*__sprt___errno_location() = ENOMEM;
		return -1;
	}
	DWORD __sent = 0;
	int __r = ::WSASendTo(__SPRT_WFD(__fd), __bufs, (DWORD)__n, &__sent, (DWORD)__flags,
			(const struct __SPRT_ID(sockaddr) *)__message->msg_name, (int)__message->msg_namelen,
			nullptr, nullptr);
	if (__owned) {
		::free(__bufs);
	}
	return __r == 0 ? (__SPRT_ID(ssize_t))__sent : -1;
#else
	return ::sendmsg(__fd, (const ::msghdr *)__message, __flags);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(recvmsg)(int __fd, struct __SPRT_ID(msghdr) * __message, int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS
	WSABUF __stack[16];
	bool __owned = false;
	unsigned int __n = (unsigned int)__message->msg_iovlen;
	WSABUF *__bufs = __sprt_win_gather(__message->msg_iov, __n, __stack, 16, &__owned);
	if (__bufs == nullptr) {
		*__sprt___errno_location() = ENOMEM;
		return -1;
	}
	DWORD __recvd = 0;
	DWORD __wflags = (DWORD)__flags;
	int __fromlen = (int)__message->msg_namelen;
	int __r = ::WSARecvFrom(__SPRT_WFD(__fd), __bufs, (DWORD)__n, &__recvd, &__wflags,
			(struct __SPRT_ID(sockaddr) *)__message->msg_name,
			__message->msg_name ? &__fromlen : nullptr, nullptr, nullptr);
	if (__message->msg_name) {
		__message->msg_namelen = (__SPRT_ID(socklen_t))__fromlen;
	}
	__message->msg_flags = (int)__wflags;
	if (__owned) {
		::free(__bufs);
	}
	return __r == 0 ? (__SPRT_ID(ssize_t))__recvd : -1;
#else
	return ::recvmsg(__fd, (::msghdr *)__message, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sendmmsg)(int __fd, struct __SPRT_ID(mmsghdr) * __msgvec,
		unsigned int __vlen, unsigned int __flags) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS || SPRT_APPLE
	// No native sendmmsg(): loop sendmsg() over the batch (Linux semantics - return the
	// count sent, or -1 if the first one fails).
	unsigned int __i = 0;
	for (; __i < __vlen; ++__i) {
		__SPRT_ID(ssize_t) __r = __SPRT_ID(sendmsg)(__fd, &__msgvec[__i].msg_hdr, (int)__flags);
		if (__r < 0) {
			break;
		}
		__msgvec[__i].msg_len = (unsigned int)__r;
	}
	if (__i == 0 && __vlen > 0) {
		return -1;
	}
	return (int)__i;
#else
	return ::sendmmsg(__fd, (::mmsghdr *)__msgvec, __vlen, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(recvmmsg)(int __fd, struct __SPRT_ID(mmsghdr) * __msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout) {
#if SPRT_WASM
	__SPRT_SOCK_ENOSYS();
#elif SPRT_WINDOWS || SPRT_APPLE
	// No native recvmmsg(): loop recvmsg(). The timeout is best-effort (not applied
	// between messages), matching how the batch degrades without kernel support.
	(void)__timeout;
	unsigned int __i = 0;
	for (; __i < __vlen; ++__i) {
		__SPRT_ID(ssize_t) __r = __SPRT_ID(recvmsg)(__fd, &__msgvec[__i].msg_hdr, (int)__flags);
		if (__r < 0) {
			break;
		}
		__msgvec[__i].msg_len = (unsigned int)__r;
	}
	if (__i == 0 && __vlen > 0) {
		return -1;
	}
	return (int)__i;
#else
	struct timespec __ts;
	if (__timeout) {
		__ts.tv_sec = __timeout->tv_sec;
		__ts.tv_nsec = __timeout->tv_nsec;
	}
	return ::recvmmsg(__fd, (::mmsghdr *)__msgvec, __vlen, __flags, __timeout ? &__ts : nullptr);
#endif
}

} // namespace sprt
