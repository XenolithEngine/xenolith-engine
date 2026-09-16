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
#define __SPRT_SIO_UDP_CONNRESET           _WSAIOW(__SPRT_IOC_VENDOR,12)
#define __SPRT_SIO_UDP_NETRESET            _WSAIOW(__SPRT_IOC_VENDOR,15)

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

#define __SPRT_WINSOCK_VERSION         __SPRT_MAKEWORD(2, 2)
#define __SPRT_FROM_PROTOCOL_INFO      (-1)

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
