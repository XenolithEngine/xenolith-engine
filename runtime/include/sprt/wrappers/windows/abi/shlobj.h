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

// ABI half of <ShObjIdl_core.h>: the file-dialog value tables and the one struct that crosses the
// boundary. Pinned against the SDK by tests/libc/windows-abi/check-shlobj.cpp.
//
// SIGDN and FILEOPENDIALOGOPTIONS are SDK enums, so they are enums here too and are checked with
// SPRT_ENUM rather than SPRT_CONST.

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_SHLOBJ_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_SHLOBJ_H_

#include <sprt/wrappers/windows/abi/basic_types.h>

// clang-format off

// IShellItem::GetDisplayName forms. FILESYSPATH is the only one that yields a real filesystem path,
// and it fails for items that have none (a virtual folder such as "This PC").
typedef enum __SPRT_SIGDN {
	SIGDN_NORMALDISPLAY                = 0x00000000,
	SIGDN_PARENTRELATIVEPARSING        = (int)0x80018001,
	SIGDN_DESKTOPABSOLUTEPARSING       = (int)0x80028000,
	SIGDN_PARENTRELATIVEEDITING        = (int)0x80031001,
	SIGDN_DESKTOPABSOLUTEEDITING       = (int)0x8004c000,
	SIGDN_FILESYSPATH                  = (int)0x80058000,
	SIGDN_URL                          = (int)0x80068000,
	SIGDN_PARENTRELATIVEFORADDRESSBAR  = (int)0x8007c001,
	SIGDN_PARENTRELATIVE               = (int)0x80080001,
	SIGDN_PARENTRELATIVEFORUI          = (int)0x80094001
} SIGDN;

// IFileDialog::SetOptions / GetOptions.
typedef enum __SPRT_FILEOPENDIALOGOPTIONS {
	FOS_OVERWRITEPROMPT          = 0x2,
	FOS_STRICTFILETYPES          = 0x4,
	FOS_NOCHANGEDIR              = 0x8,
	FOS_PICKFOLDERS              = 0x20,
	FOS_FORCEFILESYSTEM          = 0x40,
	FOS_ALLNONSTORAGEITEMS       = 0x80,
	FOS_NOVALIDATE               = 0x100,
	FOS_ALLOWMULTISELECT         = 0x200,
	FOS_PATHMUSTEXIST            = 0x800,
	FOS_FILEMUSTEXIST            = 0x1000,
	FOS_CREATEPROMPT             = 0x2000,
	FOS_SHAREAWARE               = 0x4000,
	FOS_NOREADONLYRETURN         = 0x8000,
	FOS_NOTESTFILECREATE         = 0x10000,
	FOS_HIDEMRUPLACES            = 0x20000,
	FOS_HIDEPINNEDPLACES         = 0x40000,
	FOS_NODEREFERENCELINKS       = 0x100000,
	FOS_OKBUTTONNEEDSINTERACTION = 0x200000,
	FOS_DONTADDTORECENT          = 0x2000000,
	FOS_FORCESHOWHIDDEN          = 0x10000000,
	FOS_DEFAULTNOMINIMODE        = 0x20000000,
	FOS_FORCEPREVIEWPANEON       = 0x40000000,
	FOS_SUPPORTSTREAMABLEITEMS   = (int)0x80000000
} FILEOPENDIALOGOPTIONS;

// clang-format on

// One entry of the file-type dropdown. Both members are caller-owned strings; pszSpec is a
// semicolon-separated glob list ("*.png;*.jpg").
typedef struct __SPRT_COMDLG_FILTERSPEC {
	LPCWSTR pszName;
	LPCWSTR pszSpec;
} COMDLG_FILTERSPEC;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_SHLOBJ_H_
