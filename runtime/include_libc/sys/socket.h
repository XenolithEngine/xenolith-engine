/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/socket.h>

#else

#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/bits/iovec.h>

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(sa_family_t) sa_family_t;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;

#if SPRT_WASM
// fd_set + FD_*/FD_SETSIZE. POSIX keeps these in <sys/select.h>, but a lot of BSD-
// derived socket code (e.g. curl's cshutdn.c) uses FD_SET/FD_SETSIZE having included
// only <sys/socket.h>, mirroring the glibc header layout - so surface them here too.
#include <sys/select.h>
#endif

// struct linger / msghdr / cmsghdr / mmsghdr / ucred and the AF_*/SOCK_*/SO_*/MSG_*
// constants come from the per-platform cross <sys/socket.h> surface pulled in above
// (sprt/c/sys/__sprt_socket.h). Map the public CMSG_*/SCM_* spellings onto the
// per-platform __SPRT_CMSG_* helpers.
// clang-format off
#define CMSG_DATA(cmsg)     __SPRT_CMSG_DATA(cmsg)
#define CMSG_ALIGN(len)     __SPRT_CMSG_ALIGN(len)
#define CMSG_SPACE(len)     __SPRT_CMSG_SPACE(len)
#define CMSG_LEN(len)       __SPRT_CMSG_LEN(len)
#define CMSG_FIRSTHDR(mhdr) __SPRT_CMSG_FIRSTHDR(mhdr)
#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr(mhdr, cmsg)

#ifndef SCM_RIGHTS
#define SCM_RIGHTS 0x01
#endif
#ifndef SCM_CREDENTIALS
#define SCM_CREDENTIALS 0x02
#endif
// clang-format on

__SPRT_BEGIN_DECL
struct cmsghdr *__cmsg_nxthdr(struct msghdr *__mhdr, struct cmsghdr *__cmsg);
__SPRT_END_DECL

struct timespec; // forward decl for recvmmsg timeout (real def via <time.h>)

#if SPRT_WASM

// wasm: the public socket symbols are ENOSYS stubs in libc_impl/src/wasm/socket.cc.
__SPRT_BEGIN_DECL

int socket(int __domain, int __type, int __protocol);
int socketpair(int __domain, int __type, int __protocol, int __sv[2]);
int bind(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len);
int connect(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len);
int listen(int __fd, int __backlog);
int accept(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int accept4(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len, int __flags);
int getsockname(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int getpeername(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len);
int shutdown(int __fd, int __how);
int getsockopt(int __fd, int __level, int __optname, void *__SPRT_RESTRICT __optval,
		socklen_t *__SPRT_RESTRICT __optlen);
int setsockopt(int __fd, int __level, int __optname, const void *__optval, socklen_t __optlen);
ssize_t send(int __fd, const void *__buf, size_t __n, int __flags);
ssize_t recv(int __fd, void *__buf, size_t __n, int __flags);
ssize_t sendto(int __fd, const void *__buf, size_t __n, int __flags,
		const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __addr_len);
ssize_t recvfrom(int __fd, void *__SPRT_RESTRICT __buf, size_t __n, int __flags,
		struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr, socklen_t *__SPRT_RESTRICT __addr_len);
ssize_t sendmsg(int __fd, const struct msghdr *__message, int __flags);
ssize_t recvmsg(int __fd, struct msghdr *__message, int __flags);
int sendmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen, unsigned int __flags);
int recvmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen, unsigned int __flags,
		struct timespec *__timeout);

__SPRT_END_DECL

#elif !defined(SPRT_WRAPPERS_WINDOWS_WINSOCK_H_)

// Other controlled targets (Windows): expose portable POSIX sockets as force-inline
// forwarders onto the __sprt_* backing (winsock on Windows). Guarded against winsock.h,
// which declares these same names with the native winsock (SOCKET) signatures - a TU
// wanting the raw winsock API includes that header instead.
__SPRT_BEGIN_DECL

