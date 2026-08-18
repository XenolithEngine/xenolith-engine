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
#include <sprt/wrappers/windows/abi/structures.h> // GUID
#include <sprt/wrappers/windows/abi/monitor_api.h> // HWND

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

/* One property of the shell's property system: a format GUID plus an index inside it.

Passed BY ADDRESS to IShellItem2::GetString / GetFileTime and returned by
IShellFolder2::MapColumnToSCID, so the layout is ABI and not merely a convenience. The SDK spells
it in <wtypes.h>; it is here because abi/ is where a layout the shell writes into belongs. */
typedef struct __SPRT_PROPERTYKEY {
	GUID fmtid;
	DWORD pid;
} PROPERTYKEY;

typedef const PROPERTYKEY &REFPROPERTYKEY;

// clang-format off

// IContextMenu::QueryContextMenu flags. NORMAL is what a plain right-click asks for, and it is the
// one an automated invoke wants too: the shell populates the same verb set the user would see.
// CMF_INCLUDESTATIC is absent on purpose: the SDK defines it only for NTDDI_VERSION < VISTA, and a
// name this table carries that the SDK does not is a promise nothing keeps.
#define __SPRT_CMF_NORMAL            0x00000000
#define __SPRT_CMF_DEFAULTONLY       0x00000001
#define __SPRT_CMF_VERBSONLY         0x00000002
#define __SPRT_CMF_EXPLORE           0x00000004
#define __SPRT_CMF_NOVERBS           0x00000008
#define __SPRT_CMF_CANRENAME         0x00000010
#define __SPRT_CMF_NODEFAULT         0x00000020
#define __SPRT_CMF_ITEMMENU          0x00000080
#define __SPRT_CMF_EXTENDEDVERBS     0x00000100
#define __SPRT_CMF_DISABLEDVERBS     0x00000200
#define __SPRT_CMF_ASYNCVERBSTATE    0x00000400
#define __SPRT_CMF_OPTIMIZEFORINVOKE 0x00000800
#define __SPRT_CMF_SYNCCASCADEMENU   0x00001000
#define __SPRT_CMF_DONOTPICKDEFAULT  0x00002000

/* CMINVOKECOMMANDINFO::fMask. These are the SEE_MASK_* values from <shellapi.h> under a second
name - the SDK defines them by aliasing, and so does this table, which is why they are pinned
against the SEE_MASK_ spelling rather than against a CMIC_ one the SDK does not define on its own.

FLAG_NO_UI is what turns an invoke into something a background thread may do: without it the shell
is free to put up its own error box, on a thread that owns no window. */
// CMIC_MASK_ICON is absent for the same reason as CMF_INCLUDESTATIC: its SEE_MASK_ICON is pre-Vista
// only, and the SDK's own comment on it reads "not used".
#define __SPRT_CMIC_MASK_HOTKEY      0x00000020
#define __SPRT_CMIC_MASK_FLAG_NO_UI  0x00000400
#define __SPRT_CMIC_MASK_UNICODE     0x00004000
#define __SPRT_CMIC_MASK_NO_CONSOLE  0x00008000
#define __SPRT_CMIC_MASK_ASYNCOK     0x00100000
#define __SPRT_CMIC_MASK_NOASYNC     0x00000100

// IContextMenu::GetCommandString types. VERBW is what identifies a command language-independently:
// the menu text is localized, the canonical verb ("undelete") is not.
#define __SPRT_GCS_VERBA     0x00000000
#define __SPRT_GCS_HELPTEXTA 0x00000001
#define __SPRT_GCS_VALIDATEA 0x00000002
#define __SPRT_GCS_VERBW     0x00000004
#define __SPRT_GCS_HELPTEXTW 0x00000005
#define __SPRT_GCS_VALIDATEW 0x00000006
#define __SPRT_GCS_UNICODE   0x00000004

// clang-format on

/* Argument of IContextMenu::InvokeCommand.

`lpVerb` is deliberately ANSI even in a UTF-16 world: it is either a canonical verb (a 7-bit
identifier such as "undelete") or MAKEINTRESOURCE of a menu id, and the union of the two is what
the field has always been. The wide form travels in CMINVOKECOMMANDINFOEX, which this runtime does
not need.

`cbSize` is the version tag - the shell dispatches on it, so a struct whose size does not match the
SDK's is not merely mislaid, it is misread. tests/libc/windows-abi/check-shlobj.cpp pins it. */
typedef struct __SPRT_CMINVOKECOMMANDINFO {
	DWORD cbSize;
	DWORD fMask;
	HWND hwnd;
	LPCSTR lpVerb;
	LPCSTR lpParameters;
	LPCSTR lpDirectory;
	int nShow;
	DWORD dwHotKey;
	HANDLE hIcon;
} CMINVOKECOMMANDINFO;

typedef CMINVOKECOMMANDINFO *LPCMINVOKECOMMANDINFO;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_SHLOBJ_H_
