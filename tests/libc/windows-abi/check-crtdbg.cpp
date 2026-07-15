// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/crtdbg.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// The runtime's <crtdbg.h> is a no-op report-hook shim, but its report-type and
// mode constants should still match the UCRT so callers (llvm's Signals.inc)
// that pass e.g. _CRT_ERROR / _CRTDBG_MODE_DEBUG see the same numbers.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/crtdbg.h>
#include "abi_check.h"

#include <windows.h>
#include <crtdbg.h>

// === report types ==========================================================
SPRT_CONST(_CRT_WARN);
SPRT_CONST(_CRT_ERROR);
SPRT_CONST(_CRT_ASSERT);
SPRT_CONST(_CRT_ERRCNT);

// === _CrtSetReportMode destinations ========================================
SPRT_CONST(_CRTDBG_MODE_FILE);
SPRT_CONST(_CRTDBG_MODE_DEBUG);
SPRT_CONST(_CRTDBG_MODE_WNDW);
SPRT_CONST(_CRTDBG_REPORT_MODE);

// === omitted ===============================================================
// _CRTDBG_HFILE_ERROR / _CRTDBG_FILE_STDOUT / _CRTDBG_FILE_STDERR are pointer
// sentinels (((_HFILE)-N)); an integer-to-pointer cast is not a constant
// expression, so they cannot be pinned with static_assert. Their integer bias
// (-2/-4/-5) is mirrored verbatim from the SDK in abi/crtdbg.h.
