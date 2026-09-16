// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/complex_types.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/complex_types.h>
#include "abi_check.h"

#include <windows.h>
#include <tlhelp32.h>   // PROCESSENTRY32W
#include <psapi.h>      // PSAPI_WORKING_SET_EX_INFORMATION, PSAPI_WORKING_SET_EX_BLOCK

// === PROCESSENTRY32W =======================================================
SPRT_SIZE(PROCESSENTRY32W);
SPRT_OFFSET(PROCESSENTRY32W, dwSize);
SPRT_OFFSET(PROCESSENTRY32W, cntUsage);
SPRT_OFFSET(PROCESSENTRY32W, th32ProcessID);
SPRT_OFFSET(PROCESSENTRY32W, th32DefaultHeapID);
SPRT_OFFSET(PROCESSENTRY32W, th32ModuleID);
SPRT_OFFSET(PROCESSENTRY32W, cntThreads);
SPRT_OFFSET(PROCESSENTRY32W, th32ParentProcessID);
SPRT_OFFSET(PROCESSENTRY32W, pcPriClassBase);
SPRT_OFFSET(PROCESSENTRY32W, dwFlags);
SPRT_OFFSET(PROCESSENTRY32W, szExeFile);

// === PSAPI_WORKING_SET_EX_BLOCK ============================================
// All members are bitfields, so no field offsets can be taken; size only.
SPRT_SIZE(PSAPI_WORKING_SET_EX_BLOCK);

// === PSAPI_WORKING_SET_EX_INFORMATION ======================================
SPRT_SIZE(PSAPI_WORKING_SET_EX_INFORMATION);
SPRT_OFFSET(PSAPI_WORKING_SET_EX_INFORMATION, VirtualAddress);
SPRT_OFFSET(PSAPI_WORKING_SET_EX_INFORMATION, VirtualAttributes);
