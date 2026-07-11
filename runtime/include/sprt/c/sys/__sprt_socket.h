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

// POSIX <sys/socket.h> entry points, uniform int-fd / void* / cross-typed everywhere.
// SPRuntimeCSysSocket.cpp forwards each to the platform's native call (winsock on
// Windows, translating the SOCKET handle and the msghdr/WSAMSG layout).
__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(socket)(int __domain, int __type, int __protocol);
SPRT_API int __SPRT_ID(socketpair)(int __domain, int __type, int __protocol, int __sv[2]);
SPRT_API int __SPRT_ID(bind)(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		__SPRT_ID(socklen_t) __len);
SPRT_API int __SPRT_ID(connect)(int __fd, const struct __SPRT_SOCKADDR_NAME *__addr,
		__SPRT_ID(socklen_t) __len);
SPRT_API int __SPRT_ID(listen)(int __fd, int __backlog);
SPRT_API int __SPRT_ID(accept)(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API int __SPRT_ID(accept4)(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len, int __flags);
SPRT_API int __SPRT_ID(getsockname)(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API int __SPRT_ID(getpeername)(int __fd, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __len);
SPRT_API int __SPRT_ID(shutdown)(int __fd, int __how);

SPRT_API int __SPRT_ID(getsockopt)(int __fd, int __level, int __optname,
		void *__SPRT_RESTRICT __optval, __SPRT_ID(socklen_t) * __SPRT_RESTRICT __optlen);
SPRT_API int __SPRT_ID(setsockopt)(int __fd, int __level, int __optname, const void *__optval,
		__SPRT_ID(socklen_t) __optlen);

SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(send)(int __fd, const void *__buf, __SPRT_ID(size_t) __n,
		int __flags);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(recv)(int __fd, void *__buf, __SPRT_ID(size_t) __n,
		int __flags);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(sendto)(int __fd, const void *__buf, __SPRT_ID(size_t) __n,
		int __flags, const struct __SPRT_SOCKADDR_NAME *__addr, __SPRT_ID(socklen_t) __addr_len);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(recvfrom)(int __fd, void *__SPRT_RESTRICT __buf,
		__SPRT_ID(size_t) __n, int __flags, struct __SPRT_SOCKADDR_NAME *__SPRT_RESTRICT __addr,
		__SPRT_ID(socklen_t) * __SPRT_RESTRICT __addr_len);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(sendmsg)(int __fd, const struct __SPRT_MSGHDR_NAME *__message,
		int __flags);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(recvmsg)(int __fd, struct __SPRT_MSGHDR_NAME *__message,
		int __flags);
SPRT_API int __SPRT_ID(sendmmsg)(int __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags);
SPRT_API int __SPRT_ID(recvmmsg)(int __fd, struct __SPRT_MMSGHDR_NAME *__msgvec,
		unsigned int __vlen, unsigned int __flags, struct __SPRT_TIMESPEC_NAME *__timeout);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SOCKET_H_
