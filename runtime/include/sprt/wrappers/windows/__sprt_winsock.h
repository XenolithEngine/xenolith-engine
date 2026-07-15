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

// clang-format off
#define __SPRT_WSABASEERR 10000
#define __SPRT_WSAEINTR                         10004L
#define __SPRT_WSAEBADF                         10009L
#define __SPRT_WSAEACCES                        10013L
#define __SPRT_WSAEFAULT                        10014L
#define __SPRT_WSAEINVAL                        10022L
#define __SPRT_WSAEMFILE                        10024L
#define __SPRT_WSAEWOULDBLOCK                   10035L
#define __SPRT_WSAEINPROGRESS                   10036L
#define __SPRT_WSAEALREADY                      10037L
#define __SPRT_WSAENOTSOCK                      10038L
#define __SPRT_WSAEDESTADDRREQ                  10039L
#define __SPRT_WSAEMSGSIZE                      10040L
#define __SPRT_WSAEPROTOTYPE                    10041L
#define __SPRT_WSAENOPROTOOPT                   10042L
#define __SPRT_WSAEPROTONOSUPPORT               10043L
#define __SPRT_WSAESOCKTNOSUPPORT               10044L
#define __SPRT_WSAEOPNOTSUPP                    10045L
#define __SPRT_WSAEPFNOSUPPORT                  10046L
#define __SPRT_WSAEAFNOSUPPORT                  10047L
#define __SPRT_WSAEADDRINUSE                    10048L
#define __SPRT_WSAEADDRNOTAVAIL                 10049L
#define __SPRT_WSAENETDOWN                      10050L
#define __SPRT_WSAENETUNREACH                   10051L
#define __SPRT_WSAENETRESET                     10052L
#define __SPRT_WSAECONNABORTED                  10053L
#define __SPRT_WSAECONNRESET                    10054L
#define __SPRT_WSAENOBUFS                       10055L
#define __SPRT_WSAEISCONN                       10056L
#define __SPRT_WSAENOTCONN                      10057L
#define __SPRT_WSAESHUTDOWN                     10058L
#define __SPRT_WSAETOOMANYREFS                  10059L
#define __SPRT_WSAETIMEDOUT                     10060L
#define __SPRT_WSAECONNREFUSED                  10061L
#define __SPRT_WSAELOOP                         10062L
#define __SPRT_WSAENAMETOOLONG                  10063L
#define __SPRT_WSAEHOSTDOWN                     10064L
#define __SPRT_WSAEHOSTUNREACH                  10065L
#define __SPRT_WSAENOTEMPTY                     10066L
#define __SPRT_WSAEPROCLIM                      10067L
#define __SPRT_WSAEUSERS                        10068L
#define __SPRT_WSAEDQUOT                        10069L
#define __SPRT_WSAESTALE                        10070L
#define __SPRT_WSAEREMOTE                       10071L
#define __SPRT_WSASYSNOTREADY                   10091L
#define __SPRT_WSAVERNOTSUPPORTED               10092L
#define __SPRT_WSANOTINITIALISED                10093L
#define __SPRT_WSAEDISCON                       10101L
#define __SPRT_WSAENOMORE                       10102L
#define __SPRT_WSAECANCELLED                    10103L
#define __SPRT_WSAEINVALIDPROCTABLE             10104L
#define __SPRT_WSAEINVALIDPROVIDER              10105L
#define __SPRT_WSAEPROVIDERFAILEDINIT           10106L
#define __SPRT_WSASYSCALLFAILURE                10107L
#define __SPRT_WSASERVICE_NOT_FOUND             10108L
#define __SPRT_WSATYPE_NOT_FOUND                10109L
#define __SPRT_WSA_E_NO_MORE                    10110L
#define __SPRT_WSA_E_CANCELLED                  10111L
#define __SPRT_WSAEREFUSED                      10112L
#define __SPRT_WSAHOST_NOT_FOUND                11001L
#define __SPRT_WSATRY_AGAIN                     11002L
#define __SPRT_WSANO_RECOVERY                   11003L
#define __SPRT_WSANO_DATA                       11004L
#define __SPRT_WSA_QOS_RECEIVERS                11005L
#define __SPRT_WSA_QOS_SENDERS                  11006L
#define __SPRT_WSA_QOS_NO_SENDERS               11007L
#define __SPRT_WSA_QOS_NO_RECEIVERS             11008L
#define __SPRT_WSA_QOS_REQUEST_CONFIRMED        11009L
#define __SPRT_WSA_QOS_ADMISSION_FAILURE        11010L
#define __SPRT_WSA_QOS_POLICY_FAILURE           11011L
#define __SPRT_WSA_QOS_BAD_STYLE                11012L
#define __SPRT_WSA_QOS_BAD_OBJECT               11013L
#define __SPRT_WSA_QOS_TRAFFIC_CTRL_ERROR       11014L
#define __SPRT_WSA_QOS_GENERIC_ERROR            11015L
#define __SPRT_WSA_QOS_ESERVICETYPE             11016L
#define __SPRT_WSA_QOS_EFLOWSPEC                11017L
#define __SPRT_WSA_QOS_EPROVSPECBUF             11018L
#define __SPRT_WSA_QOS_EFILTERSTYLE             11019L
#define __SPRT_WSA_QOS_EFILTERTYPE              11020L
#define __SPRT_WSA_QOS_EFILTERCOUNT             11021L
#define __SPRT_WSA_QOS_EOBJLENGTH               11022L
#define __SPRT_WSA_QOS_EFLOWCOUNT               11023L
#define __SPRT_WSA_QOS_EUNKOWNPSOBJ             11024L
#define __SPRT_WSA_QOS_EPOLICYOBJ               11025L
#define __SPRT_WSA_QOS_EFLOWDESC                11026L
#define __SPRT_WSA_QOS_EPSFLOWSPEC              11027L
#define __SPRT_WSA_QOS_EPSFILTERSPEC            11028L
#define __SPRT_WSA_QOS_ESDMODEOBJ               11029L
#define __SPRT_WSA_QOS_ESHAPERATEOBJ            11030L
#define __SPRT_WSA_QOS_RESERVED_PETYPE          11031L
#define __SPRT_WSA_SECURE_HOST_NOT_FOUND        11032L
#define __SPRT_WSA_IPSEC_NAME_POLICY_ERROR      11033L

