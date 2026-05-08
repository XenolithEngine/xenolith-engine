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

#include <sprt/wrappers/windows/message_api.h>

#include "dllloader.h"

extern "C" {

// ---- Message Formatting Function (kernelbase) ----

DWORD FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
		LPWSTR lpBuffer, DWORD nSize, __builtin_va_list *Arguments) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&FormatMessageW)>(loader->kernelbase.FormatMessageW,
			dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments);
}

DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
		LPSTR lpBuffer, DWORD nSize, __builtin_va_list *Arguments) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&FormatMessageA)>(loader->kernelbase.FormatMessageA,
			dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments);
}
// ---- Send/Post Message Functions (user32) ----

LRESULT SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SendMessageW)>(loader->user32.SendMessageW, hWnd, Msg,
			wParam, lParam);
}

BOOL PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&PostMessageW)>(loader->user32.PostMessageW, hWnd, Msg,
			wParam, lParam);
}

// ---- Quit Message Function (user32) ----

VOID PostQuitMessage(int nExitCode) {
	auto loader = sprt::DllLoader::get();
	loader->user32.call<decltype(&PostQuitMessage)>(loader->user32.PostQuitMessage, nExitCode);
}

// ---- Peek Message Function (user32) ----

BOOL PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&PeekMessageW)>(loader->user32.PeekMessageW, lpMsg, hWnd,
			wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

// ---- Message Translation Function (user32) ----

BOOL TranslateMessage(const MSG *lpMsg) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&TranslateMessage)>(loader->user32.TranslateMessage, lpMsg);
}

// ---- Message Dispatch Function (user32) ----

LRESULT DispatchMessageW(const MSG *lpMsg) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&DispatchMessageW)>(loader->user32.DispatchMessageW, lpMsg);
}

// ---- Get Message Time Function (user32) ----

LONG GetMessageTime(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetMessageTime)>(loader->user32.GetMessageTime);
}

// ---- MsgWaitForMultipleObjectsEx Function (user32) ----

DWORD MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE *pHandles, DWORD dwMilliseconds,
		DWORD dwWakeMask, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&MsgWaitForMultipleObjectsEx)>(
			loader->user32.MsgWaitForMultipleObjectsEx, nCount, pHandles, dwMilliseconds,
			dwWakeMask, dwFlags);
}

// ---- System Power Status Function (kernel32) ----

BOOL GetSystemPowerStatus(LPSYSTEM_POWER_STATUS lpSystemPowerStatus) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetSystemPowerStatus)>(
			loader->kernel32.GetSystemPowerStatus, lpSystemPowerStatus);
}

// ---- Current Theme Name Function (uxtheme) ----

HRESULT GetCurrentThemeName(LPWSTR pszThemeFileName, int cchMaxNameChars, LPWSTR pszColorBuff,
		int cchMaxColorChars, LPWSTR pszSizeBuff, int cchMaxSizeChars) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&GetCurrentThemeName)>(loader->uxtheme.GetCurrentThemeName,
			pszThemeFileName, cchMaxNameChars, pszColorBuff, cchMaxColorChars, pszSizeBuff,
			cchMaxSizeChars);
}

// ---- System Metrics Function (user32) ----

int GetSystemMetrics(int nIndex) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetSystemMetrics)>(loader->user32.GetSystemMetrics,
			nIndex);
}

// ---- System Parameters Info Function (user32) ----

BOOL SystemParametersInfoW(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SystemParametersInfoW)>(
			loader->user32.SystemParametersInfoW, uiAction, uiParam, pvParam, fWinIni);
}

// ---- Stock Object Function (gdi32) ----

HANDLE GetStockObject(int i) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetStockObject)>(loader->gdi32.GetStockObject, i);
}

// ---- Object Width/Height Query Function (gdi32) ----

int GetObjectW(HANDLE h, int c, LPVOID pv) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetObjectW)>(loader->gdi32.GetObjectW, h, c, pv);
}

} // extern "C"
