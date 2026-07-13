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

#ifndef SPRT_WRAPPERS_WINDOWS___SPRT_WINSOCK_H_
#define SPRT_WRAPPERS_WINDOWS___SPRT_WINSOCK_H_

#include <sprt/wrappers/windows/basic_api.h>

#include <sprt/c/cross/__sprt_polltypes.h>
#include <sprt/c/sys/__sprt_socket.h>

// clang-format off
#define IOC_UNIX                      0x00000000
#define IOC_WS2                       0x08000000
#define IOC_PROTOCOL                  0x10000000
#define IOC_VENDOR                    0x18000000

#define IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000      /* no parameters */
#define IOC_OUT         0x40000000      /* copy out parameters */
#define IOC_IN          0x80000000      /* copy in parameters */
#define IOC_INOUT       (IOC_IN|IOC_OUT)
// clang-format on

#define _WSAIO(x, y)                   (IOC_VOID|(x)|(y))
#define _WSAIOR(x, y)                  (IOC_OUT|(x)|(y))
#define _WSAIOW(x, y)                  (IOC_IN|(x)|(y))
#define _WSAIORW(x, y)                 (IOC_INOUT|(x)|(y))

#define _IO(x, y)        (IOC_VOID|((x)<<8)|(y))
#define _IOR(x, y, t)     (IOC_OUT|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))
#define _IOW(x, y, t)     (IOC_IN|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

#define FIONREAD    _IOR('f', 127, u_long)
#define FIONBIO     _IOW('f', 126, u_long)
#define FIOASYNC    _IOW('f', 125, u_long)

// WSAPoll() event flags (mstcpip). Guarded so an ambient <poll.h> cannot clash.
#ifndef POLLRDNORM
#define POLLRDNORM  __SPRT_POLLRDNORM
#define POLLRDBAND  __SPRT_POLLRDBAND
#define POLLIN      __SPRT_POLLIN
#define POLLPRI     __SPRT_POLLPRI
#define POLLWRNORM  __SPRT_POLLWRNORM
#define POLLOUT     __SPRT_POLLOUT
#define POLLWRBAND  __SPRT_POLLWRBAND
#define POLLERR     __SPRT_POLLERR
#define POLLHUP     __SPRT_POLLHUP
#define POLLNVAL    __SPRT_POLLNVAL
#endif

#define WSAEVENT                HANDLE
#define LPWSAEVENT              LPHANDLE
#define WSAOVERLAPPED           OVERLAPPED
typedef struct _OVERLAPPED *LPWSAOVERLAPPED;

#define WSA_INVALID_EVENT       ((WSAEVENT)__SPRT_NULL)

typedef unsigned int GROUP;

typedef unsigned long u_long;

typedef struct __SPRT_POLLFD_NAME WSAPOLLFD, *PWSAPOLLFD, *LPWSAPOLLFD;

// WSABUF: winsock's scatter/gather buffer (note the {len, buf} order vs iovec's
// {base, len}); used to carry sendmsg()/recvmsg() iovecs through WSASendTo/WSARecvFrom.
typedef struct __sprt_wsabuf {
	ULONG len;
	CHAR *buf;
} WSABUF, *LPWSABUF;

typedef void (*LPWSAOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwError, DWORD cbTransferred,
		LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI int WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
		LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct __SPRT_SOCKADDR_NAME *lpTo,
		int iTolen, LPWSAOVERLAPPED lpOverlapped,
		LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

__SPRT_WIN_IMPORT WINAPI int WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
		LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct __SPRT_SOCKADDR_NAME *lpFrom,
		int *lpFromlen, LPWSAOVERLAPPED lpOverlapped,
		LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_WINSOCK_H_
