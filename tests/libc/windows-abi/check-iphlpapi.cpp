// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/iphlpapi.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/iphlpapi.h>
#include "abi_check.h"

#include <windows.h>
#include <iphlpapi.h>   // NET_IFINDEX (via ifdef.h)

// === NET_IFINDEX ===========================================================
// The header only introduces the NET_IFINDEX scalar typedef; pin its width
// against the SDK's (ULONG). No structs or integer constants to compare.
SPRT_SIZE(NET_IFINDEX);
