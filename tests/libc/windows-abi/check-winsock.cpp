// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/winsock.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// The core address/option structs (sockaddr*, in_addr*, hostent, addrinfo,
// WSAData, linger, ...) are already pinned against winsock by check-types.cpp.
// This TU covers the structs and constants that abi/winsock.h adds on top:
// WSAMSG, WSAPROTOCOLCHAIN, WSAPROTOCOL_INFOA/W, WSANETWORKEVENTS, and the WSA_*
// / SIO_* / FD_* / EAI_* integer constants.
//
// This file uses the shared abi_check.h harness (abi header first, under
// __SPRT_BUILD, inside namespace sprt_abi) like every other check-*.cpp. Keeping
// abi first matters: the constant asserts want the SDK's real value in scope for
// each name, so the abi header's __SPRT_* macros must be defined before the SDK's.
//
// Unlike a pure-type abi header, abi/winsock.h materializes the full *public*
// winsock/win32 surface (it drags in the public basic_api.h / __sprt_winsock.h),
// which needs three collisions defused before the SDK winsock headers are pulled:
//
//   1. It transitively `#include <sys/socket.h>`; under __SPRT_BUILD that public
//      header is an `#include_next <sys/socket.h>` meant for the on-host SPRT build,
//      with no windows-msvc target to defer to. Every type it would materialize is
//      already provided by <.../__sprt_winsock.h>, so we pre-trip its include guard.
//      (check.sh adds `-idirafter runtime/include_libc` so the now self-skipping
//      header is still findable, placed last so it never shadows the SDK.)
//   2. Its forward declarations of the win32/winsock API are `extern "C"`, so even
//      inside namespace sprt_abi they would collide (by C linkage) with the SDK's
//      identically named globals. Neutralizing __SPRT_BEGIN_DECL/__SPRT_C_FUNC keeps
//      them as ordinary namespaced C++ declarations - harmless for a layout check.
//   3. A few global macros leak (they are not namespaced): HRESULT_FROM_WIN32 (the
//      SDK re-declares it as a real FORCEINLINE function) and the in{,6}_addr field
//      aliases s_addr/s6_addr/... (the SDK gates IN_ADDR/IN6_ADDR behind
//      `#ifndef s_addr` / `#ifndef s6_addr`). Undo them after the abi block so the
//      SDK headers parse cleanly.
//   4. fdset.h declares `extern "C" int __WSAFDIsSet(SOCKET, __sprt fd_set *)` under
//      a literal extern "C" (so the neutralization in [2] cannot reach it); the SDK
//      declares the same C symbol with its own fd_set. Rename SPRT's to a private
//      symbol across the abi block so the two C declarations no longer collide.

// [1] neutralize the redundant public <sys/socket.h>
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_ 1

// [2] make SPRT's API forward-decls namespaced C++ instead of global extern "C".
// __sprt_def.h is include-guarded, so redefining here sticks through the abi block.
#include <sprt/c/bits/__sprt_def.h>
#undef __SPRT_BEGIN_DECL
#undef __SPRT_END_DECL
#undef __SPRT_C_FUNC
#define __SPRT_BEGIN_DECL
#define __SPRT_END_DECL
#define __SPRT_C_FUNC

// [4] rename SPRT's extern-"C" __WSAFDIsSet to a private symbol for the abi block.
#define __WSAFDIsSet __sprt_abi_WSAFDIsSet

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/winsock.h>
#include "abi_check.h"

// [3]+[4] undo the leaked global macros / the __WSAFDIsSet rename so the SDK headers
// parse cleanly.
#undef __WSAFDIsSet
#undef HRESULT_FROM_WIN32
#undef __HRESULT_FROM_WIN32
#undef s_addr
#undef s_host
#undef s_net
#undef s_imp
#undef s_impno
#undef s6_addr

// abi/winsock.h defines both WSAPROTOCOL_INFOA and _W variants; the SDK marks the
// ANSI struct deprecated - silence so referencing it for the layout check is clean.
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <winsock2.h> // WSADATA, WSAPROTOCOL_INFO*, WSANETWORKEVENTS, WSAE*, FD_*
#include <ws2tcpip.h> // EAI_*, WSAMSG
#include <mstcpip.h> // SIO_* ioctl helpers
#include <mswsock.h> // SIO_UDP_CONNRESET / SIO_UDP_NETRESET

