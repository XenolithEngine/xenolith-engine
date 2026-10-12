// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/tlhelp32.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/tlhelp32.h>
#include "api_check.h"

#include <windows.h>
#include <tlhelp32.h>

SPRT_FN(Module32FirstW);
SPRT_FN(Module32NextW);
