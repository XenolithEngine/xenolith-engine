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
#include <sprt/wrappers/windows/constants.h>

#include <sprt/wrappers/unistd/unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

// clang-format off
#define WSABASEERR 10000
#define WSAEINTR                         10004L
#define WSAEBADF                         10009L
#define WSAEACCES                        10013L
#define WSAEFAULT                        10014L
#define WSAEINVAL                        10022L
#define WSAEMFILE                        10024L
#define WSAEWOULDBLOCK                   10035L
#define WSAEINPROGRESS                   10036L
#define WSAEALREADY                      10037L
#define WSAENOTSOCK                      10038L
#define WSAEDESTADDRREQ                  10039L
#define WSAEMSGSIZE                      10040L
#define WSAEPROTOTYPE                    10041L
#define WSAENOPROTOOPT                   10042L
#define WSAEPROTONOSUPPORT               10043L
#define WSAESOCKTNOSUPPORT               10044L
#define WSAEOPNOTSUPP                    10045L
#define WSAEPFNOSUPPORT                  10046L
#define WSAEAFNOSUPPORT                  10047L
#define WSAEADDRINUSE                    10048L
#define WSAEADDRNOTAVAIL                 10049L
#define WSAENETDOWN                      10050L
#define WSAENETUNREACH                   10051L
#define WSAENETRESET                     10052L
#define WSAECONNABORTED                  10053L
#define WSAECONNRESET                    10054L
#define WSAENOBUFS                       10055L
#define WSAEISCONN                       10056L
#define WSAENOTCONN                      10057L
#define WSAESHUTDOWN                     10058L
#define WSAETOOMANYREFS                  10059L
#define WSAETIMEDOUT                     10060L
#define WSAECONNREFUSED                  10061L
#define WSAELOOP                         10062L
#define WSAENAMETOOLONG                  10063L
#define WSAEHOSTDOWN                     10064L
#define WSAEHOSTUNREACH                  10065L
#define WSAENOTEMPTY                     10066L
#define WSAEPROCLIM                      10067L
#define WSAEUSERS                        10068L
#define WSAEDQUOT                        10069L
#define WSAESTALE                        10070L
#define WSAEREMOTE                       10071L
#define WSASYSNOTREADY                   10091L
#define WSAVERNOTSUPPORTED               10092L
#define WSANOTINITIALISED                10093L
#define WSAEDISCON                       10101L
#define WSAENOMORE                       10102L
#define WSAECANCELLED                    10103L
#define WSAEINVALIDPROCTABLE             10104L
#define WSAEINVALIDPROVIDER              10105L
#define WSAEPROVIDERFAILEDINIT           10106L
#define WSASYSCALLFAILURE                10107L
#define WSASERVICE_NOT_FOUND             10108L
#define WSATYPE_NOT_FOUND                10109L
#define WSA_E_NO_MORE                    10110L
#define WSA_E_CANCELLED                  10111L
#define WSAEREFUSED                      10112L
#define WSAHOST_NOT_FOUND                11001L
#define WSATRY_AGAIN                     11002L
#define WSANO_RECOVERY                   11003L
#define WSANO_DATA                       11004L
#define WSA_QOS_RECEIVERS                11005L
#define WSA_QOS_SENDERS                  11006L
#define WSA_QOS_NO_SENDERS               11007L
#define WSA_QOS_NO_RECEIVERS             11008L
#define WSA_QOS_REQUEST_CONFIRMED        11009L
#define WSA_QOS_ADMISSION_FAILURE        11010L
#define WSA_QOS_POLICY_FAILURE           11011L
#define WSA_QOS_BAD_STYLE                11012L
#define WSA_QOS_BAD_OBJECT               11013L
#define WSA_QOS_TRAFFIC_CTRL_ERROR       11014L
#define WSA_QOS_GENERIC_ERROR            11015L
#define WSA_QOS_ESERVICETYPE             11016L
#define WSA_QOS_EFLOWSPEC                11017L
#define WSA_QOS_EPROVSPECBUF             11018L
#define WSA_QOS_EFILTERSTYLE             11019L
#define WSA_QOS_EFILTERTYPE              11020L
#define WSA_QOS_EFILTERCOUNT             11021L
#define WSA_QOS_EOBJLENGTH               11022L
#define WSA_QOS_EFLOWCOUNT               11023L
#define WSA_QOS_EUNKOWNPSOBJ             11024L
#define WSA_QOS_EPOLICYOBJ               11025L
#define WSA_QOS_EFLOWDESC                11026L
#define WSA_QOS_EPSFLOWSPEC              11027L
#define WSA_QOS_EPSFILTERSPEC            11028L
#define WSA_QOS_ESDMODEOBJ               11029L
#define WSA_QOS_ESHAPERATEOBJ            11030L
#define WSA_QOS_RESERVED_PETYPE          11031L
#define WSA_SECURE_HOST_NOT_FOUND        11032L
#define WSA_IPSEC_NAME_POLICY_ERROR      11033L

