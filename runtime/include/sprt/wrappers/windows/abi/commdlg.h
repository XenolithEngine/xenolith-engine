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

// ABI half of <commdlg.h>: the CHOOSECOLORW / CHOOSEFONTW layouts and their flag tables.
//
// Both structs are passed to comdlg32 by address with lStructSize set from sizeof, so their size
// and every field offset are load-bearing — pinned against the SDK by
// tests/libc/windows-abi/check-commdlg.cpp.

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_COMMDLG_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_COMMDLG_H_

#include <sprt/wrappers/windows/abi/basic_types.h>
#include <sprt/wrappers/windows/abi/monitor_api.h> // HWND / HDC
#include <sprt/wrappers/windows/abi/message_api.h> // LOGFONTW

// A COLORREF is 0x00bbggrr packed into a DWORD.
typedef DWORD COLORREF;

// The two hook procedures. Neither dialog is customised here, so the fields only have to be the
// right size — a function pointer, which on every Windows ABI is the size of a data pointer.
typedef void *LPCCHOOKPROC;
typedef void *LPCFHOOKPROC;

// clang-format off

// ---- ChooseColorW --------------------------------------------------------

#define __SPRT_CC_RGBINIT              0x00000001
#define __SPRT_CC_FULLOPEN             0x00000002
#define __SPRT_CC_PREVENTFULLOPEN      0x00000004
#define __SPRT_CC_SHOWHELP             0x00000008
#define __SPRT_CC_ENABLEHOOK           0x00000010
#define __SPRT_CC_ENABLETEMPLATE       0x00000020
#define __SPRT_CC_ENABLETEMPLATEHANDLE 0x00000040
#define __SPRT_CC_SOLIDCOLOR           0x00000080
#define __SPRT_CC_ANYCOLOR             0x00000100

// ---- ChooseFontW ---------------------------------------------------------

#define __SPRT_CF_SCREENFONTS          0x00000001
#define __SPRT_CF_PRINTERFONTS         0x00000002
#define __SPRT_CF_BOTH                 (__SPRT_CF_SCREENFONTS | __SPRT_CF_PRINTERFONTS)
#define __SPRT_CF_SHOWHELP             0x00000004
#define __SPRT_CF_ENABLEHOOK           0x00000008
#define __SPRT_CF_ENABLETEMPLATE       0x00000010
#define __SPRT_CF_ENABLETEMPLATEHANDLE 0x00000020
#define __SPRT_CF_INITTOLOGFONTSTRUCT  0x00000040
#define __SPRT_CF_USESTYLE             0x00000080
#define __SPRT_CF_EFFECTS              0x00000100
#define __SPRT_CF_APPLY                0x00000200
#define __SPRT_CF_ANSIONLY             0x00000400
#define __SPRT_CF_NOVECTORFONTS        0x00000800
#define __SPRT_CF_NOSIMULATIONS        0x00001000
#define __SPRT_CF_LIMITSIZE            0x00002000
#define __SPRT_CF_FIXEDPITCHONLY       0x00004000
#define __SPRT_CF_WYSIWYG              0x00008000
#define __SPRT_CF_FORCEFONTEXIST       0x00010000
#define __SPRT_CF_SCALABLEONLY         0x00020000
#define __SPRT_CF_TTONLY               0x00040000
#define __SPRT_CF_NOFACESEL            0x00080000
#define __SPRT_CF_NOSTYLESEL           0x00100000
#define __SPRT_CF_NOSIZESEL            0x00200000
#define __SPRT_CF_SELECTSCRIPT         0x00400000
#define __SPRT_CF_NOSCRIPTSEL          0x00800000
#define __SPRT_CF_NOVERTFONTS          0x01000000
#define __SPRT_CF_INACTIVEFONTS        0x02000000

// LOGFONTW::lfWeight values worth naming.
#define __SPRT_FW_NORMAL 400
#define __SPRT_FW_BOLD   700

// clang-format on

// rgbResult is a COLORREF: 0x00bbggrr, so the byte order is the reverse of HTML.
typedef struct __SPRT_tagCHOOSECOLORW {
	DWORD lStructSize;
	HWND hwndOwner;
	HWND hInstance;
	COLORREF rgbResult;
	COLORREF *lpCustColors; // must point at 16 writable COLORREFs
	DWORD Flags;
	LPARAM lCustData;
	LPCCHOOKPROC lpfnHook;
	LPCWSTR lpTemplateName;
} CHOOSECOLORW, *LPCHOOSECOLORW;

// iPointSize is in tenths of a point; lpLogFont carries the result and, with
// CF_INITTOLOGFONTSTRUCT, the initial value too.
typedef struct __SPRT_tagCHOOSEFONTW {
	DWORD lStructSize;
	HWND hwndOwner;
	HDC hDC;
	LOGFONTW *lpLogFont;
	INT iPointSize;
	DWORD Flags;
	COLORREF rgbColors;
	LPARAM lCustData;
	LPCFHOOKPROC lpfnHook;
	LPCWSTR lpTemplateName;
	HINSTANCE hInstance;
	LPWSTR lpszStyle;
	WORD nFontType;
	WORD ___MISSING_ALIGNMENT__;
	INT nSizeMin;
	INT nSizeMax;
} CHOOSEFONTW, *LPCHOOSEFONTW;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_COMMDLG_H_