// === WSA error constants (winerror.h via winsock2.h) =======================
SPRT_CONST(WSABASEERR);
SPRT_CONST(WSAEINTR);
SPRT_CONST(WSAEBADF);
SPRT_CONST(WSAEACCES);
SPRT_CONST(WSAEFAULT);
SPRT_CONST(WSAEINVAL);
SPRT_CONST(WSAEMFILE);
SPRT_CONST(WSAEWOULDBLOCK);
SPRT_CONST(WSAEINPROGRESS);
SPRT_CONST(WSAEALREADY);
SPRT_CONST(WSAENOTSOCK);
SPRT_CONST(WSAEDESTADDRREQ);
SPRT_CONST(WSAEMSGSIZE);
SPRT_CONST(WSAEPROTOTYPE);
SPRT_CONST(WSAENOPROTOOPT);
SPRT_CONST(WSAEPROTONOSUPPORT);
SPRT_CONST(WSAESOCKTNOSUPPORT);
SPRT_CONST(WSAEOPNOTSUPP);
SPRT_CONST(WSAEPFNOSUPPORT);
SPRT_CONST(WSAEAFNOSUPPORT);
SPRT_CONST(WSAEADDRINUSE);
SPRT_CONST(WSAEADDRNOTAVAIL);
SPRT_CONST(WSAENETDOWN);
SPRT_CONST(WSAENETUNREACH);
SPRT_CONST(WSAENETRESET);
SPRT_CONST(WSAECONNABORTED);
SPRT_CONST(WSAECONNRESET);
SPRT_CONST(WSAENOBUFS);
SPRT_CONST(WSAEISCONN);
SPRT_CONST(WSAENOTCONN);
SPRT_CONST(WSAESHUTDOWN);
SPRT_CONST(WSAETOOMANYREFS);
SPRT_CONST(WSAETIMEDOUT);
SPRT_CONST(WSAECONNREFUSED);
SPRT_CONST(WSAELOOP);
SPRT_CONST(WSAENAMETOOLONG);
SPRT_CONST(WSAEHOSTDOWN);
SPRT_CONST(WSAEHOSTUNREACH);
SPRT_CONST(WSAENOTEMPTY);
SPRT_CONST(WSAEPROCLIM);
SPRT_CONST(WSAEUSERS);
SPRT_CONST(WSAEDQUOT);
SPRT_CONST(WSAESTALE);
SPRT_CONST(WSAEREMOTE);
SPRT_CONST(WSASYSNOTREADY);
SPRT_CONST(WSAVERNOTSUPPORTED);
SPRT_CONST(WSANOTINITIALISED);
SPRT_CONST(WSAEDISCON);
SPRT_CONST(WSAENOMORE);
SPRT_CONST(WSAECANCELLED);
SPRT_CONST(WSAEINVALIDPROCTABLE);
SPRT_CONST(WSAEINVALIDPROVIDER);
SPRT_CONST(WSAEPROVIDERFAILEDINIT);
SPRT_CONST(WSASYSCALLFAILURE);
SPRT_CONST(WSASERVICE_NOT_FOUND);
SPRT_CONST(WSATYPE_NOT_FOUND);
SPRT_CONST(WSA_E_NO_MORE);
SPRT_CONST(WSA_E_CANCELLED);
SPRT_CONST(WSAEREFUSED);
SPRT_CONST(WSAHOST_NOT_FOUND);
SPRT_CONST(WSATRY_AGAIN);
SPRT_CONST(WSANO_RECOVERY);
SPRT_CONST(WSANO_DATA);

