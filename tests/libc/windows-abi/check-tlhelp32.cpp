// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/tlhelp32.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// The Toolhelp module-walk half: MODULEENTRY32W is filled by the kernel via
// Module32FirstW/NextW, so its size and every field offset must match the SDK.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/tlhelp32.h>
#include "abi_check.h"

#include <windows.h>
#include <tlhelp32.h>

// === MAX_MODULE_NAME32 =====================================================
SPRT_CONST(MAX_MODULE_NAME32);

// === struct MODULEENTRY32W =================================================
SPRT_SIZE(MODULEENTRY32W);
SPRT_OFFSET(MODULEENTRY32W, dwSize);
SPRT_OFFSET(MODULEENTRY32W, th32ModuleID);
SPRT_OFFSET(MODULEENTRY32W, th32ProcessID);
SPRT_OFFSET(MODULEENTRY32W, GlblcntUsage);
SPRT_OFFSET(MODULEENTRY32W, ProccntUsage);
SPRT_OFFSET(MODULEENTRY32W, modBaseAddr);
SPRT_OFFSET(MODULEENTRY32W, modBaseSize);
SPRT_OFFSET(MODULEENTRY32W, hModule);
SPRT_OFFSET(MODULEENTRY32W, szModule);
SPRT_OFFSET(MODULEENTRY32W, szExePath);
