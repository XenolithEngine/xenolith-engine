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

#include "dllloader.h"
#include "../initterm.h"

#include <sprt/c/__sprt_string.h>

#include <sprt/cxx/type_traits>
#include <sprt/cxx/new>

namespace sprt {

/*
	Loader
*/

extern "C" unsigned __int64 __readgsqword(unsigned long);

// clang-format off
static BYTE s_TolowerTable[] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
	48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,
	96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};
// clang-format on

SAFELOADER inline wchar_t __tolower(wchar_t c) {
	if (c > 256) {
		return c;
	} else {
		return (wchar_t)(s_TolowerTable[BYTE(c)]);
	}
}

SAFELOADER int __wcsicmp(const wchar_t *l, const wchar_t *r) {
	for (; __tolower(*l) == __tolower(*r) && *l && *r; l++, r++);
	return __tolower(*l) < __tolower(*r) ? -1 : __tolower(*l) > __tolower(*r);
}

SAFELOADER size_t __wcslen(const wchar_t *s) {
	const wchar_t *a;
	for (a = s; *s; s++);
	return s - a;
}

PPEB GetPEB(void) { return (PPEB)__readgsqword(0x60); }

SAFELOADER HMODULE GetKernel32Module(void) {
	wchar_t Kernel32Name[] = L"kernel32.dll";
	int nameLen = __wcslen(Kernel32Name);

	PPEB peb = GetPEB();
	if (!peb || !peb->Ldr) {
		return 0;
	}

	PEB_LDR_DATA *ldr = peb->Ldr;
	PLDR_DATA_TABLE_ENTRY module = (PLDR_DATA_TABLE_ENTRY)ldr->InLoadOrderModuleList.Flink;

	while (module) {
		if (module->FullDllName.Length >= nameLen
				&& __wcsicmp(module->FullDllName.Buffer
								   + module->FullDllName.Length / sizeof(wchar_t) - nameLen,
						   Kernel32Name)
						== 0) {
			return module->DllBase;
		}

		module = (PLDR_DATA_TABLE_ENTRY)module->InLoadOrderLinks.Flink;
		if (module == (PLDR_DATA_TABLE_ENTRY)ldr->InLoadOrderModuleList.Flink) {
			return 0;
		}
	}
	return 0;
}

SAFELOADER int CompareStrings(const char *l, const char *r) {
	while (*l && *r && *l == *r) {
		l++;
		r++;
	}
	return *(const BYTE *)l - *(const BYTE *)r;
}

SAFELOADER int FindExportNameIndex(HMODULE hModule, PIMAGE_EXPORT_DIRECTORY exportDir,
		const char *functionName) {
	// Get the name pointer table (array of RVAs to name strings)
	DWORD *nameTable = (DWORD *)PE_GET_OFFSET(hModule, exportDir->AddressOfNames);

	// TODO - carefully rewrite this to binary search
	for (int i = 0; i < exportDir->NumberOfNames; i++) {
		const char *name = (const char *)PE_GET_OFFSET(hModule, nameTable[i]);
		if (CompareStrings(name, functionName) == 0) {
			return i;
		}
	}

	return -1; // Not found
}

SAFELOADER __funcptr LookupFunctionInModule(HMODULE hModule, const char *functionName) {
	// 1. Get DOS header to find PE offset
	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
	if (dosHeader->e_magic != 0x5A4D) { // Not "MZ"
		return 0;
	}

	PIMAGE_NT_HEADERS64 peHeader = (PIMAGE_NT_HEADERS64)PE_GET_OFFSET(hModule, dosHeader->e_lfanew);

	if (peHeader->Signature != 0x0000'4550
			|| peHeader->OptionalHeader.Magic != 0x20B) { // Not "PE\0\0" or not PE32+
		return 0;
	}

	// 3. Get export directory from data directories
	IMAGE_DATA_DIRECTORY *dataDirs =
			&peHeader->OptionalHeader
					 .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]; // Index 0 = Export

	if (dataDirs->VirtualAddress == 0 || dataDirs->Size == 0) {
		return 0; // No export table
	}

	// 4. Parse export directory
	PIMAGE_EXPORT_DIRECTORY exportDir =
			(PIMAGE_EXPORT_DIRECTORY)PE_GET_OFFSET(hModule, dataDirs->VirtualAddress);

	if (!exportDir || !exportDir->NumberOfNames) {
		return 0;
	}

	int nameIndex = FindExportNameIndex(hModule, exportDir, functionName);
	if (nameIndex == -1) {
		return 0; // Function not found
	}

	WORD *ordinalTable = (WORD *)PE_GET_OFFSET(hModule, exportDir->AddressOfNameOrdinals);
	WORD ordinal = ordinalTable[nameIndex];

	DWORD *funcTable = (DWORD *)PE_GET_OFFSET(hModule, exportDir->AddressOfFunctions);
	DWORD funcRVA = funcTable[ordinal /*- exportDir->Base*/]; // Subtract base offset

	return (__funcptr)PE_GET_OFFSET(hModule, funcRVA);
}

