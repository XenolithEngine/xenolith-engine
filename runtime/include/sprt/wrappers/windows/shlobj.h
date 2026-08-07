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

// <shlobj.h>: the Known Folder API subset used by llvm's Path.inc (home / local
// app-data resolution). The FOLDERID_* GUIDs match the SDK.

#ifndef SPRT_WRAPPERS_WINDOWS_SHLOBJ_H_
#define SPRT_WRAPPERS_WINDOWS_SHLOBJ_H_

#include <sprt/wrappers/windows/abi/structures.h> // GUID
#include <sprt/wrappers/windows/abi/com_api.h>
#include <sprt/wrappers/windows/abi/shlobj.h> // SIGDN / FILEOPENDIALOGOPTIONS / COMDLG_FILTERSPEC
#include <sprt/wrappers/windows/basic_api.h> // HANDLE / DWORD / HRESULT
#include <sprt/wrappers/windows/com_api.h> // CoInitializeEx / CoUninitialize / CoCreateInstance

#ifdef __cplusplus
#include <sprt/wrappers/windows/com_cxx.hpp>
#endif

typedef GUID KNOWNFOLDERID;

#define REFKNOWNFOLDERID const KNOWNFOLDERID &

// clang-format off
// {5E6C858F-0E22-4760-9AFE-EA3317B67173}
static const KNOWNFOLDERID FOLDERID_Profile = {
	0x5E6C858F, 0x0E22, 0x4760, {0x9A, 0xFE, 0xEA, 0x33, 0x17, 0xB6, 0x71, 0x73}};
// {F1B32785-6FBA-4FCF-9D55-7B8E7F157091}
static const KNOWNFOLDERID FOLDERID_LocalAppData = {
	0xF1B32785, 0x6FBA, 0x4FCF, {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91}};
// clang-format on

__SPRT_WIN_IMPORT WINAPI HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags,
		HANDLE hToken, LPWSTR *ppszPath);

#ifdef __cplusplus
extern "C" {
__SPRT_WIN_IMPORT WINAPI PIDLIST_ABSOLUTE ILCreateFromPathW(PCWSTR pszPath);
__SPRT_WIN_IMPORT WINAPI void ILFree(PIDLIST_ABSOLUTE pidl);
__SPRT_WIN_IMPORT WINAPI HRESULT SHCreateItemFromIDList(PCIDLIST_ABSOLUTE pidl, REFIID riid,
		void **ppv);

// Wraps a path as an IShellItem. Unlike SHCreateItemFromIDList this does not require the item to
// exist, which is what makes it usable for a save dialog's target directory.
__SPRT_WIN_IMPORT WINAPI HRESULT SHCreateItemFromParsingName(PCWSTR pszPath, void *pbc, REFIID riid,
		void **ppv);

// Opens the containing folder with the given items selected — "reveal in Explorer". With cidl 0
// and apidl null it opens `pidlFolder` itself.
__SPRT_WIN_IMPORT WINAPI HRESULT SHOpenFolderAndSelectItems(PCIDLIST_ABSOLUTE pidlFolder, UINT cidl,
		PCUITEMID_CHILD_ARRAY apidl, DWORD dwFlags);
}
#endif

#endif // SPRT_WRAPPERS_WINDOWS_SHLOBJ_H_
