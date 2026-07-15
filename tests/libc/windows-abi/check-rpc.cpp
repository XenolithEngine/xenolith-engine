// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/rpc.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// A minimal MS-RPC surface: the UUID generation/formatting used to mint unique
// names (e.g. LLDB's anonymous pipe names). RPC_S_* live in winerror.h; UUID and
// RPC_STATUS come from the RPC headers pulled by <rpc.h>.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/rpc.h>
#include "abi_check.h"

#include <windows.h>
#include <rpc.h>

// === RPC_STATUS codes (winerror.h) =========================================
SPRT_CONST(RPC_S_OK);
SPRT_CONST(RPC_S_UUID_LOCAL_ONLY);
SPRT_CONST(RPC_S_UUID_NO_ADDRESS);

// === types =================================================================
SPRT_SIZE(UUID);         // == GUID
SPRT_SIZE(RPC_STATUS);   // == long
