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
HANDLE GetStockObject(int i) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, gdi32, GetStockObject, i);
}

int GetObjectW(HANDLE h, int c, LPVOID pv) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, gdi32, GetObjectW, h, c, pv);
}

WINAPI HDC CreateDCW(LPCWSTR pwszDriver, LPCWSTR pwszDevice, LPCWSTR pszPort, const DEVMODEW *pdm) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, gdi32, CreateDCW, pwszDriver, pwszDevice, pszPort, pdm);
}

WINAPI int GetDeviceCaps(HDC hdc, int index) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, gdi32, GetDeviceCaps, hdc, index);
}

WINAPI BOOL DeleteDC(HDC hdc) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, gdi32, DeleteDC, hdc);
}
}
