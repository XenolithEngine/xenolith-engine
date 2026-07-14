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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_WINSOCK_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_WINSOCK_H_


#include <sprt/wrappers/windows/abi/complex_types.h>
#include <sprt/wrappers/windows/__sprt_winsock.h>
// LPSOCKADDR (used by WSAMSG) comes from the SPRT socket layer pulled in by
// __sprt_winsock.h (-> __sprt_socket.h -> cross/__sprt_socket.h -> windows_sprt/socket.h);
// no <sys/socket.h> (include_libc) dependency at the abi level.

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

#define __SPRT_SIO_UDP_CONNRESET           _WSAIOW(IOC_VENDOR,12)
#define __SPRT_SIO_UDP_NETRESET            _WSAIOW(IOC_VENDOR,15)

#define __SPRT_WSA_IO_PENDING          (__SPRT_ERROR_IO_PENDING)
#define __SPRT_WSA_IO_INCOMPLETE       (__SPRT_ERROR_IO_INCOMPLETE)
#define __SPRT_WSA_INVALID_HANDLE      (__SPRT_ERROR_INVALID_HANDLE)
#define __SPRT_WSA_INVALID_PARAMETER   (__SPRT_ERROR_INVALID_PARAMETER)
#define __SPRT_WSA_NOT_ENOUGH_MEMORY   (__SPRT_ERROR_NOT_ENOUGH_MEMORY)
#define __SPRT_WSA_OPERATION_ABORTED   (__SPRT_ERROR_OPERATION_ABORTED)

#define __SPRT_WSA_MAXIMUM_WAIT_EVENTS (__SPRT_MAXIMUM_WAIT_OBJECTS)
#define __SPRT_WSA_WAIT_FAILED         (__SPRT_WAIT_FAILED)
#define __SPRT_WSA_WAIT_EVENT_0        (__SPRT_WAIT_OBJECT_0)
#define __SPRT_WSA_WAIT_IO_COMPLETION  (__SPRT_WAIT_IO_COMPLETION)
#define __SPRT_WSA_WAIT_TIMEOUT        (__SPRT_WAIT_TIMEOUT)
#define __SPRT_WSA_INFINITE            (__SPRT_INFINITE)

#define __SPRT_EAI_AGAIN           __SPRT_WSATRY_AGAIN
#define __SPRT_EAI_BADFLAGS        __SPRT_WSAEINVAL
#define __SPRT_EAI_FAIL            __SPRT_WSANO_RECOVERY
#define __SPRT_EAI_FAMILY          __SPRT_WSAEAFNOSUPPORT
#define __SPRT_EAI_MEMORY          __SPRT_WSA_NOT_ENOUGH_MEMORY
#define __SPRT_EAI_NOSECURENAME    __SPRT_WSA_SECURE_HOST_NOT_FOUND
#define __SPRT_EAI_NONAME          __SPRT_WSAHOST_NOT_FOUND
#define __SPRT_EAI_SERVICE         __SPRT_WSATYPE_NOT_FOUND
#define __SPRT_EAI_SOCKTYPE        __SPRT_WSAESOCKTNOSUPPORT
#define __SPRT_EAI_IPSECPOLICY     __SPRT_WSA_IPSEC_NAME_POLICY_ERROR

// IPV6_* option numbers live in <sprt/c/cross/__sprt_netinet.h> (the Windows
// values are selected there via SPRT_WINDOWS) and reach here as public names
// through the <arpa/inet.h> materialization included above.

#define __SPRT_FD_READ_BIT      0
#define __SPRT_FD_READ          (1 << __SPRT_FD_READ_BIT)

#define __SPRT_FD_WRITE_BIT     1
#define __SPRT_FD_WRITE         (1 << __SPRT_FD_WRITE_BIT)

#define __SPRT_FD_OOB_BIT       2
#define __SPRT_FD_OOB           (1 << __SPRT_FD_OOB_BIT)

#define __SPRT_FD_ACCEPT_BIT    3
#define __SPRT_FD_ACCEPT        (1 << __SPRT_FD_ACCEPT_BIT)

#define __SPRT_FD_CONNECT_BIT   4
#define __SPRT_FD_CONNECT       (1 << __SPRT_FD_CONNECT_BIT)

#define __SPRT_FD_CLOSE_BIT     5
#define __SPRT_FD_CLOSE         (1 << __SPRT_FD_CLOSE_BIT)

#define __SPRT_FD_QOS_BIT       6
#define __SPRT_FD_QOS           (1 << __SPRT_FD_QOS_BIT)

#define __SPRT_FD_GROUP_QOS_BIT 7
#define __SPRT_FD_GROUP_QOS     (1 << __SPRT_FD_GROUP_QOS_BIT)

#define __SPRT_FD_ROUTING_INTERFACE_CHANGE_BIT 8
#define __SPRT_FD_ROUTING_INTERFACE_CHANGE     (1 << __SPRT_FD_ROUTING_INTERFACE_CHANGE_BIT)

#define __SPRT_FD_ADDRESS_LIST_CHANGE_BIT 9
#define __SPRT_FD_ADDRESS_LIST_CHANGE     (1 << __SPRT_FD_ADDRESS_LIST_CHANGE_BIT)

#define __SPRT_FD_MAX_EVENTS    10
#define __SPRT_FD_ALL_EVENTS    ((1 << __SPRT_FD_MAX_EVENTS) - 1)

// clang-format on

#define __SPRT_MAX_PROTOCOL_CHAIN 7
#define __SPRT_WSAPROTOCOL_LEN  255

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
	DWORD ChainEntries[__SPRT_MAX_PROTOCOL_CHAIN]; /* a list of dwCatalogEntryIds */
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
	CHAR szProtocol[__SPRT_WSAPROTOCOL_LEN + 1];
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
	WCHAR szProtocol[__SPRT_WSAPROTOCOL_LEN + 1];
} WSAPROTOCOL_INFOW, *LPWSAPROTOCOL_INFOW;

typedef struct _WSANETWORKEVENTS {
	long lNetworkEvents;
	int iErrorCode[__SPRT_FD_MAX_EVENTS];
} WSANETWORKEVENTS, *LPWSANETWORKEVENTS;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_WINSOCK_H_
