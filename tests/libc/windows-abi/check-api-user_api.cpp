// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/user_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/user_api.h>
#include "api_check.h"

#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>

SPRT_FN(ShellExecuteA);
SPRT_FN(ShellExecuteW);
SPRT_FN(LoadImageW);
SPRT_FN(LoadCursorW);
SPRT_FN(GetKeyState);
SPRT_FN(GetWindowLongPtrW);
SPRT_FN(SetWindowLongPtrW);
SPRT_FN(CreateWindowExW);
SPRT_FN(ShowWindow);
SPRT_FN(SetForegroundWindow);
SPRT_FN(SetActiveWindow);
SPRT_FN(SetFocus);
SPRT_FN(EnableWindow);
SPRT_FN(IsWindowEnabled);
SPRT_FN(GetWindowRect);
SPRT_FN(GetClientRect);
SPRT_FN(ClientToScreen);
SPRT_FN(ScreenToClient);
SPRT_FN(SetWindowLongW);
SPRT_FN(AdjustWindowRect);
SPRT_FN(AdjustWindowRectEx);
SPRT_FN(DefWindowProcW);
SPRT_FN(SetWindowPos);
SPRT_FN(MonitorFromWindow);
SPRT_FN(GetDpiForWindow);
SPRT_FN(FlashWindow);
SPRT_FN(GetWindowPlacement);
SPRT_FN(SetWindowPlacement);
SPRT_FN(DestroyWindow);
SPRT_FN(SetCursor);
SPRT_FN(RegisterClassW);
SPRT_FN(OpenClipboard);
SPRT_FN(CloseClipboard);
SPRT_FN(EmptyClipboard);
SPRT_FN(SetClipboardData);
SPRT_FN(GetClipboardData);
SPRT_FN(EnumClipboardFormats);
SPRT_FN(CountClipboardFormats);
SPRT_FN(IsClipboardFormatAvailable);
SPRT_FN(RegisterClipboardFormatW);
SPRT_FN(GetClipboardFormatNameW);
SPRT_FN(GetClipboardOwner);
SPRT_FN(AddClipboardFormatListener);
SPRT_FN(RemoveClipboardFormatListener);
SPRT_FN(GetClipboardSequenceNumber);
SPRT_FN(UnregisterClassW);
SPRT_FN(GetSystemMenu);
SPRT_FN(SetMenuItemInfoW);
SPRT_FN(SetMenuDefaultItem);
SPRT_FN(TrackPopupMenu);
SPRT_FN(CreatePopupMenu);
SPRT_FN(DestroyMenu);
SPRT_FN(DwmExtendFrameIntoClientArea);
SPRT_FN(GetCapture);
SPRT_FN(SetCapture);
SPRT_FN(ReleaseCapture);
SPRT_FN(TrackMouseEvent);
SPRT_FN(MapVirtualKeyW);
SPRT_FN(MessageBoxW);
SPRT_FN(CreateBitmap);
SPRT_FN(DeleteObject);
SPRT_FN(CreateIconIndirect);
SPRT_FN(DestroyIcon);