SPRT_FORCEINLINE int socket(int __domain, int __type, int __protocol) __SPRT_NOEXCEPT {
	return __SPRT_ID(socket)(__domain, __type, __protocol);
}
SPRT_FORCEINLINE int socketpair(int __domain, int __type, int __protocol, int __sv[2])
		__SPRT_NOEXCEPT {
	return __SPRT_ID(socketpair)(__domain, __type, __protocol, __sv);
}
SPRT_FORCEINLINE int bind(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len)
		__SPRT_NOEXCEPT {
	return __SPRT_ID(bind)(__fd, __addr, __len);
}
SPRT_FORCEINLINE int connect(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __len)
		__SPRT_NOEXCEPT {
	return __SPRT_ID(connect)(__fd, __addr, __len);
}
SPRT_FORCEINLINE int listen(int __fd, int __backlog) __SPRT_NOEXCEPT {
	return __SPRT_ID(listen)(__fd, __backlog);
}
SPRT_FORCEINLINE int accept(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) __SPRT_NOEXCEPT {
	return __SPRT_ID(accept)(__fd, __addr, __len);
}
SPRT_FORCEINLINE int accept4(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len, int __flags) __SPRT_NOEXCEPT {
	return __SPRT_ID(accept4)(__fd, __addr, __len, __flags);
}
SPRT_FORCEINLINE int getsockname(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) __SPRT_NOEXCEPT {
	return __SPRT_ID(getsockname)(__fd, __addr, __len);
}
SPRT_FORCEINLINE int getpeername(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) __SPRT_NOEXCEPT {
	return __SPRT_ID(getpeername)(__fd, __addr, __len);
}
SPRT_FORCEINLINE int shutdown(int __fd, int __how) __SPRT_NOEXCEPT {
	return __SPRT_ID(shutdown)(__fd, __how);
}
SPRT_FORCEINLINE int getsockopt(int __fd, int __level, int __optname,
		void *__SPRT_RESTRICT __optval, socklen_t *__SPRT_RESTRICT __optlen) __SPRT_NOEXCEPT {
	return __SPRT_ID(getsockopt)(__fd, __level, __optname, __optval, __optlen);
}
SPRT_FORCEINLINE int setsockopt(int __fd, int __level, int __optname, const void *__optval,
		socklen_t __optlen) __SPRT_NOEXCEPT {
	return __SPRT_ID(setsockopt)(__fd, __level, __optname, __optval, __optlen);
}
SPRT_FORCEINLINE ssize_t send(int __fd, const void *__buf, size_t __n, int __flags)
		__SPRT_NOEXCEPT {
	return __SPRT_ID(send)(__fd, __buf, __n, __flags);
}
SPRT_FORCEINLINE ssize_t recv(int __fd, void *__buf, size_t __n, int __flags) __SPRT_NOEXCEPT {
	return __SPRT_ID(recv)(__fd, __buf, __n, __flags);
}
SPRT_FORCEINLINE ssize_t sendto(int __fd, const void *__buf, size_t __n, int __flags,
		const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __addr_len) __SPRT_NOEXCEPT {
	return __SPRT_ID(sendto)(__fd, __buf, __n, __flags, __addr, __addr_len);
}
SPRT_FORCEINLINE ssize_t recvfrom(int __fd, void *__SPRT_RESTRICT __buf, size_t __n, int __flags,
		struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __addr_len) __SPRT_NOEXCEPT {
	return __SPRT_ID(recvfrom)(__fd, __buf, __n, __flags, __addr, __addr_len);
}
SPRT_FORCEINLINE ssize_t sendmsg(int __fd, const struct msghdr *__message, int __flags)
		__SPRT_NOEXCEPT {
	return __SPRT_ID(sendmsg)(__fd, __message, __flags);
}
SPRT_FORCEINLINE ssize_t recvmsg(int __fd, struct msghdr *__message, int __flags) __SPRT_NOEXCEPT {
	return __SPRT_ID(recvmsg)(__fd, __message, __flags);
}
SPRT_FORCEINLINE int sendmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen,
		unsigned int __flags) __SPRT_NOEXCEPT {
	return __SPRT_ID(sendmmsg)(__fd, __msgvec, __vlen, __flags);
}
SPRT_FORCEINLINE int recvmmsg(int __fd, struct mmsghdr *__msgvec, unsigned int __vlen,
		unsigned int __flags, struct timespec *__timeout) __SPRT_NOEXCEPT {
	return __SPRT_ID(recvmmsg)(__fd, __msgvec, __vlen, __flags, __timeout);
}

__SPRT_END_DECL

#endif // SPRT_WASM / winsock guard

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_