// === WSA QoS error constants ===============================================
SPRT_CONST(WSA_QOS_RECEIVERS);
SPRT_CONST(WSA_QOS_SENDERS);
SPRT_CONST(WSA_QOS_NO_SENDERS);
SPRT_CONST(WSA_QOS_NO_RECEIVERS);
SPRT_CONST(WSA_QOS_REQUEST_CONFIRMED);
SPRT_CONST(WSA_QOS_ADMISSION_FAILURE);
SPRT_CONST(WSA_QOS_POLICY_FAILURE);
SPRT_CONST(WSA_QOS_BAD_STYLE);
SPRT_CONST(WSA_QOS_BAD_OBJECT);
SPRT_CONST(WSA_QOS_TRAFFIC_CTRL_ERROR);
SPRT_CONST(WSA_QOS_GENERIC_ERROR);
SPRT_CONST(WSA_QOS_ESERVICETYPE);
SPRT_CONST(WSA_QOS_EFLOWSPEC);
SPRT_CONST(WSA_QOS_EPROVSPECBUF);
SPRT_CONST(WSA_QOS_EFILTERSTYLE);
SPRT_CONST(WSA_QOS_EFILTERTYPE);
SPRT_CONST(WSA_QOS_EFILTERCOUNT);
SPRT_CONST(WSA_QOS_EOBJLENGTH);
SPRT_CONST(WSA_QOS_EFLOWCOUNT);
SPRT_CONST(WSA_QOS_EUNKOWNPSOBJ);
SPRT_CONST(WSA_QOS_EPOLICYOBJ);
SPRT_CONST(WSA_QOS_EFLOWDESC);
SPRT_CONST(WSA_QOS_EPSFLOWSPEC);
SPRT_CONST(WSA_QOS_EPSFILTERSPEC);
SPRT_CONST(WSA_QOS_ESDMODEOBJ);
SPRT_CONST(WSA_QOS_ESHAPERATEOBJ);
SPRT_CONST(WSA_QOS_RESERVED_PETYPE);
SPRT_CONST(WSA_SECURE_HOST_NOT_FOUND);
SPRT_CONST(WSA_IPSEC_NAME_POLICY_ERROR);
// SPRT-only: WSA_SECURE_HOST_NOT_FOUND / WSA_IPSEC_NAME_POLICY_ERROR are not in
// this SDK's winerror.h; skip (they only seed EAI_NOSECURENAME/IPSECPOLICY).

// === WSA_* wait/io status aliases (integer, from winbase/winerror) =========
SPRT_CONST(WSA_IO_PENDING);
SPRT_CONST(WSA_IO_INCOMPLETE);
SPRT_CONST(WSA_INVALID_HANDLE);
SPRT_CONST(WSA_INVALID_PARAMETER);
SPRT_CONST(WSA_NOT_ENOUGH_MEMORY);
SPRT_CONST(WSA_OPERATION_ABORTED);
SPRT_CONST(WSA_MAXIMUM_WAIT_EVENTS);
SPRT_CONST(WSA_WAIT_FAILED);
SPRT_CONST(WSA_WAIT_EVENT_0);
SPRT_CONST(WSA_WAIT_IO_COMPLETION);
SPRT_CONST(WSA_WAIT_TIMEOUT);
SPRT_CONST(WSA_INFINITE);

// === SIO_* vendor ioctls (integer, mstcpip.h) ==============================
SPRT_CONST(SIO_UDP_CONNRESET);
SPRT_CONST(SIO_UDP_NETRESET);

// === getaddrinfo EAI_* codes (ws2tcpip.h) ==================================
SPRT_CONST(EAI_AGAIN);
SPRT_CONST(EAI_BADFLAGS);
SPRT_CONST(EAI_FAIL);
SPRT_CONST(EAI_FAMILY);
SPRT_CONST(EAI_MEMORY);
SPRT_CONST(EAI_NONAME);
SPRT_CONST(EAI_SERVICE);
SPRT_CONST(EAI_SOCKTYPE);
SPRT_CONST(EAI_NOSECURENAME);
SPRT_CONST(EAI_IPSECPOLICY);

// === FD_* network event bits (winsock2.h) ==================================
SPRT_CONST(FD_READ_BIT);
SPRT_CONST(FD_READ);
SPRT_CONST(FD_WRITE_BIT);
SPRT_CONST(FD_WRITE);
SPRT_CONST(FD_OOB_BIT);
SPRT_CONST(FD_OOB);
SPRT_CONST(FD_ACCEPT_BIT);
SPRT_CONST(FD_ACCEPT);
SPRT_CONST(FD_CONNECT_BIT);
SPRT_CONST(FD_CONNECT);
SPRT_CONST(FD_CLOSE_BIT);
SPRT_CONST(FD_CLOSE);
SPRT_CONST(FD_QOS_BIT);
SPRT_CONST(FD_QOS);
SPRT_CONST(FD_GROUP_QOS_BIT);
SPRT_CONST(FD_GROUP_QOS);
SPRT_CONST(FD_ROUTING_INTERFACE_CHANGE_BIT);
SPRT_CONST(FD_ROUTING_INTERFACE_CHANGE);
SPRT_CONST(FD_ADDRESS_LIST_CHANGE_BIT);
SPRT_CONST(FD_ADDRESS_LIST_CHANGE);
SPRT_CONST(FD_MAX_EVENTS);
SPRT_CONST(FD_ALL_EVENTS);

