// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/dl_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/dl_api.h>
#include "api_check.h"

#include <windows.h>

SPRT_FN(VirtualQuery);
SPRT_FN(VirtualQueryEx);
SPRT_FN(ExitProcess);
SPRT_FN(LoadLibraryW);
SPRT_FN(LoadLibraryA);
SPRT_FN(LoadLibraryExW);
SPRT_FN(LoadLibraryExA);
SPRT_FN(FreeLibrary);
SPRT_FN(GetProcAddress);
SPRT_FN(GetModuleHandleW);
SPRT_FN(GetModuleHandleA);
SPRT_FN(GetModuleHandleExA);
SPRT_FN(GetModuleHandleExW);
SPRT_FN(GetModuleFileNameW);
SPRT_FN(GetModuleFileNameA);
SPRT_FN(RtlPcToFileHeader);
