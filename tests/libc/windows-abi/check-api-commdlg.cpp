// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/commdlg.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/commdlg.h>
#include "api_check.h"

#include <windows.h>
#include <commdlg.h>

SPRT_FN(ChooseColorW);
SPRT_FN(ChooseFontW);
SPRT_FN(CommDlgExtendedError);