// === protocol-info sizing constants ========================================
SPRT_CONST(MAX_PROTOCOL_CHAIN);
SPRT_CONST(WSAPROTOCOL_LEN);

// === struct WSAData (WSADATA) ==============================================
SPRT_SIZE(WSAData);
SPRT_OFFSET(WSAData, wVersion);
SPRT_OFFSET(WSAData, wHighVersion);
SPRT_OFFSET(WSAData, iMaxSockets);
SPRT_OFFSET(WSAData, iMaxUdpDg);
SPRT_OFFSET(WSAData, lpVendorInfo);
SPRT_OFFSET(WSAData, szDescription);
SPRT_OFFSET(WSAData, szSystemStatus);

// === struct WSAMSG (WSASendMsg/WSARecvMsg) =================================
SPRT_SIZE(WSAMSG);
SPRT_OFFSET(WSAMSG, name);
SPRT_OFFSET(WSAMSG, namelen);
SPRT_OFFSET(WSAMSG, lpBuffers);
SPRT_OFFSET(WSAMSG, dwBufferCount);
SPRT_OFFSET(WSAMSG, Control);
SPRT_OFFSET(WSAMSG, dwFlags);

// === struct WSAPROTOCOLCHAIN ===============================================
SPRT_SIZE(WSAPROTOCOLCHAIN);
SPRT_OFFSET(WSAPROTOCOLCHAIN, ChainLen);
SPRT_OFFSET(WSAPROTOCOLCHAIN, ChainEntries);

// === struct WSAPROTOCOL_INFOA ==============================================
SPRT_SIZE(WSAPROTOCOL_INFOA);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwServiceFlags1);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwServiceFlags2);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwServiceFlags3);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwServiceFlags4);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwProviderFlags);
SPRT_OFFSET(WSAPROTOCOL_INFOA, ProviderId);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwCatalogEntryId);
SPRT_OFFSET(WSAPROTOCOL_INFOA, ProtocolChain);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iVersion);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iAddressFamily);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iMaxSockAddr);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iMinSockAddr);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iSocketType);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iProtocol);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iProtocolMaxOffset);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iNetworkByteOrder);
SPRT_OFFSET(WSAPROTOCOL_INFOA, iSecurityScheme);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwMessageSize);
SPRT_OFFSET(WSAPROTOCOL_INFOA, dwProviderReserved);
SPRT_OFFSET(WSAPROTOCOL_INFOA, szProtocol); // string contents skipped; offset only

// === struct WSAPROTOCOL_INFOW ==============================================
SPRT_SIZE(WSAPROTOCOL_INFOW);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwServiceFlags1);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwServiceFlags2);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwServiceFlags3);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwServiceFlags4);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwProviderFlags);
SPRT_OFFSET(WSAPROTOCOL_INFOW, ProviderId);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwCatalogEntryId);
SPRT_OFFSET(WSAPROTOCOL_INFOW, ProtocolChain);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iVersion);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iAddressFamily);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iMaxSockAddr);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iMinSockAddr);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iSocketType);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iProtocol);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iProtocolMaxOffset);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iNetworkByteOrder);
SPRT_OFFSET(WSAPROTOCOL_INFOW, iSecurityScheme);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwMessageSize);
SPRT_OFFSET(WSAPROTOCOL_INFOW, dwProviderReserved);
SPRT_OFFSET(WSAPROTOCOL_INFOW, szProtocol); // string contents skipped; offset only

// === struct WSANETWORKEVENTS ===============================================
SPRT_SIZE(WSANETWORKEVENTS);
SPRT_OFFSET(WSANETWORKEVENTS, lNetworkEvents);
SPRT_OFFSET(WSANETWORKEVENTS, iErrorCode);

// === new values (wrapper completion) =======================================
SPRT_CONST(WINSOCK_VERSION);
SPRT_CONST(FROM_PROTOCOL_INFO);
