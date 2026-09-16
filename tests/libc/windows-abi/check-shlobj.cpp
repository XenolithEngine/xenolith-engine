// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/shlobj.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// These feed the system-dialog backend (runtime/window/windows): SIGDN selects which spelling
// IShellItem::GetDisplayName hands back, FILEOPENDIALOGOPTIONS is what turns IFileDialog into a
// folder picker or a multi-select, and COMDLG_FILTERSPEC is passed to SetFileTypes by address.
// All three go straight to the shell with no translation, so every value and offset has to match.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/shlobj.h>
#include "abi_check.h"

#include <windows.h>
#include <ShObjIdl_core.h> // SIGDN, _FILEOPENDIALOGOPTIONS, COMDLG_FILTERSPEC, IContextMenu
#include <shellapi.h> // SEE_MASK_*, which is how the SDK spells the CMIC_MASK_ values

// === SIGDN ==================================================================
SPRT_ENUM(SIGDN_NORMALDISPLAY);
SPRT_ENUM(SIGDN_PARENTRELATIVEPARSING);
SPRT_ENUM(SIGDN_DESKTOPABSOLUTEPARSING);
SPRT_ENUM(SIGDN_PARENTRELATIVEEDITING);
SPRT_ENUM(SIGDN_DESKTOPABSOLUTEEDITING);
SPRT_ENUM(SIGDN_FILESYSPATH);
SPRT_ENUM(SIGDN_URL);
SPRT_ENUM(SIGDN_PARENTRELATIVEFORADDRESSBAR);
SPRT_ENUM(SIGDN_PARENTRELATIVE);
SPRT_ENUM(SIGDN_PARENTRELATIVEFORUI);

// === FILEOPENDIALOGOPTIONS =================================================
SPRT_ENUM(FOS_OVERWRITEPROMPT);
SPRT_ENUM(FOS_STRICTFILETYPES);
SPRT_ENUM(FOS_NOCHANGEDIR);
SPRT_ENUM(FOS_PICKFOLDERS);
SPRT_ENUM(FOS_FORCEFILESYSTEM);
SPRT_ENUM(FOS_ALLNONSTORAGEITEMS);
SPRT_ENUM(FOS_NOVALIDATE);
SPRT_ENUM(FOS_ALLOWMULTISELECT);
SPRT_ENUM(FOS_PATHMUSTEXIST);
SPRT_ENUM(FOS_FILEMUSTEXIST);
SPRT_ENUM(FOS_CREATEPROMPT);
SPRT_ENUM(FOS_SHAREAWARE);
SPRT_ENUM(FOS_NOREADONLYRETURN);
SPRT_ENUM(FOS_NOTESTFILECREATE);
SPRT_ENUM(FOS_HIDEMRUPLACES);
SPRT_ENUM(FOS_HIDEPINNEDPLACES);
SPRT_ENUM(FOS_NODEREFERENCELINKS);
SPRT_ENUM(FOS_OKBUTTONNEEDSINTERACTION);
SPRT_ENUM(FOS_DONTADDTORECENT);
SPRT_ENUM(FOS_FORCESHOWHIDDEN);
SPRT_ENUM(FOS_DEFAULTNOMINIMODE);
SPRT_ENUM(FOS_FORCEPREVIEWPANEON);
SPRT_ENUM(FOS_SUPPORTSTREAMABLEITEMS);

// === COMDLG_FILTERSPEC =====================================================
// Handed to IFileDialog::SetFileTypes as an array, so the stride matters as much as the fields.
SPRT_SIZE(COMDLG_FILTERSPEC);
SPRT_OFFSET(COMDLG_FILTERSPEC, pszName);
SPRT_OFFSET(COMDLG_FILTERSPEC, pszSpec);

// === PROPERTYKEY ============================================================
// Passed by address to IShellItem2::GetString / GetFileTime, so the shell reads both fields out of
// the caller's memory. The SDK spells it in <wtypes.h>.
SPRT_SIZE(PROPERTYKEY);
SPRT_OFFSET(PROPERTYKEY, fmtid);
SPRT_OFFSET(PROPERTYKEY, pid);

