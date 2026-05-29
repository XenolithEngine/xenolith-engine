/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/monitor_api.h>

#include "dllloader.h"

extern "C" {
LRESULT SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SendMessageW, hWnd, Msg, wParam, lParam);
}

BOOL PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, PostMessageW, hWnd, Msg, wParam, lParam);
}

VOID PostQuitMessage(int nExitCode) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, user32, PostQuitMessage, nExitCode);
}

BOOL PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, PeekMessageW, lpMsg, hWnd, wMsgFilterMin,
			wMsgFilterMax, wRemoveMsg);
}

BOOL TranslateMessage(const MSG *lpMsg) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, TranslateMessage, lpMsg);
}

LRESULT DispatchMessageW(const MSG *lpMsg) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, DispatchMessageW, lpMsg);
}

LONG GetMessageTime(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetMessageTime);
}

DWORD MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE *pHandles, DWORD dwMilliseconds,
		DWORD dwWakeMask, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, MsgWaitForMultipleObjectsEx, nCount, pHandles,
			dwMilliseconds, dwWakeMask, dwFlags);
}

int GetSystemMetrics(int nIndex) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetSystemMetrics, nIndex);
}

BOOL SystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SystemParametersInfoW, uiAction, uiParam, pvParam,
			fWinIni);
}

WINAPI BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO lpmi) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetMonitorInfoW, hMonitor, lpmi);
}

WINAPI BOOL EnumDisplayDevicesW(LPCWSTR lpDevice, DWORD iDevNum, PDISPLAY_DEVICEW lpDisplayDevice,
		DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, EnumDisplayDevicesW, lpDevice, iDevNum,
			lpDisplayDevice, dwFlags);
}

WINAPI BOOL EnumDisplayMonitors(HDC hdc, LPCRECT lprcClip, MONITORENUMPROC lpfnEnum,
		LPARAM dwData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, EnumDisplayMonitors, hdc, lprcClip, lpfnEnum, dwData);
}

WINAPI BOOL EnumDisplaySettingsW(LPCWSTR lpszDeviceName, DWORD iModeNum, DEVMODEW *lpDevMode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, EnumDisplaySettingsW, lpszDeviceName, iModeNum,
			lpDevMode);
}

WINAPI LONG ChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd,
		DWORD dwflags, LPVOID lParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, ChangeDisplaySettingsExW, lpszDeviceName, lpDevMode,
			hwnd, dwflags, lParam);
}

WINAPI BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetProcessDpiAwarenessContext, value);
}

WINAPI int GetSystemMetricsForDpi(int nIndex, UINT dpi) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetSystemMetricsForDpi, nIndex, dpi);
}

WINAPI BOOL CloseWindowStation(HWINSTA hWinSta) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, CloseWindowStation, hWinSta);
}

WINAPI BOOL SetProcessWindowStation(HWINSTA hWinSta) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetProcessWindowStation, hWinSta);
}

WINAPI HWINSTA GetProcessWindowStation(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetProcessWindowStation);
}

WINAPI BOOL GetUserObjectInformationW(HANDLE hObj, int nIndex, PVOID pvInfo, DWORD nLength,
		LPDWORD lpnLengthNeeded) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetUserObjectInformationW, hObj, nIndex, pvInfo,
			nLength, lpnLengthNeeded);
}

HANDLE LoadImageW(HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, LoadImageW, hInst, name, type, cx, cy, fuLoad);
}

HCURSOR LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, LoadCursorW, hInstance, lpCursorName);
}

SHORT GetKeyState(int nVirtKey) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetKeyState, nVirtKey);
}

HCURSOR SetCursor(HCURSOR hCursor) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetCursor, hCursor);
}

LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetWindowLongPtrW, hWnd, nIndex);
}

LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetWindowLongPtrW, hWnd, nIndex, dwNewLong);
}

LONG SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetWindowLongW, hWnd, nIndex, dwNewLong);
}

HWND CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
		int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
		LPVOID lpParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, CreateWindowExW, dwExStyle, lpClassName, lpWindowName,
			dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, ShowWindow, hWnd, nCmdShow);
}

BOOL SetForegroundWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetForegroundWindow, hWnd);
}

HWND SetActiveWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetActiveWindow, hWnd);
}

HWND SetFocus(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetFocus, hWnd);
}

BOOL EnableWindow(HWND hWnd, BOOL bEnable) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, EnableWindow, hWnd, bEnable);
}

BOOL IsWindowEnabled(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, IsWindowEnabled, hWnd);
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetWindowRect, hWnd, lpRect);
}

BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, AdjustWindowRect, lpRect, dwStyle, bMenu);
}

BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, AdjustWindowRectEx, lpRect, dwStyle, bMenu, dwExStyle);
}

LRESULT DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, DefWindowProcW, hWnd, Msg, wParam, lParam);
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetWindowPos, hWnd, hWndInsertAfter, X, Y, cx, cy,
			uFlags);
}

HANDLE MonitorFromWindow(HWND hwnd, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, MonitorFromWindow, hwnd, dwFlags);
}

UINT GetDpiForWindow(HWND hwnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetDpiForWindow, hwnd);
}

BOOL FlashWindow(HWND hWnd, BOOL bInvert) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, FlashWindow, hWnd, bInvert);
}

BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetWindowPlacement, hWnd, lpwndpl);
}

BOOL SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT *lpwndpl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetWindowPlacement, hWnd, lpwndpl);
}

BOOL DestroyWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, DestroyWindow, hWnd);
}

ATOM RegisterClassW(const WNDCLASSW *lpWndClass) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, RegisterClassW, lpWndClass);
}

BOOL UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, UnregisterClassW, lpClassName, hInstance);
}

HMENU GetSystemMenu(HWND hWnd, BOOL bRevert) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetSystemMenu, hWnd, bRevert);
}

BOOL SetMenuItemInfoW(HMENU hmenu, UINT item, BOOL fByPositon, LPCMENUITEMINFOW lpmii) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetMenuItemInfoW, hmenu, item, fByPositon, lpmii);
}

BOOL SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetMenuDefaultItem, hMenu, uItem, fByPos);
}

BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd,
		const RECT *prcRect) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, TrackPopupMenu, hMenu, uFlags, x, y, nReserved, hWnd,
			prcRect);
}

HWND GetCapture(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, GetCapture);
}

HWND SetCapture(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, SetCapture, hWnd);
}

BOOL ReleaseCapture(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, ReleaseCapture);
}

BOOL TrackMouseEvent(LPTRACKMOUSEEVENT lpEventTrack) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, TrackMouseEvent, lpEventTrack);
}

UINT MapVirtualKeyW(UINT uCode, UINT uMapType) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, MapVirtualKeyW, uCode, uMapType);
}

WINAPI int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, user32, MessageBoxW, hWnd, lpText, lpCaption, uType);
}
}
