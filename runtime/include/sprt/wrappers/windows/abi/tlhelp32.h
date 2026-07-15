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

// Materialized ABI values for <tlhelp32.h> (clean names re-exported by
// sprt/wrappers/windows/tlhelp32.h).

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_TLHELP32_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_TLHELP32_H_

#include <sprt/wrappers/windows/abi/structures.h> // DWORD/BYTE/WCHAR/HMODULE
#include <sprt/wrappers/windows/abi/constants.h> // MAX_PATH

#define __SPRT_MAX_MODULE_NAME32 255

typedef struct tagMODULEENTRY32W {
	DWORD dwSize;
	DWORD th32ModuleID;
	DWORD th32ProcessID;
	DWORD GlblcntUsage;
	DWORD ProccntUsage;
	BYTE *modBaseAddr;
	DWORD modBaseSize;
	HMODULE hModule;
	WCHAR szModule[__SPRT_MAX_MODULE_NAME32 + 1];
	WCHAR szExePath[__SPRT_MAX_PATH];
} MODULEENTRY32W, *PMODULEENTRY32W, *LPMODULEENTRY32W;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_TLHELP32_H_
