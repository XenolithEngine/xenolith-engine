// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/winapifamily.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/winapifamily.h>
#include "abi_check.h"

#include <windows.h>   // winapifamily.h -> WINAPI_FAMILY_*

// === WINAPI_FAMILY_* partition ids =========================================
SPRT_CONST(WINAPI_FAMILY_PC_APP);
SPRT_CONST(WINAPI_FAMILY_PHONE_APP);
SPRT_CONST(WINAPI_FAMILY_SYSTEM);
SPRT_CONST(WINAPI_FAMILY_SERVER);
SPRT_CONST(WINAPI_FAMILY_GAMES);
SPRT_CONST(WINAPI_FAMILY_DESKTOP_APP);

SPRT_CONST(WINAPI_FAMILY);
SPRT_CONST(WINAPI_PARTITION_DESKTOP);
