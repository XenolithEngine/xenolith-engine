// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/dbghelp.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/dbghelp.h>
#include "api_check.h"

#include <windows.h>
#include <dbghelp.h>

SPRT_FN(MiniDumpWriteDump);
