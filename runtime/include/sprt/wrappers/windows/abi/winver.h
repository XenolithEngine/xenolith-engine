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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_WINVER_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_WINVER_H_


#include <sprt/wrappers/windows/abi/message_api.h>

// clang-format off

#define __SPRT_VER_EQUAL                       1
#define __SPRT_VER_GREATER                     2
#define __SPRT_VER_GREATER_EQUAL               3
#define __SPRT_VER_LESS                        4
#define __SPRT_VER_LESS_EQUAL                  5
#define __SPRT_VER_AND                         6
#define __SPRT_VER_OR                          7
#define __SPRT_VER_CONDITION_MASK              7
#define __SPRT_VER_NUM_BITS_PER_CONDITION_MASK 3

#define __SPRT_VER_MINORVERSION                0x0000001
#define __SPRT_VER_MAJORVERSION                0x0000002
#define __SPRT_VER_BUILDNUMBER                 0x0000004
#define __SPRT_VER_PLATFORMID                  0x0000008
#define __SPRT_VER_SERVICEPACKMINOR            0x0000010
#define __SPRT_VER_SERVICEPACKMAJOR            0x0000020
#define __SPRT_VER_SUITENAME                   0x0000040
#define __SPRT_VER_PRODUCT_TYPE                0x0000080

// clang-format on

typedef struct _OSVERSIONINFOEXW {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128]; // Maintenance string for PSS usage
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wSuiteMask;
	BYTE wProductType;
	BYTE wReserved;
} OSVERSIONINFOEXW, *POSVERSIONINFOEXW, *LPOSVERSIONINFOEXW, RTL_OSVERSIONINFOEXW,
		*PRTL_OSVERSIONINFOEXW;

typedef struct _OSVERSIONINFOW {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
} OSVERSIONINFOW, *POSVERSIONINFOW, *LPOSVERSIONINFOW, RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;

typedef struct _OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_WINVER_H_
