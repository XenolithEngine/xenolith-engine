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
#include <sprt/wrappers/windows/__sprt_winsock.h>

#include <sprt/wrappers/unistd/unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>

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

// IPV6_* option numbers live in <sprt/c/cross/__sprt_netinet.h> (the Windows
// values are selected there via SPRT_WINDOWS) and reach here as public names
// through the <arpa/inet.h> materialization included above.

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

#define MAX_PROTOCOL_CHAIN 7
#define WSAPROTOCOL_LEN  255

/*
 * WSAMSG -- for WSASendMsg
 */

typedef struct _WSAMSG {
	LPSOCKADDR name; /* Remote address */
	INT namelen; /* Remote address length */
	LPWSABUF lpBuffers; /* Data buffer array */
	ULONG dwBufferCount; /* Number of elements in the array */
	WSABUF Control; /* Control buffer */
	ULONG dwFlags; /* Flags */
} WSAMSG, *PWSAMSG, *LPWSAMSG;

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

#endif // SPRT_WRAPPERS_WINDOWS_WINSOCK_H_