#define FIONREAD    _IOR('f', 127, u_long)
#define FIONBIO     _IOW('f', 126, u_long)
#define FIOASYNC    _IOW('f', 125, u_long)

#define IOC_UNIX                      0x00000000
#define IOC_WS2                       0x08000000
#define IOC_PROTOCOL                  0x10000000
#define IOC_VENDOR                    0x18000000

#define IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000      /* no parameters */
#define IOC_OUT         0x40000000      /* copy out parameters */
#define IOC_IN          0x80000000      /* copy in parameters */
#define IOC_INOUT       (IOC_IN|IOC_OUT)
#define _IO(x, y)        (IOC_VOID|((x)<<8)|(y))
#define _IOR(x, y, t)     (IOC_OUT|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))
#define _IOW(x, y, t)     (IOC_IN|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

#define _WSAIO(x, y)                   (IOC_VOID|(x)|(y))
#define _WSAIOR(x, y)                  (IOC_OUT|(x)|(y))
#define _WSAIOW(x, y)                  (IOC_IN|(x)|(y))
#define _WSAIORW(x, y)                 (IOC_INOUT|(x)|(y))

#define SIO_UDP_CONNRESET           _WSAIOW(IOC_VENDOR,12)
#define SIO_UDP_NETRESET            _WSAIOW(IOC_VENDOR,15)

#define WSA_IO_PENDING          (ERROR_IO_PENDING)
#define WSA_IO_INCOMPLETE       (ERROR_IO_INCOMPLETE)
#define WSA_INVALID_HANDLE      (ERROR_INVALID_HANDLE)
#define WSA_INVALID_PARAMETER   (ERROR_INVALID_PARAMETER)
#define WSA_NOT_ENOUGH_MEMORY   (ERROR_NOT_ENOUGH_MEMORY)
#define WSA_OPERATION_ABORTED   (ERROR_OPERATION_ABORTED)

#define WSA_MAXIMUM_WAIT_EVENTS (MAXIMUM_WAIT_OBJECTS)
#define WSA_WAIT_FAILED         (WAIT_FAILED)
#define WSA_WAIT_EVENT_0        (WAIT_OBJECT_0)
#define WSA_WAIT_IO_COMPLETION  (WAIT_IO_COMPLETION)
#define WSA_WAIT_TIMEOUT        (WAIT_TIMEOUT)
#define WSA_INFINITE            (INFINITE)

#define EAI_AGAIN           WSATRY_AGAIN
#define EAI_BADFLAGS        WSAEINVAL
#define EAI_FAIL            WSANO_RECOVERY
#define EAI_FAMILY          WSAEAFNOSUPPORT
#define EAI_MEMORY          WSA_NOT_ENOUGH_MEMORY
#define EAI_NOSECURENAME    WSA_SECURE_HOST_NOT_FOUND
#define EAI_NONAME          WSAHOST_NOT_FOUND
#define EAI_SERVICE         WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE        WSAESOCKTNOSUPPORT
#define EAI_IPSECPOLICY     WSA_IPSEC_NAME_POLICY_ERROR

