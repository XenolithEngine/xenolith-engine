/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_SHELLAPI_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_SHELLAPI_H_

#include <sprt/wrappers/windows/abi/basic_types.h>
#include <sprt/wrappers/windows/abi/monitor_api.h> // HWND

// clang-format off
#define __SPRT_FOF_SILENT 0x0004
#define __SPRT_FOF_NOCONFIRMATION 0x0010
#define __SPRT_FOF_NOCONFIRMMKDIR 0x0200
#define __SPRT_FOF_NOERRORUI 0x0400
#define __SPRT_FOF_NO_UI (__SPRT_FOF_SILENT | __SPRT_FOF_NOCONFIRMATION \
	| __SPRT_FOF_NOERRORUI | __SPRT_FOF_NOCONFIRMMKDIR)

// ALLOWUNDO is what makes a delete land in the Recycle Bin instead of being permanent.
// RECYCLEONDELETE says the same thing to Windows 8 and newer, which stopped honouring ALLOWUNDO
// on its own for some item types; EARLYFAILURE makes IFileOperation report a problem up front
// rather than after it has already started.
#define __SPRT_FOF_ALLOWUNDO 0x0040
#define __SPRT_FOFX_RECYCLEONDELETE 0x00080000
#define __SPRT_FOFX_EARLYFAILURE 0x00100000

#define __SPRT_FOFX_NOCOPYHOOKS 0x00800000

// SHFILEOPSTRUCTW::wFunc
#define __SPRT_FO_MOVE   0x0001
#define __SPRT_FO_COPY   0x0002
#define __SPRT_FO_DELETE 0x0003
#define __SPRT_FO_RENAME 0x0004
	// clang-format on

// Note the width: the SDK spells this WORD, not DWORD, even though the FOF_/FOFX_ table above runs
// past 16 bits — the extended bits are reachable only through IFileOperation::SetOperationFlags,
// which takes a DWORD. Getting this wrong silently shifts every field after it.
typedef WORD FILEOP_FLAGS;

// The SDK wraps <shellapi.h> in pshpack1.h, but only `#if !defined(_WIN64)` — so this struct is
// byte-packed on 32-bit and naturally aligned on 64-bit. Mirroring that condition exactly is not
// optional: pack it unconditionally and every field from pFrom onward lands four bytes early.
// tests/libc/windows-abi/check-shellapi.cpp pins the offsets.
#if __SIZEOF_POINTER__ == 4
#pragma pack(push, 1)
#endif
typedef struct __SPRT_SHFILEOPSTRUCTW {
	HWND hwnd;
	UINT wFunc;
	PCZZWSTR pFrom; // double-NUL-terminated list of paths, not a plain string
	PCZZWSTR pTo;
	FILEOP_FLAGS fFlags;
	BOOL fAnyOperationsAborted;
	LPVOID hNameMappings;
	PCWSTR lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;
#if __SIZEOF_POINTER__ == 4
#pragma pack(pop)
#endif

#endif
