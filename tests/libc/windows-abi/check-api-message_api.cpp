// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/message_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/message_api.h>
#include "api_check.h"

#include <windows.h>
#include <uxtheme.h>

SPRT_FN(FormatMessageW);
SPRT_FN(FormatMessageA);
SPRT_FN(GetUserPreferredUILanguages);
SPRT_FN(SendMessageW);
SPRT_FN(PostMessageW);
SPRT_FN(PostQuitMessage);
SPRT_FN(PeekMessageW);
SPRT_FN(TranslateMessage);
SPRT_FN(DispatchMessageW);
SPRT_FN(GetMessageTime);
SPRT_FN(MsgWaitForMultipleObjectsEx);
SPRT_FN(GetSystemPowerStatus);
SPRT_FN(GetCurrentThemeName);
SPRT_FN(GetSystemMetrics);
SPRT_FN(SystemParametersInfoW);
SPRT_FN(GetStockObject);
SPRT_FN(GetObjectW);
