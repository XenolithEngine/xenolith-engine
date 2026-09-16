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

// <tlhelp32.h> — Toolhelp process/thread/module snapshot API. The snapshot
// handle, PROCESSENTRY32W/THREADENTRY32 and CreateToolhelp32Snapshot/Process32*/
// Thread32* live in the windows umbrella; this header adds the module-walk half
// (MODULEENTRY32W + Module32First/NextW) and the neutral (UNICODE-mapped) names.

#ifndef SPRT_WRAPPERS_WINDOWS_TLHELP32_H_
#define SPRT_WRAPPERS_WINDOWS_TLHELP32_H_

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/abi/tlhelp32.h>

/* Clean public names (materialized __SPRT_ value / MODULEENTRY32W live in abi/tlhelp32.h) */
#ifndef MAX_MODULE_NAME32
#define MAX_MODULE_NAME32 __SPRT_MAX_MODULE_NAME32
#endif

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI BOOL Module32FirstW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);

__SPRT_WIN_IMPORT WINAPI BOOL Module32NextW(HANDLE hSnapshot, LPMODULEENTRY32W lpme);

__SPRT_END_DECL

// Neutral names resolve to the wide variants (this sysroot builds with UNICODE).
#define PROCESSENTRY32 PROCESSENTRY32W
#define LPPROCESSENTRY32 LPPROCESSENTRY32W
#define Process32First Process32FirstW
#define Process32Next Process32NextW
#define MODULEENTRY32 MODULEENTRY32W
#define LPMODULEENTRY32 LPMODULEENTRY32W
#define Module32First Module32FirstW
#define Module32Next Module32NextW

#endif // SPRT_WRAPPERS_WINDOWS_TLHELP32_H_
