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

#include <sprt/wrappers/windows/user_api.h>

#include "dllloader.h"

extern "C" {

// ---- Shell Functions (shell32) ----

HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile, LPCSTR lpParameters,
		LPCSTR lpDirectory, INT nShowCmd) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&ShellExecuteA)>(loader->shell32.ShellExecuteA, hwnd,
			lpOperation, lpFile, lpParameters, lpDirectory, nShowCmd);
}

HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile, LPCWSTR lpParameters,
		LPCWSTR lpDirectory, INT nShowCmd) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&ShellExecuteW)>(loader->shell32.ShellExecuteW, hwnd,
			lpOperation, lpFile, lpParameters, lpDirectory, nShowCmd);
}

// ---- Image Loading Functions (gdi32/user32) ----

HANDLE LoadImageW(HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&LoadImageW)>(loader->user32.LoadImageW, hInst, name, type,
			cx, cy, fuLoad);
}

// ---- Cursor Functions (user32) ----

HCURSOR LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&LoadCursorW)>(loader->user32.LoadCursorW, hInstance,
			lpCursorName);
}

SHORT GetKeyState(int nVirtKey) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetKeyState)>(loader->user32.GetKeyState, nVirtKey);
}

HCURSOR SetCursor(HCURSOR hCursor) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetCursor)>(loader->user32.SetCursor, hCursor);
}

// ---- Window Procedure Functions (user32) ----

LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetWindowLongPtrW)>(loader->user32.GetWindowLongPtrW, hWnd,
			nIndex);
}

LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetWindowLongPtrW)>(loader->user32.SetWindowLongPtrW, hWnd,
			nIndex, dwNewLong);
}

LONG SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetWindowLongW)>(loader->user32.SetWindowLongW, hWnd,
			nIndex, dwNewLong);
}

// ---- Window Creation Functions (user32) ----

HWND CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
		int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
		LPVOID lpParam) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&CreateWindowExW)>(loader->user32.CreateWindowExW,
			dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu,
			hInstance, lpParam);
}

// ---- Window Display Functions (user32) ----

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&ShowWindow)>(loader->user32.ShowWindow, hWnd, nCmdShow);
}

BOOL SetForegroundWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetForegroundWindow)>(loader->user32.SetForegroundWindow,
			hWnd);
}

HWND SetActiveWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetActiveWindow)>(loader->user32.SetActiveWindow, hWnd);
}

HWND SetFocus(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetFocus)>(loader->user32.SetFocus, hWnd);
}

BOOL EnableWindow(HWND hWnd, BOOL bEnable) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&EnableWindow)>(loader->user32.EnableWindow, hWnd, bEnable);
}

BOOL IsWindowEnabled(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&IsWindowEnabled)>(loader->user32.IsWindowEnabled, hWnd);
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetWindowRect)>(loader->user32.GetWindowRect, hWnd,
			lpRect);
}

// ---- Rectangle Adjustment Functions (user32) ----

BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&AdjustWindowRect)>(loader->user32.AdjustWindowRect, lpRect,
			dwStyle, bMenu);
}

BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&AdjustWindowRectEx)>(loader->user32.AdjustWindowRectEx,
			lpRect, dwStyle, bMenu, dwExStyle);
}

// ---- Window Procedure Default Function (user32) ----

LRESULT DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&DefWindowProcW)>(loader->user32.DefWindowProcW, hWnd, Msg,
			wParam, lParam);
}

// ---- Window Position Functions (user32) ----

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetWindowPos)>(loader->user32.SetWindowPos, hWnd,
			hWndInsertAfter, X, Y, cx, cy, uFlags);
}

// ---- Monitor Functions (user32) ----

HANDLE MonitorFromWindow(HWND hwnd, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&MonitorFromWindow)>(loader->user32.MonitorFromWindow, hwnd,
			dwFlags);
}

UINT GetDpiForWindow(HWND hwnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetDpiForWindow)>(loader->user32.GetDpiForWindow, hwnd);
}

// ---- Window Flash Function (user32) ----

BOOL FlashWindow(HWND hWnd, BOOL bInvert) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&FlashWindow)>(loader->user32.FlashWindow, hWnd, bInvert);
}

// ---- Window Placement Functions (user32) ----

BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetWindowPlacement)>(loader->user32.GetWindowPlacement,
			hWnd, lpwndpl);
}

BOOL SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT *lpwndpl) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetWindowPlacement)>(loader->user32.SetWindowPlacement,
			hWnd, lpwndpl);
}

// ---- Window Destruction Function (user32) ----

BOOL DestroyWindow(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&DestroyWindow)>(loader->user32.DestroyWindow, hWnd);
}


// ---- Window Class Registration Functions (user32) ----

ATOM RegisterClassW(const WNDCLASSW *lpWndClass) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&RegisterClassW)>(loader->user32.RegisterClassW,
			lpWndClass);
}

BOOL UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&UnregisterClassW)>(loader->user32.UnregisterClassW,
			lpClassName, hInstance);
}

// ---- System Menu Functions (user32) ----

HMENU GetSystemMenu(HWND hWnd, BOOL bRevert) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetSystemMenu)>(loader->user32.GetSystemMenu, hWnd,
			bRevert);
}

// ---- Menu Functions (user32) ----

BOOL SetMenuItemInfoW(HMENU hmenu, UINT item, BOOL fByPositon, LPCMENUITEMINFOW lpmii) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetMenuItemInfoW)>(loader->user32.SetMenuItemInfoW, hmenu,
			item, fByPositon, lpmii);
}

BOOL SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetMenuDefaultItem)>(loader->user32.SetMenuDefaultItem,
			hMenu, uItem, fByPos);
}

BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd,
		const RECT *prcRect) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&TrackPopupMenu)>(loader->user32.TrackPopupMenu, hMenu,
			uFlags, x, y, nReserved, hWnd, prcRect);
}

// ---- DWM Functions (dwmapi) ----

HRESULT DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS *pMarInset) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&DwmExtendFrameIntoClientArea)>(
			loader->dwmapi.DwmExtendFrameIntoClientArea, hWnd, pMarInset);
}

// ---- Capture Functions (user32) ----

HWND GetCapture(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetCapture)>(loader->user32.GetCapture);
}

HWND SetCapture(HWND hWnd) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetCapture)>(loader->user32.SetCapture, hWnd);
}

BOOL ReleaseCapture(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&ReleaseCapture)>(loader->user32.ReleaseCapture);
}

// ---- Mouse Track Functions (user32) ----

BOOL TrackMouseEvent(LPTRACKMOUSEEVENT lpEventTrack) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&TrackMouseEvent)>(loader->user32.TrackMouseEvent,
			lpEventTrack);
}

// ---- Keyboard Mapping Functions (user32) ----

UINT MapVirtualKeyW(UINT uCode, UINT uMapType) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&MapVirtualKeyW)>(loader->user32.MapVirtualKeyW, uCode,
			uMapType);
}

WINAPI int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&MessageBoxW)>(loader->user32.MessageBoxW, hWnd, lpText,
			lpCaption, uType);
}

} // extern "C"
