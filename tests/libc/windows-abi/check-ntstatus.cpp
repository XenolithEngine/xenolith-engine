// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/ntstatus.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// A minimal NTSTATUS subset; llvm's ErrorHandling.cpp checks STATUS_DELETE_PENDING
// against RtlGetLastNtStatus(), and the WoW64 debug loop reports STATUS_WX86_*.
// The full set lives in <ntstatus.h> (STATUS_SUCCESS also leaks via winnt.h).

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/ntstatus.h>
#include "abi_check.h"

#include <windows.h>
#include <ntstatus.h>  // STATUS_WX86_*, STATUS_DELETE_PENDING, ... (winnt.h STATUS_* re-def
                       // suppressed by -Wno-macro-redefined)

// NTSTATUS-cast codes: compare the 32-bit bit pattern (SDK spells them signed).
SPRT_STATUS(STATUS_SUCCESS);
SPRT_STATUS(STATUS_WX86_SINGLE_STEP);
SPRT_STATUS(STATUS_WX86_BREAKPOINT);
SPRT_STATUS(STATUS_DELETE_PENDING);
SPRT_STATUS(STATUS_OBJECT_NAME_NOT_FOUND);
SPRT_STATUS(STATUS_ACCESS_DENIED);
