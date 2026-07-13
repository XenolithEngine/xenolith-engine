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

#include <sys/__sockdef.h>

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(sa_family_t) sa_family_t;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;

#if SPRT_WINDOWS
// __SPRT_WIN_IMPORT / WINAPI for the winsock forward declarations below (socket.h owns them).
#include <sprt/wrappers/windows/basic_types.h>
#endif

#if SPRT_WASM
// fd_set + FD_*/FD_SETSIZE. POSIX keeps these in <sys/select.h>, but a lot of BSD-
// derived socket code (e.g. curl's cshutdn.c) uses FD_SET/FD_SETSIZE having included
// only <sys/socket.h>, mirroring the glibc header layout - so surface them here too.
#include <sys/select.h>
#endif

// struct linger / msghdr / cmsghdr / mmsghdr / ucred come from the per-platform cross
// <sys/socket.h> surface pulled in above (sprt/c/sys/__sprt_socket.h). Map the public
// CMSG_*/SCM_* spellings onto the per-platform __SPRT_CMSG_* helpers.
// clang-format off
#define CMSG_DATA(cmsg)     __SPRT_CMSG_DATA(cmsg)
#define CMSG_ALIGN(len)     __SPRT_CMSG_ALIGN(len)
#define CMSG_SPACE(len)     __SPRT_CMSG_SPACE(len)
#define CMSG_LEN(len)       __SPRT_CMSG_LEN(len)
#define CMSG_FIRSTHDR(mhdr) __SPRT_CMSG_FIRSTHDR(mhdr)
#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr(mhdr, cmsg)

__SPRT_BEGIN_DECL
struct cmsghdr *__cmsg_nxthdr(struct msghdr *__mhdr, struct cmsghdr *__cmsg);
__SPRT_END_DECL

struct timespec; // forward decl for recvmmsg timeout (real def via <time.h>)

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC SOCKET socket(int __domain, int __type, int __protocol) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(socket)(__domain, __type, __protocol);
}
#endif

SPRT_UMBRELLA_FUNC int bind(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		socklen_t __len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(bind)(__fd, __addr, __len);
}
#endif

SPRT_UMBRELLA_FUNC int connect(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		socklen_t __len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(connect)(__fd, __addr, __len);
}
#endif

SPRT_UMBRELLA_FUNC int listen(SOCKET __fd, int __backlog) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(listen)(__fd, __backlog);
}
#endif

SPRT_UMBRELLA_FUNC SOCKET accept(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(accept)(__fd, __addr, __len);
}
#endif

SPRT_UMBRELLA_FUNC int getsockname(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(getsockname)(__fd, __addr, __len);
}
#endif

SPRT_UMBRELLA_FUNC int getpeername(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(getpeername)(__fd, __addr, __len);
}
#endif

SPRT_UMBRELLA_FUNC int shutdown(SOCKET __fd, int __how) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(shutdown)(__fd, __how);
}
#endif

SPRT_UMBRELLA_FUNC int getsockopt(SOCKET __fd, int __level, int __optname,
		sockdata_t *__SPRT_RESTRICT __optval, socklen_t *__SPRT_RESTRICT __optlen) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(getsockopt)(__fd, __level, __optname, __optval, __optlen);
}
#endif

SPRT_UMBRELLA_FUNC int setsockopt(SOCKET __fd, int __level, int __optname,
		const sockdata_t *__optval, socklen_t __optlen) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(setsockopt)(__fd, __level, __optname, __optval, __optlen);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t send(SOCKET __fd, const sockdata_t *__buf, size_t __n,
		int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(send)(__fd, __buf, __n, __flags);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t recv(SOCKET __fd, sockdata_t *__buf, size_t __n,
		int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(recv)(__fd, __buf, __n, __flags);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t sendto(SOCKET __fd, const sockdata_t *__buf, size_t __n, int __flags,
		const struct __SPRT_SOCKADDR_NAME *__addr, socklen_t __addr_len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(sendto)(__fd, __buf, __n, __flags, __addr, __addr_len);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t recvfrom(SOCKET __fd, sockdata_t *__SPRT_RESTRICT __buf, size_t __n,
		int __flags, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __addr_len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(recvfrom)(__fd, __buf, __n, __flags, __addr, __addr_len);
}
#endif

// winsock has no socketpair/accept4/sendmsg/recvmsg/sendmmsg/recvmmsg: umbrella on every target,
// the __sprt_* wrapper emulating them on Windows (socketpair -> ENOSYS, accept4 -> accept +
// FIONBIO, sendmsg/recvmsg -> WSASendTo/WSARecvFrom, sendmmsg/recvmmsg -> per-message loop).
// Same as pselect() in <sys/select.h>.

SPRT_UMBRELLA_FUNC int socketpair(int __domain, int __type, int __protocol,
		SOCKET __sv[2]) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(socketpair)(__domain, __type, __protocol, __sv);
}
#endif

SPRT_UMBRELLA_FUNC SOCKET accept4(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		socklen_t *__SPRT_RESTRICT __len, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(accept4)(__fd, __addr, __len, __flags);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t sendmsg(SOCKET __fd, const struct msghdr *__message,
		int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(sendmsg)(__fd, __message, __flags);
}
#endif

SPRT_UMBRELLA_FUNC socksize_t recvmsg(SOCKET __fd, struct msghdr *__message,
		int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(recvmsg)(__fd, __message, __flags);
}
#endif

SPRT_UMBRELLA_FUNC int sendmmsg(SOCKET __fd, struct mmsghdr *__msgvec, unsigned int __vlen,
		unsigned int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(sendmmsg)(__fd, __msgvec, __vlen, __flags);
}
#endif

SPRT_UMBRELLA_FUNC int recvmmsg(SOCKET __fd, struct mmsghdr *__msgvec, unsigned int __vlen,
		unsigned int __flags, struct timespec *__timeout) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(recvmmsg)(__fd, __msgvec, __vlen, __flags, __timeout);
}
#endif

SPRT_UMBRELLA_FUNC int closesocket(SOCKET s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(closesocket)(s);
}
#endif

SPRT_UMBRELLA_FUNC int ioctlsocket(SOCKET s, long cmd, unsigned long *argp) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(ioctlsocket)(s, cmd, argp);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_
