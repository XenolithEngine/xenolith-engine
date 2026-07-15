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

/* VS_VERSIONINFO resource constants (verrsrc.h) */
#define __SPRT_VS_VERSION_INFO                 1
#define __SPRT_VS_USER_DEFINED                 100

#define __SPRT_VS_FFI_SIGNATURE                0xFEEF04BDL
#define __SPRT_VS_FFI_STRUCVERSION             0x00010000L
#define __SPRT_VS_FFI_FILEFLAGSMASK            0x0000003FL

#define __SPRT_VS_FF_DEBUG                     0x00000001L
#define __SPRT_VS_FF_PRERELEASE                0x00000002L
#define __SPRT_VS_FF_PATCHED                   0x00000004L
#define __SPRT_VS_FF_PRIVATEBUILD              0x00000008L
#define __SPRT_VS_FF_INFOINFERRED              0x00000010L
#define __SPRT_VS_FF_SPECIALBUILD              0x00000020L

#define __SPRT_VOS_UNKNOWN                     0x00000000L
#define __SPRT_VOS_DOS                         0x00010000L
#define __SPRT_VOS_OS216                       0x00020000L
#define __SPRT_VOS_OS232                       0x00030000L
#define __SPRT_VOS_NT                          0x00040000L
#define __SPRT_VOS_WINCE                       0x00050000L

#define __SPRT_VOS__BASE                       0x00000000L
#define __SPRT_VOS__WINDOWS16                  0x00000001L
#define __SPRT_VOS__PM16                       0x00000002L
#define __SPRT_VOS__PM32                       0x00000003L
#define __SPRT_VOS__WINDOWS32                  0x00000004L

#define __SPRT_VOS_DOS_WINDOWS16               0x00010001L
#define __SPRT_VOS_DOS_WINDOWS32               0x00010004L
#define __SPRT_VOS_OS216_PM16                  0x00020002L
#define __SPRT_VOS_OS232_PM32                  0x00030003L
#define __SPRT_VOS_NT_WINDOWS32                0x00040004L

#define __SPRT_VFT_UNKNOWN                     0x00000000L
#define __SPRT_VFT_APP                         0x00000001L
#define __SPRT_VFT_DLL                         0x00000002L
#define __SPRT_VFT_DRV                         0x00000003L
#define __SPRT_VFT_FONT                        0x00000004L
#define __SPRT_VFT_VXD                         0x00000005L
#define __SPRT_VFT_STATIC_LIB                  0x00000007L

#define __SPRT_VFT2_UNKNOWN                    0x00000000L
#define __SPRT_VFT2_DRV_PRINTER                0x00000001L
#define __SPRT_VFT2_DRV_KEYBOARD               0x00000002L
#define __SPRT_VFT2_DRV_LANGUAGE               0x00000003L
#define __SPRT_VFT2_DRV_DISPLAY                0x00000004L
#define __SPRT_VFT2_DRV_MOUSE                  0x00000005L
#define __SPRT_VFT2_DRV_NETWORK                0x00000006L
#define __SPRT_VFT2_DRV_SYSTEM                 0x00000007L
#define __SPRT_VFT2_DRV_INSTALLABLE            0x00000008L
#define __SPRT_VFT2_DRV_SOUND                  0x00000009L
#define __SPRT_VFT2_DRV_COMM                   0x0000000AL
#define __SPRT_VFT2_DRV_INPUTMETHOD            0x0000000BL
#define __SPRT_VFT2_DRV_VERSIONED_PRINTER      0x0000000CL

#define __SPRT_VFT2_FONT_RASTER                0x00000001L
#define __SPRT_VFT2_FONT_VECTOR                0x00000002L
#define __SPRT_VFT2_FONT_TRUETYPE              0x00000003L

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
