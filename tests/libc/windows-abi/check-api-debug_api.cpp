// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/debug_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/debug_api.h>
#include "api_check.h"

#include <windows.h>
#include <dbghelp.h>

SPRT_FN(SymSetOptions);
SPRT_FN(SymInitialize);
SPRT_FN(SymGetOptions);
SPRT_FN(SymCleanup);
SPRT_FN(StackWalk64);
SPRT_FN(SymGetSymFromAddr64);
SPRT_FN(SymGetLineFromAddr64);
SPRT_FN(UnDecorateSymbolName);
