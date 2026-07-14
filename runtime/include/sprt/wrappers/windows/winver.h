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

#ifndef SPRT_WRAPPERS_WINDOWS_WINVER_H_
#define SPRT_WRAPPERS_WINDOWS_WINVER_H_

#include <sprt/wrappers/windows/message_api.h>
#include <sprt/wrappers/windows/abi/winver.h>

/* Clean public names (materialized __SPRT_ values live in abi/winver.h) */
#define VER_EQUAL __SPRT_VER_EQUAL
#define VER_GREATER __SPRT_VER_GREATER
#define VER_GREATER_EQUAL __SPRT_VER_GREATER_EQUAL
#define VER_LESS __SPRT_VER_LESS
#define VER_LESS_EQUAL __SPRT_VER_LESS_EQUAL
#define VER_AND __SPRT_VER_AND
#define VER_OR __SPRT_VER_OR
#define VER_CONDITION_MASK __SPRT_VER_CONDITION_MASK
#define VER_NUM_BITS_PER_CONDITION_MASK __SPRT_VER_NUM_BITS_PER_CONDITION_MASK
#define VER_MINORVERSION __SPRT_VER_MINORVERSION
#define VER_MAJORVERSION __SPRT_VER_MAJORVERSION
#define VER_BUILDNUMBER __SPRT_VER_BUILDNUMBER
#define VER_PLATFORMID __SPRT_VER_PLATFORMID
#define VER_SERVICEPACKMINOR __SPRT_VER_SERVICEPACKMINOR
#define VER_SERVICEPACKMAJOR __SPRT_VER_SERVICEPACKMAJOR
#define VER_SUITENAME __SPRT_VER_SUITENAME
#define VER_PRODUCT_TYPE __SPRT_VER_PRODUCT_TYPE

#include <sprt/wrappers/windows/winapifamily.h>

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation);

__SPRT_WIN_IMPORT WINAPI BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);

__SPRT_WIN_IMPORT WINAPI ULONGLONG VerSetConditionMask(ULONGLONG ConditionMask, DWORD TypeMask,
		BYTE Condition);

__SPRT_WIN_IMPORT WINAPI BOOL VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation,
		DWORD dwTypeMask, DWORDLONG dwlConditionMask);

#ifdef UNICODE
#define VerifyVersionInfo VerifyVersionInfoW
typedef OSVERSIONINFOEXW OSVERSIONINFOEX;
typedef POSVERSIONINFOEXW POSVERSIONINFOEX;
typedef LPOSVERSIONINFOEXW LPOSVERSIONINFOEX;
#endif

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINVER_H_
