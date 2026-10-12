// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/winver.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/winver.h>
#include "api_check.h"

#include <windows.h>
#include <winver.h>

SPRT_FN(GetVersionExA);
SPRT_FN(GetVersionExW);
SPRT_FN(VerSetConditionMask);
SPRT_FN(VerifyVersionInfoW);
SPRT_FN(GetFileVersionInfoSizeA);
SPRT_FN(GetFileVersionInfoSizeW);
SPRT_FN(GetFileVersionInfoA);
SPRT_FN(GetFileVersionInfoW);
SPRT_FN(GetFileVersionInfoSizeExA);
SPRT_FN(GetFileVersionInfoSizeExW);
SPRT_FN(GetFileVersionInfoExA);
SPRT_FN(GetFileVersionInfoExW);
SPRT_FN(VerQueryValueA);
SPRT_FN(VerQueryValueW);
