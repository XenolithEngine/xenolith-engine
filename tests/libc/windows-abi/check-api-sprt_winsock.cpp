// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/__sprt_winsock.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// The winsock recipe from check-winsock.cpp: pre-trip the public <sys/socket.h>
// guard, and rename SPRT's extern-"C" __WSAFDIsSet to a private symbol across the
// api block so it does not collide with winsock2.h's (fd_set tags differ nominally).
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SOCKET_H_ 1
#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/__sprt_winsock.h>
#include "api_check.h"

#include <winsock2.h>

SPRT_FN(WSASendTo);
SPRT_FN(WSARecvFrom);