#define IPV6_HOPOPTS           1 // Set/get IPv6 hop-by-hop options.
#define IPV6_HDRINCL           2 // Header is included with data.
#define IPV6_UNICAST_HOPS      4 // IP unicast hop limit.
#define IPV6_MULTICAST_IF      9 // IP multicast interface.
#define IPV6_MULTICAST_HOPS   10 // IP multicast hop limit.
#define IPV6_MULTICAST_LOOP   11 // IP multicast loopback.
#define IPV6_ADD_MEMBERSHIP   12 // Add an IP group membership.
#define IPV6_JOIN_GROUP       IPV6_ADD_MEMBERSHIP
#define IPV6_DROP_MEMBERSHIP  13 // Drop an IP group membership.
#define IPV6_LEAVE_GROUP      IPV6_DROP_MEMBERSHIP
#define IPV6_DONTFRAG         14 // Don't fragment IP datagrams.
#define IPV6_PKTINFO          19 // Receive packet information.
#define IPV6_HOPLIMIT         21 // Receive packet hop limit.
#define IPV6_PROTECTION_LEVEL 23 // Set/get IPv6 protection level.
#define IPV6_RECVIF           24 // Receive arrival interface.
#define IPV6_RECVDSTADDR      25 // Receive destination address.
#define IPV6_CHECKSUM         26 // Offset to checksum for raw IP socket send.
#define IPV6_V6ONLY           27 // Treat wildcard bind as AF_INET6-only.
#define IPV6_IFLIST           28 // Enable/Disable an interface list.
#define IPV6_ADD_IFLIST       29 // Add an interface list entry.
#define IPV6_DEL_IFLIST       30 // Delete an interface list entry.
#define IPV6_UNICAST_IF       31 // IP unicast interface.
#define IPV6_RTHDR            32 // Set/get IPv6 routing header.
#define IPV6_GET_IFLIST       33 // Get an interface list.
#define IPV6_RECVRTHDR        38 // Receive the routing header.
#define IPV6_TCLASS           39 // Packet traffic class.
#define IPV6_RECVTCLASS       40 // Receive packet traffic class.
#define IPV6_ECN              50 // IPv6 ECN codepoint.
#define IPV6_RECVECN          50 // Receive ECN codepoints in the IPv6 header.
#define IPV6_PKTINFO_EX       51 // Receive extended packet information.
#define IPV6_WFP_REDIRECT_RECORDS   60 // WFP's Connection Redirect Records
#define IPV6_WFP_REDIRECT_CONTEXT   70 // WFP's Connection Redirect Context
#define IPV6_MTU_DISCOVER           71 // Set/get path MTU discover state.
#define IPV6_MTU                    72 // Get path MTU.
#define IPV6_NRT_INTERFACE          74 // Set NRT interface constraint (outbound).
#define IPV6_RECVERR                75 // Receive ICMPv6 errors.
#define IPV6_USER_MTU               76 // Set/get app defined upper bound IP layer MTU.


#define FD_READ_BIT      0
#define FD_READ          (1 << FD_READ_BIT)

#define FD_WRITE_BIT     1
#define FD_WRITE         (1 << FD_WRITE_BIT)

#define FD_OOB_BIT       2
#define FD_OOB           (1 << FD_OOB_BIT)

#define FD_ACCEPT_BIT    3
#define FD_ACCEPT        (1 << FD_ACCEPT_BIT)

#define FD_CONNECT_BIT   4
#define FD_CONNECT       (1 << FD_CONNECT_BIT)

#define FD_CLOSE_BIT     5
#define FD_CLOSE         (1 << FD_CLOSE_BIT)

#define FD_QOS_BIT       6
#define FD_QOS           (1 << FD_QOS_BIT)

#define FD_GROUP_QOS_BIT 7
#define FD_GROUP_QOS     (1 << FD_GROUP_QOS_BIT)

#define FD_ROUTING_INTERFACE_CHANGE_BIT 8
#define FD_ROUTING_INTERFACE_CHANGE     (1 << FD_ROUTING_INTERFACE_CHANGE_BIT)

#define FD_ADDRESS_LIST_CHANGE_BIT 9
#define FD_ADDRESS_LIST_CHANGE     (1 << FD_ADDRESS_LIST_CHANGE_BIT)

#define FD_MAX_EVENTS    10
#define FD_ALL_EVENTS    ((1 << FD_MAX_EVENTS) - 1)


// clang-format on

#define WSAEVENT                HANDLE
#define LPWSAEVENT              LPHANDLE
#define WSAOVERLAPPED           OVERLAPPED
typedef struct _OVERLAPPED *LPWSAOVERLAPPED;

#define WSA_INVALID_EVENT       ((WSAEVENT)__SPRT_NULL)

typedef unsigned int GROUP;

typedef unsigned long u_long;

