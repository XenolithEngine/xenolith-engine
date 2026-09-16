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

#ifndef SPRT_WRAPPERS_WINDOWS_WINSOCK_H_
#define SPRT_WRAPPERS_WINDOWS_WINSOCK_H_

#include <sprt/wrappers/windows/complex_types.h>
#include <sprt/wrappers/windows/abi/winsock.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <poll.h>

#include <sprt/wrappers/unistd/unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>

#include <sprt/wrappers/windows/winsock_constants.h>

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI INT getaddrinfo(PCSTR pNodeName, PCSTR pServiceName,
		const ADDRINFOA *pHints, PADDRINFOA *ppResult);

__SPRT_WIN_IMPORT WINAPI VOID freeaddrinfo(PADDRINFOA pAddrInfo);

__SPRT_WIN_IMPORT WINAPI struct hostent *gethostbyname(const char *name);

__SPRT_WIN_IMPORT WINAPI struct servent *getservbyname(const char *name, const char *proto);

// The POSIX socket calls winsock provides - socket/bind/connect/listen/accept/getsockname/
// getpeername/shutdown/getsockopt/setsockopt/send/recv/sendto/recvfrom - are declared by
// <sys/socket.h> (its SPRT_WINDOWS branch emits them as these same ws2_32 imports); it is the
// primary declarer, the same way <sys/select.h> owns select(). Only the winsock-specific entry
// points remain here.

__SPRT_WIN_IMPORT WINAPI int WSAPoll(LPWSAPOLLFD fdArray, ULONG fds, INT timeout);

__SPRT_WIN_IMPORT WINAPI SOCKET WSASocketA(int af, int type, int protocol,
		LPWSAPROTOCOL_INFOA lpProtocolInfo, GROUP g, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI SOCKET WSASocketW(int af, int type, int protocol,
		LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI int WSADuplicateSocketW(SOCKET s, DWORD dwProcessId,
		LPWSAPROTOCOL_INFOW lpProtocolInfo);

__SPRT_WIN_IMPORT WINAPI int WSADuplicateSocketA(SOCKET s, DWORD dwProcessId,
		LPWSAPROTOCOL_INFOA lpProtocolInfo);

__SPRT_WIN_IMPORT WINAPI int WSAIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer,
		DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned,
		LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

__SPRT_WIN_IMPORT WINAPI int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);

__SPRT_WIN_IMPORT WINAPI int WSACleanup(void);

__SPRT_WIN_IMPORT WINAPI void WSASetLastError(int iError);

__SPRT_WIN_IMPORT WINAPI int WSAGetLastError(void);

__SPRT_WIN_IMPORT WINAPI WSAEVENT WSACreateEvent(void);

__SPRT_WIN_IMPORT WINAPI int WSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents);

__SPRT_WIN_IMPORT WINAPI DWORD WSAWaitForMultipleEvents(DWORD cEvents, const WSAEVENT *lphEvents,
		BOOL fWaitAll, DWORD dwTimeout, BOOL fAlertable);

__SPRT_WIN_IMPORT WINAPI int WSAEnumNetworkEvents(SOCKET s, WSAEVENT hEventObject,
		LPWSANETWORKEVENTS lpNetworkEvents);

__SPRT_WIN_IMPORT WINAPI BOOL WSASetEvent(WSAEVENT hEvent);

__SPRT_WIN_IMPORT WINAPI BOOL WSAResetEvent(WSAEVENT hEvent);

__SPRT_WIN_IMPORT WINAPI BOOL WSACloseEvent(WSAEVENT hEvent);

__SPRT_END_DECL

// Neutral WSAPROTOCOL_INFO name (this sysroot builds with UNICODE).
#ifdef UNICODE
typedef WSAPROTOCOL_INFOW WSAPROTOCOL_INFO;
typedef LPWSAPROTOCOL_INFOW LPWSAPROTOCOL_INFO;
#define WSASocket WSASocketW
#define WSADuplicateSocket WSADuplicateSocketW
#else
typedef WSAPROTOCOL_INFOA WSAPROTOCOL_INFO;
typedef LPWSAPROTOCOL_INFOA LPWSAPROTOCOL_INFO;
#define WSASocket WSASocketA
#define WSADuplicateSocket WSADuplicateSocketA
#endif

#ifndef WINSOCK_VERSION
#define WINSOCK_VERSION __SPRT_WINSOCK_VERSION
#endif
#ifndef FROM_PROTOCOL_INFO
#define FROM_PROTOCOL_INFO __SPRT_FROM_PROTOCOL_INFO
#endif

// The IPv6 wildcard (::) and loopback (::1) addresses. The Windows SDK imports
// these as data from ws2_32; here they are header constants (their value is
// fixed, and consumers use them by value and by address for comparison).
#ifdef __cplusplus
inline const struct __SPRT_IN6_ADDR_NAME in6addr_any = {{{0}}};
inline const struct __SPRT_IN6_ADDR_NAME in6addr_loopback = {
	{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
#else
static const struct __SPRT_IN6_ADDR_NAME in6addr_any __attribute__((unused)) = {{{0}}};
static const struct __SPRT_IN6_ADDR_NAME in6addr_loopback
		__attribute__((unused)) = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
#endif

#endif // SPRT_WRAPPERS_WINDOWS_WINSOCK_H_