#define __SPRT_IOC_UNIX                      0x00000000
#define __SPRT_IOC_WS2                       0x08000000
#define __SPRT_IOC_PROTOCOL                  0x10000000
#define __SPRT_IOC_VENDOR                    0x18000000

#define __SPRT_IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define __SPRT_IOC_VOID        0x20000000      /* no parameters */
#define __SPRT_IOC_OUT         0x40000000      /* copy out parameters */
#define __SPRT_IOC_IN          0x80000000      /* copy in parameters */
#define __SPRT_IOC_INOUT       (__SPRT_IOC_IN|__SPRT_IOC_OUT)
// clang-format on

#define _WSAIO(x, y)                   (__SPRT_IOC_VOID|(x)|(y))
#define _WSAIOR(x, y)                  (__SPRT_IOC_OUT|(x)|(y))
#define _WSAIOW(x, y)                  (__SPRT_IOC_IN|(x)|(y))
#define _WSAIORW(x, y)                 (__SPRT_IOC_INOUT|(x)|(y))

#define _IO(x, y)        (__SPRT_IOC_VOID|((x)<<8)|(y))
#define _IOR(x, y, t)     (__SPRT_IOC_OUT|(((long)sizeof(t)&__SPRT_IOCPARM_MASK)<<16)|((x)<<8)|(y))
#define _IOW(x, y, t)     (__SPRT_IOC_IN|(((long)sizeof(t)&__SPRT_IOCPARM_MASK)<<16)|((x)<<8)|(y))

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
