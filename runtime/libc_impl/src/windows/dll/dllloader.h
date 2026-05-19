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

#ifndef RUNTIME_FREESTANDING_SRC_WINDOWS_DLLLOADER_H_
#define RUNTIME_FREESTANDING_SRC_WINDOWS_DLLLOADER_H_

#include <sprt/wrappers/windows/dl_api.h>

#include "dll_advapi32.h"
#include "dll_kernel32.h"
#include "dll_kernelbase.h"
#include "dll_ntdll.h"
#include "dll_setupapi.h"
#include "dll_shell32.h"
#include "dll_user32.h"
#include "dll_combase.h"
#include "dll_uxtheme.h"
#include "dll_gdi32.h"
#include "dll_dxva2.h"
#include "dll_shcore.h"
#include "dll_dwmapi.h"
#include "dll_userenv.h"
#include "dll_bcrypt.h"
#include "dll_ws2_32.h"
#include "dll_oleaut32.h"
#include "dll_dbghelp.h"

// Mark all loader functions with this to remove buffer checks from functions,
// that can be called before security cookie initialization
#define SAFELOADER __declspec(safebuffers) static

#define PE_GET_OFFSET(module, offset) ((BYTE*)(module) + (offset))

// Loader can do error reporting by exit codes in [1-255]
static constexpr int LOADER_ERROR_NO_KERNEL32 = 254;
static constexpr int LOADER_ERROR_NO_GETPROCADDRESS = 253;
static constexpr int LOADER_ERROR_NO_LOAD_FREE_LIBRARY = 252;
static constexpr int LOADER_ERROR_NO_NTDLL = 251;
static constexpr int LOADER_ERROR_NO_BASE_DLLS = 250;
static constexpr int LOADER_ERROR_NO_MEMSET = 249;
static constexpr int LOADER_ERROR_FAIL_TO_PRELOAD = 248;
static constexpr int LOADER_ERROR_STATIC_C_INIT_FAILED = 247;
static constexpr int LOADER_ERROR_STATIC_CXX_INIT_FAILED = 246;

namespace sprt {

struct DllLoader {
	static DllLoader *construct();
	static DllLoader *get();

	decltype(&::GetProcAddress) __GetProcAddress = nullptr;
	decltype(&::LoadLibraryW) __LoadLibraryW = nullptr;
	decltype(&::FreeLibrary) __FreeLibrary = nullptr;
	decltype(&::ExitProcess) __ExitProcess = nullptr;

	NtdllTable ntdll; // 0
	KernelBaseTable kernelbase; // 1
	Kernel32Table kernel32; // 2
	Advapi32Table advapi32; // 3
	Shell32Table shell32; // 4
	User32Table user32; // 5
	SetupapiTable setupapi; // 6
	CombaseTable combase; // 7
	UxthemeTable uxtheme; // 8
	Gdi32Table gdi32; // 9
	Dxva2Table dxva2; // 10
	ShcoreTable shcore; // 11
	DwmapiTable dwmapi; // 12
	UserenvTable userenv; // 13
	BCryptTable bcrypt; // 14
	Ws2_32Table ws2_32; // 15
	Oleaut32Table oleaut32; // 16
	DbghelpTable dbghelp; // 17

	DllTable *__tables[19];

	DllLoader()
	: __tables{
		  &ntdll,
		  &kernelbase,
		  &kernel32,
		  &advapi32,
		  &shell32,
		  &user32,
		  &setupapi,
		  &combase,
		  &uxtheme,
		  &gdi32,
		  &dxva2,
		  &shcore,
		  &dwmapi,
		  &userenv,
		  &bcrypt,
		  &ws2_32,
		  &oleaut32,
		  &dbghelp,
		  nullptr,
	  } { }

	int load();
	int unload();
};

} // namespace sprt

#endif // RUNTIME_FREESTANDING_SRC_WINDOWS_DLLLOADER_H_