typedef void (*LPWSAOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwError, DWORD cbTransferred,
		LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

#define MAX_PROTOCOL_CHAIN 7
#define WSAPROTOCOL_LEN  255

typedef struct _WSAPROTOCOLCHAIN {
	int ChainLen; /* the length of the chain,     */
	/* length = 0 means layered protocol, */
	/* length = 1 means base protocol, */
	/* length > 1 means protocol chain */
	DWORD ChainEntries[MAX_PROTOCOL_CHAIN]; /* a list of dwCatalogEntryIds */
} WSAPROTOCOLCHAIN, *LPWSAPROTOCOLCHAIN;

typedef struct _WSAPROTOCOL_INFOA {
	DWORD dwServiceFlags1;
	DWORD dwServiceFlags2;
	DWORD dwServiceFlags3;
	DWORD dwServiceFlags4;
	DWORD dwProviderFlags;
	GUID ProviderId;
	DWORD dwCatalogEntryId;
	WSAPROTOCOLCHAIN ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	DWORD dwMessageSize;
	DWORD dwProviderReserved;
	CHAR szProtocol[WSAPROTOCOL_LEN + 1];
} WSAPROTOCOL_INFOA, *LPWSAPROTOCOL_INFOA;

typedef struct _WSAPROTOCOL_INFOW {
	DWORD dwServiceFlags1;
	DWORD dwServiceFlags2;
	DWORD dwServiceFlags3;
	DWORD dwServiceFlags4;
	DWORD dwProviderFlags;
	GUID ProviderId;
	DWORD dwCatalogEntryId;
	WSAPROTOCOLCHAIN ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	DWORD dwMessageSize;
	DWORD dwProviderReserved;
	WCHAR szProtocol[WSAPROTOCOL_LEN + 1];
} WSAPROTOCOL_INFOW, *LPWSAPROTOCOL_INFOW;

typedef struct _WSANETWORKEVENTS {
	long lNetworkEvents;
	int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, *LPWSANETWORKEVENTS;

__SPRT_BEGIN_DECL

WINAPI INT getaddrinfo(PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA *pHints,
		PADDRINFOA *ppResult);

WINAPI VOID freeaddrinfo(PADDRINFOA pAddrInfo);

WINAPI struct hostent *gethostbyname(const char *name);

WINAPI struct servent *getservbyname(const char *name, const char *proto);

WINAPI int getsockname(SOCKET s, struct __SPRT_SOCKADDR_NAME *name, int *namelen);

WINAPI int getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen);

WINAPI SOCKET accept(SOCKET s, struct __SPRT_SOCKADDR_NAME *addr, int *addrlen);

WINAPI int bind(SOCKET s, const struct __SPRT_SOCKADDR_NAME *addr, int namelen);

WINAPI int closesocket(SOCKET s);

WINAPI int connect(SOCKET s, const struct __SPRT_SOCKADDR_NAME *name, int namelen);

WINAPI int ioctlsocket(SOCKET s, long cmd, unsigned long *argp);

WINAPI int getpeername(SOCKET s, struct __SPRT_SOCKADDR_NAME *name, int *namelen);

WINAPI int listen(SOCKET s, int backlog);

WINAPI int recv(SOCKET s, char *buf, int len, int flags);

WINAPI int recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);

WINAPI int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		const struct timeval *timeout);

WINAPI int send(SOCKET s, const char *buf, int len, int flags);

WINAPI int sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to,
		int tolen);

WINAPI int setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen);

WINAPI int shutdown(SOCKET s, int how);

WINAPI SOCKET socket(int af, int type, int protocol);

WINAPI SOCKET WSASocketA(int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo,
		GROUP g, DWORD dwFlags);

WINAPI SOCKET WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo,
		GROUP g, DWORD dwFlags);

WINAPI int WSAIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer,
		LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned,
		LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

WINAPI int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);

WINAPI int WSACleanup(void);

WINAPI void WSASetLastError(int iError);

WINAPI int WSAGetLastError(void);

WINAPI WSAEVENT WSACreateEvent(void);

WINAPI int WSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents);

WINAPI DWORD WSAWaitForMultipleEvents(DWORD cEvents, const WSAEVENT *lphEvents, BOOL fWaitAll,
		DWORD dwTimeout, BOOL fAlertable);

WINAPI int WSAEnumNetworkEvents(SOCKET s, WSAEVENT hEventObject,
		LPWSANETWORKEVENTS lpNetworkEvents);

WINAPI BOOL WSASetEvent(WSAEVENT hEvent);

WINAPI BOOL WSAResetEvent(WSAEVENT hEvent);

WINAPI BOOL WSACloseEvent(WSAEVENT hEvent);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINSOCK_H_