// === IContextMenu::QueryContextMenu flags ===================================
// CMF_INCLUDESTATIC and CMIC_MASK_ICON are not here, and not in the abi table either: the SDK
// defines both only for NTDDI_VERSION < VISTA, so on this target they do not exist to be pinned
// against. That absence is the check.
SPRT_CONST(CMF_NORMAL);
SPRT_CONST(CMF_DEFAULTONLY);
SPRT_CONST(CMF_VERBSONLY);
SPRT_CONST(CMF_EXPLORE);
SPRT_CONST(CMF_NOVERBS);
SPRT_CONST(CMF_CANRENAME);
SPRT_CONST(CMF_NODEFAULT);
SPRT_CONST(CMF_ITEMMENU);
SPRT_CONST(CMF_EXTENDEDVERBS);
SPRT_CONST(CMF_DISABLEDVERBS);
SPRT_CONST(CMF_ASYNCVERBSTATE);
SPRT_CONST(CMF_OPTIMIZEFORINVOKE);
SPRT_CONST(CMF_SYNCCASCADEMENU);
SPRT_CONST(CMF_DONOTPICKDEFAULT);

// === CMINVOKECOMMANDINFO::fMask =============================================
// The SDK defines every CMIC_MASK_ as an alias of a SEE_MASK_ from <shellapi.h>, and two of them
// only under a high enough NTDDI_VERSION - so they are pinned against the SEE_MASK_ spelling,
// which is unconditional and is what the value actually is.
SPRT_CONST_MAP(CMIC_MASK_HOTKEY, SEE_MASK_HOTKEY);
SPRT_CONST_MAP(CMIC_MASK_FLAG_NO_UI, SEE_MASK_FLAG_NO_UI);
SPRT_CONST_MAP(CMIC_MASK_UNICODE, SEE_MASK_UNICODE);
SPRT_CONST_MAP(CMIC_MASK_NO_CONSOLE, SEE_MASK_NO_CONSOLE);
SPRT_CONST_MAP(CMIC_MASK_ASYNCOK, SEE_MASK_ASYNCOK);
SPRT_CONST_MAP(CMIC_MASK_NOASYNC, SEE_MASK_NOASYNC);

// === IContextMenu::GetCommandString types ===================================
SPRT_CONST(GCS_VERBA);
SPRT_CONST(GCS_HELPTEXTA);
SPRT_CONST(GCS_VALIDATEA);
SPRT_CONST(GCS_VERBW);
SPRT_CONST(GCS_HELPTEXTW);
SPRT_CONST(GCS_VALIDATEW);
SPRT_CONST(GCS_UNICODE);

// === CMINVOKECOMMANDINFO ====================================================
// The shell dispatches on cbSize, so a struct whose size does not match is not merely mislaid, it
// is misread. The SDK puts it under `#include <pshpack8.h>`, which changes nothing on either
// architecture here - and that is exactly what these asserts are for.
SPRT_SIZE(CMINVOKECOMMANDINFO);
SPRT_OFFSET(CMINVOKECOMMANDINFO, cbSize);
SPRT_OFFSET(CMINVOKECOMMANDINFO, fMask);
SPRT_OFFSET(CMINVOKECOMMANDINFO, hwnd);
SPRT_OFFSET(CMINVOKECOMMANDINFO, lpVerb);
SPRT_OFFSET(CMINVOKECOMMANDINFO, lpParameters);
SPRT_OFFSET(CMINVOKECOMMANDINFO, lpDirectory);
SPRT_OFFSET(CMINVOKECOMMANDINFO, nShow);
SPRT_OFFSET(CMINVOKECOMMANDINFO, dwHotKey);
SPRT_OFFSET(CMINVOKECOMMANDINFO, hIcon);
