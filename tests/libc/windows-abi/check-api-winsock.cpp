// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/winsock.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// The winsock recipe from check-winsock.cpp: pre-trip the public <sys/socket.h>
// guard, and rename SPRT's extern-"C" __WSAFDIsSet to a private symbol across the
// api block so it does not collide with winsock2.h's (fd_set tags differ nominally).
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_ 1
#define CORE_RUNTIME_INCLUDE_LIBC_ARPA_INET_H_ 1
#define CORE_RUNTIME_INCLUDE_LIBC_NETINET_TCP_H_ 1
#define CORE_RUNTIME_INCLUDE_LIBC_POLL_H_ 1
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SELECT_H_ 1
#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/winsock.h>
#include "api_check.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

SPRT_FN(getaddrinfo);
SPRT_FN(freeaddrinfo);
SPRT_FN(gethostbyname);
SPRT_FN(getservbyname);
SPRT_FN(WSAPoll);
SPRT_FN(WSASocketA);
SPRT_FN(WSASocketW);
SPRT_FN(WSADuplicateSocketW);
SPRT_FN(WSADuplicateSocketA);
SPRT_FN(WSAIoctl);
SPRT_FN(WSAStartup);
SPRT_FN(WSACleanup);
SPRT_FN(WSASetLastError);
SPRT_FN(WSAGetLastError);
SPRT_FN(WSACreateEvent);
SPRT_FN(WSAEventSelect);
SPRT_FN(WSAWaitForMultipleEvents);
SPRT_FN(WSAEnumNetworkEvents);
SPRT_FN(WSASetEvent);
SPRT_FN(WSAResetEvent);
SPRT_FN(WSACloseEvent);
