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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SOCKET_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SOCKET_H_

#include <sprt/c/cross/__sprt_socket.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/cross/__sprt_fdset.h>

// POSIX <sys/socket.h> entry points in one cross-typed form: SOCKET is the descriptor and
// the socket()/accept() result (int on POSIX, the 64-bit winsock SOCKET on Windows -> no
// truncation), sockdata_t* is the data buffer (void* on POSIX, char* on Windows), and
// socklen_t the address/option lengths (int on Windows). One declaration therefore matches
// both the native libc and winsock. SPRuntimeCSysSocket.cpp forwards each to the platform's
// native call (winsock on Windows, translating the size_t/int byte counts and the
// msghdr/WSABUF layout).
__SPRT_BEGIN_DECL

SPRT_API SOCKET __SPRT_ID(socket)(int __domain, int __type, int __protocol);
SPRT_API int __SPRT_ID(socketpair)(int __domain, int __type, int __protocol, SOCKET __sv[2]);
SPRT_API int __SPRT_ID(
		bind)(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr, __SPRT_ID(socklen_t) __len);
SPRT_API int __SPRT_ID(connect)(SOCKET __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		__SPRT_ID(socklen_t) __len);
SPRT_API int __SPRT_ID(listen)(SOCKET __fd, int __backlog);
SPRT_API SOCKET __SPRT_ID(accept)(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API SOCKET __SPRT_ID(accept4)(SOCKET __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len, int __flags);
SPRT_API int __SPRT_ID(getsockname)(SOCKET __fd,
		struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API int __SPRT_ID(getpeername)(SOCKET __fd,
		struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API int __SPRT_ID(shutdown)(SOCKET __fd, int __how);

SPRT_API int __SPRT_ID(getsockopt)(SOCKET __fd, int __level, int __optname,
		sockdata_t *__SPRT_RESTRICT __optval, __SPRT_ID(socklen_t) * __SPRT_RESTRICT __optlen);
SPRT_API int __SPRT_ID(setsockopt)(SOCKET __fd, int __level, int __optname,
		const sockdata_t *__optval, __SPRT_ID(socklen_t) __optlen);

SPRT_API socksize_t __SPRT_ID(
		send)(SOCKET __fd, const sockdata_t *__buf, __SPRT_ID(size_t) __n, int __flags);
SPRT_API socksize_t __SPRT_ID(
		recv)(SOCKET __fd, sockdata_t *__buf, __SPRT_ID(size_t) __n, int __flags);
SPRT_API socksize_t __SPRT_ID(sendto)(SOCKET __fd, const sockdata_t *__buf, __SPRT_ID(size_t) __n,
		int __flags, const struct __SPRT_SOCKADDR_NAME *__addr, __SPRT_ID(socklen_t) __addr_len);
SPRT_API socksize_t __SPRT_ID(recvfrom)(SOCKET __fd, sockdata_t *__SPRT_RESTRICT __buf,
		__SPRT_ID(size_t) __n, int __flags, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __addr_len);
SPRT_API socksize_t __SPRT_ID(
		sendmsg)(SOCKET __fd, const struct __SPRT_MSGHDR_NAME *__message, int __flags);
SPRT_API socksize_t __SPRT_ID(
		recvmsg)(SOCKET __fd, struct __SPRT_MSGHDR_NAME *__message, int __flags);
SPRT_API int __SPRT_ID(sendmmsg)(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags);
SPRT_API int __SPRT_ID(recvmmsg)(SOCKET __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout);

// Windows specifics
SPRT_API int __SPRT_ID(closesocket)(SOCKET s);
SPRT_API int __SPRT_ID(ioctlsocket)(SOCKET s, long cmd, unsigned long *argp);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SOCKET_H_
