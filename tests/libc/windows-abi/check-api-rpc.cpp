// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/rpc.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/rpc.h>
#include "api_check.h"

#include <windows.h>
#include <rpc.h>       // __RPC_FAR / RPC_STATUS spelling rpcdce.h builds on (LEAN windows.h skips it)
#include <rpcdce.h>

SPRT_FN(UuidCreate);
SPRT_FN(UuidCreateSequential);
SPRT_FN(UuidToStringA);
SPRT_FN(UuidToStringW);
SPRT_FN(RpcStringFreeA);
SPRT_FN(RpcStringFreeW);