// uninitialized buffer for DllLoader
// DllLoader should be initialized BEFORE static initializers is called
static unsigned char s_loaderBuffer[sizeof(DllLoader)];

__declspec(safebuffers) DllLoader *DllLoader::construct() {
	return new (s_loaderBuffer, sprt::nothrow) DllLoader;
}

__declspec(safebuffers) DllLoader *DllLoader::get() {
	return reinterpret_cast<DllLoader *>(s_loaderBuffer);
}

__declspec(safebuffers) int DllLoader::load() {
	// Note that this is not reference-counted handle, use only for initial loading;
	// For actual usage, use LoadLibraryW("kernel32.dll")
	auto _rootKernel32 = GetKernel32Module();
	if (!_rootKernel32) {
		return LOADER_ERROR_NO_KERNEL32;
	}

	auto _GetProcAddress = reinterpret_cast<decltype(&::GetProcAddress)>(
			LookupFunctionInModule(_rootKernel32, "GetProcAddress"));
	if (!_GetProcAddress) {
		return LOADER_ERROR_NO_GETPROCADDRESS; // We unable to load functions, aborting
	}

	auto _LoadLibraryW = reinterpret_cast<decltype(&::LoadLibraryW)>(
			_GetProcAddress(_rootKernel32, "LoadLibraryW"));

	auto _FreeLibrary = reinterpret_cast<decltype(&::FreeLibrary)>(
			_GetProcAddress(_rootKernel32, "FreeLibrary"));

	auto _ExitProcess = reinterpret_cast<decltype(&::ExitProcess)>(
			_GetProcAddress(_rootKernel32, "ExitProcess"));

	if (!_LoadLibraryW || !_FreeLibrary || !_ExitProcess) {
		return LOADER_ERROR_NO_LOAD_FREE_LIBRARY; // We unable to modules, aborting
	}

	__GetProcAddress = _GetProcAddress;
	__LoadLibraryW = _LoadLibraryW;
	__FreeLibrary = _FreeLibrary;
	__ExitProcess = _ExitProcess;

	int ret = 0;
	for (auto &it : __tables) {
		if (it == nullptr) {
			break;
		}

		auto lib = __LoadLibraryW(it->__name);
		if (!lib) {
			ret = LOADER_ERROR_NO_BASE_DLLS;
			break;
		}

		if (!it->init(lib)) {
			ret = LOADER_ERROR_FAIL_TO_PRELOAD;
			break;
		}
	}

	if (ret != 0) {
		unload();
	}

	return ret;
}

__declspec(safebuffers) int DllLoader::unload() {
	for (auto &it : __tables) {
		if (it == nullptr) {
			break;
		}

		if (it->__handle) {
			__FreeLibrary(it->__handle);
			it->__handle = nullptr;
		}
	}
	return 0;
}


DllTable::DllTable(const wchar_t *n, DllTableRecord *start, DllTableRecord *end,
		DllTableRecord *preloads)
: __name(n), __start(start), __end(end), __preloads(preloads) { }

bool DllTable::init(HANDLE h) {
	auto loader = reinterpret_cast<DllLoader *>(s_loaderBuffer);

	__handle = h;

	DllTableRecord *rec = nullptr;
	if (__preloads) {
		rec = __preloads;
		while (rec != __end && rec->name) {
			rec->fn = loader->__GetProcAddress(__handle, rec->name);
			if (!rec->fn) {
				return false;
			}
			++rec;
		}
	}

	if (__debug) {
		// try to preload all symbols

		rec = __start + 1;
		while (rec != __end && rec->name) {
			rec->fn = loader->__GetProcAddress(__handle, rec->name);
			++rec;
		}
	}

	return true;
}

bool DllTable::load(DllTableRecord *rec) {
	auto loader = reinterpret_cast<DllLoader *>(s_loaderBuffer);

	if (!rec->fn) {
		rec->fn = loader->__GetProcAddress(__handle, rec->name);
	}

	if (rec->fn) {
		return true;
	}
	return false;
}

} // namespace sprt
