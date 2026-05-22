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

#include <sprt/wrappers/windows/winsock.h>

#include "dllloader.h"

#include <arpa/inet.h>

extern "C" {

uint32_t htonl(uint32_t v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&htonl)>(loader->ws2_32.htonl, v);
}

uint16_t htons(uint16_t v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&htons)>(loader->ws2_32.htons, v);
}

uint32_t ntohl(uint32_t v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&ntohl)>(loader->ws2_32.ntohl, v);
}

uint16_t ntohs(uint16_t v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&ntohs)>(loader->ws2_32.ntohs, v);
}

in_addr_t inet_addr(const char *v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&inet_addr)>(loader->ws2_32.inet_addr, v);
}

char *inet_ntoa(struct __SPRT_IN_ADDR_NAME v) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&inet_ntoa)>(loader->ws2_32.inet_ntoa, v);
}

int inet_pton(int Family, const char *__SPRT_RESTRICT pszAddrString,
		void *__SPRT_RESTRICT pAddrBuf) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&inet_pton)>(loader->ws2_32.inet_pton, Family,
			pszAddrString, pAddrBuf);
}

const char *inet_ntop(int Family, const void *__SPRT_RESTRICT pAddr,
		char *__SPRT_RESTRICT pStringBuf, socklen_t StringBufSize) SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&inet_ntop)>(loader->ws2_32.inet_ntop, Family, pAddr,
			pStringBuf, StringBufSize);
}

WINAPI INT getaddrinfo(PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA *pHints,
		PADDRINFOA *ppResult) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&getaddrinfo)>(loader->ws2_32.getaddrinfo, pNodeName,
			pServiceName, pHints, ppResult);
}

WINAPI VOID freeaddrinfo(PADDRINFOA pAddrInfo) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&freeaddrinfo)>(loader->ws2_32.freeaddrinfo, pAddrInfo);
}

WINAPI int gethostname(char *name, size_t namelen) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&gethostname)>(loader->ws2_32.gethostname, name, namelen);
}

WINAPI struct hostent *gethostbyname(const char *name) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&gethostbyname)>(loader->ws2_32.gethostbyname, name);
}

WINAPI struct servent *getservbyname(const char *name, const char *proto) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&getservbyname)>(loader->ws2_32.getservbyname, name, proto);
}

WINAPI int getsockname(SOCKET s, struct __SPRT_SOCKADDR_NAME *name, int *namelen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&getsockname)>(loader->ws2_32.getsockname, s, name,
			namelen);
}

WINAPI int getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&getsockopt)>(loader->ws2_32.getsockopt, s, level, optname,
			optval, optlen);
}

WINAPI SOCKET accept(SOCKET s, struct __SPRT_SOCKADDR_NAME *addr, int *addrlen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&accept)>(loader->ws2_32.accept, s, addr, addrlen);
}

WINAPI int bind(SOCKET s, const struct __SPRT_SOCKADDR_NAME *addr, int namelen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&bind)>(loader->ws2_32.bind, s, addr, namelen);
}

WINAPI int closesocket(SOCKET s) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&closesocket)>(loader->ws2_32.closesocket, s);
}

WINAPI int connect(SOCKET s, const struct __SPRT_SOCKADDR_NAME *name, int namelen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&connect)>(loader->ws2_32.connect, s, name, namelen);
}

WINAPI int ioctlsocket(SOCKET s, long cmd, unsigned long *argp) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&ioctlsocket)>(loader->ws2_32.ioctlsocket, s, cmd, argp);
}

WINAPI int getpeername(SOCKET s, struct __SPRT_SOCKADDR_NAME *name, int *namelen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&getpeername)>(loader->ws2_32.getpeername, s, name,
			namelen);
}

WINAPI int listen(SOCKET s, int backlog) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&listen)>(loader->ws2_32.listen, s, backlog);
}

WINAPI int recv(SOCKET s, char *buf, int len, int flags) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&recv)>(loader->ws2_32.recv, s, buf, len, flags);
}

WINAPI int recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&recvfrom)>(loader->ws2_32.recvfrom, s, buf, len, flags,
			from, fromlen);
}

WINAPI int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		const struct timeval *timeout) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&select)>(loader->ws2_32.select, nfds, readfds, writefds,
			exceptfds, timeout);
}

WINAPI int send(SOCKET s, const char *buf, int len, int flags) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&send)>(loader->ws2_32.send, s, buf, len, flags);
}

WINAPI int sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to,
		int tolen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&sendto)>(loader->ws2_32.sendto, s, buf, len, flags, to,
			tolen);
}

WINAPI int setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&setsockopt)>(loader->ws2_32.setsockopt, s, level, optname,
			optval, optlen);
}

WINAPI int shutdown(SOCKET s, int how) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&shutdown)>(loader->ws2_32.shutdown, s, how);
}

WINAPI SOCKET socket(int af, int type, int protocol) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&socket)>(loader->ws2_32.socket, af, type, protocol);
}

WINAPI SOCKET WSASocketA(int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo,
		GROUP g, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSASocketA)>(loader->ws2_32.WSASocketA, af, type, protocol,
			lpProtocolInfo, g, dwFlags);
}

WINAPI SOCKET WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo,
		GROUP g, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSASocketW)>(loader->ws2_32.WSASocketW, af, type, protocol,
			lpProtocolInfo, g, dwFlags);
}

WINAPI int WSAIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer,
		LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned,
		LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAIoctl)>(loader->ws2_32.WSAIoctl, s, dwIoControlCode,
			lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped,
			lpCompletionRoutine);
}

WINAPI int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAStartup)>(loader->ws2_32.WSAStartup, wVersionRequested,
			lpWSAData);
}

WINAPI int WSACleanup(void) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSACleanup)>(loader->ws2_32.WSACleanup);
}

WINAPI void WSASetLastError(int iError) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSASetLastError)>(loader->ws2_32.WSASetLastError, iError);
}

WINAPI int WSAGetLastError(void) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAGetLastError)>(loader->ws2_32.WSAGetLastError);
}

WINAPI WSAEVENT WSACreateEvent(void) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSACreateEvent)>(loader->ws2_32.WSACreateEvent);
}

WINAPI int WSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAEventSelect)>(loader->ws2_32.WSAEventSelect, s,
			hEventObject, lNetworkEvents);
}

WINAPI DWORD WSAWaitForMultipleEvents(DWORD cEvents, const WSAEVENT *lphEvents, BOOL fWaitAll,
		DWORD dwTimeout, BOOL fAlertable) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAWaitForMultipleEvents)>(
			loader->ws2_32.WSAWaitForMultipleEvents, cEvents, lphEvents, fWaitAll, dwTimeout,
			fAlertable);
}

WINAPI int WSAEnumNetworkEvents(SOCKET s, WSAEVENT hEventObject,
		LPWSANETWORKEVENTS lpNetworkEvents) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAEnumNetworkEvents)>(loader->ws2_32.WSAEnumNetworkEvents,
			s, hEventObject, lpNetworkEvents);
}

WINAPI BOOL WSASetEvent(WSAEVENT hEvent) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSASetEvent)>(loader->ws2_32.WSASetEvent, hEvent);
}

WINAPI BOOL WSAResetEvent(WSAEVENT hEvent) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSAResetEvent)>(loader->ws2_32.WSAResetEvent, hEvent);
}

WINAPI BOOL WSACloseEvent(WSAEVENT hEvent) {
	auto loader = sprt::DllLoader::get();
	return loader->ws2_32.call<decltype(&WSACloseEvent)>(loader->ws2_32.WSACloseEvent, hEvent);
}
}
