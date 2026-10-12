// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/psapi.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// keep psapi.h on its plain (non-K32*) spellings
#define PSAPI_VERSION 1

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/psapi.h>
#include "api_check.h"

#include <windows.h>
#include <psapi.h>     // GetModuleFileNameEx* / GetMappedFileName*


SPRT_FN(GetModuleFileNameExW);
SPRT_FN(GetModuleFileNameExA);
SPRT_FN(GetMappedFileNameW);
SPRT_FN(GetMappedFileNameA);
