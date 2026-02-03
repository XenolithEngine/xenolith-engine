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

/*
	Based on https://github.com/dlfcn-win32/dlfcn-win32 implementation

	Removed dirty hacks for outdated systems (we support only Windows 10 / 1903+)
	Implemented some Linux-specific flags, that can be supported on Windows (RTLD_NOLOAD, RTLD_NODELETE)
*/

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_dlfcn.h>
#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_stdio.h>

#include <sprt/runtime/stream.h>
#include <sprt/runtime/mem/set.h>

#include "private/SPRTFilename.h"

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

using Dl_info = __SPRT_ID(Dl_info);

struct DlError {
	static constexpr size_t BufferSize = 4'096;
	char buffer[BufferSize];
	bool errorIsSet = false;
};

static memory::dynset<HMODULE> s_locals;
static thread_local DlError tl_errorData;

/* Load Psapi.dll at runtime, this avoids linking caveat */
static BOOL __EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb,
		LPDWORD lpcbNeeded) {
	static BOOL(WINAPI * EnumProcessModulesPtr)(HANDLE, HMODULE *, DWORD, LPDWORD) = nullptr;
	static BOOL failed = FALSE;
	HMODULE kernel32;
	HMODULE psapi;

	if (failed) {
		return FALSE;
	}

	if (EnumProcessModulesPtr == nullptr) {
		/* Windows 7 and newer versions have K32EnumProcessModules in Kernel32.dll which is always pre-loaded */
		kernel32 = GetModuleHandleA("Kernel32.dll");
		if (kernel32 != nullptr) {
			EnumProcessModulesPtr = (BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD))(
					void (*)(void))GetProcAddress(kernel32, "K32EnumProcessModules");
		}

		/* Windows Vista and older version have EnumProcessModules in Psapi.dll which needs to be loaded */
		if (EnumProcessModulesPtr == nullptr) {
			psapi = LoadLibraryA("Psapi.dll");
			if (psapi != nullptr) {
				EnumProcessModulesPtr = (BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD))(
						void (*)(void))GetProcAddress(psapi, "EnumProcessModules");
				if (EnumProcessModulesPtr == nullptr) {
					FreeLibrary(psapi);
				}
			}
		}

		if (EnumProcessModulesPtr == nullptr) {
			failed = TRUE;
			return FALSE;
		}
	}

	return EnumProcessModulesPtr(hProcess, lphModule, cb, lpcbNeeded);
}

static int dlclose(void *ptr) {
	DWORD dwProcModsBefore = 0;
	DWORD dwProcModsAfter = 0;
	auto hCurrentProc = GetCurrentProcess();

	if (__EnumProcessModules(hCurrentProc, nullptr, 0, &dwProcModsBefore) == 0) {
		dwProcModsBefore = 0;
	}

	if (!FreeLibrary(HMODULE(ptr))) {
		StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize,
				"Fail to dlclose: ", status::lastErrorToStatus(GetLastError()));
		tl_errorData.errorIsSet = true;
		return -1;
	}

	if (__EnumProcessModules(hCurrentProc, nullptr, 0, &dwProcModsAfter) == 0) {
		dwProcModsAfter = 0;
	}

	if (dwProcModsBefore != dwProcModsAfter) {
		s_locals.erase(HMODULE(ptr));
	}

	return 0;
}

static char *dlerror(void) {
	if (tl_errorData.errorIsSet) {
		tl_errorData.errorIsSet = false;
		return tl_errorData.buffer;
	}
	return nullptr;
}

static void *dlopen(const char *path, int __flags) {
	DWORD dwProcModsBefore = 0;
	DWORD dwProcModsAfter = 0;
	auto hCurrentProc = GetCurrentProcess();

	if (__EnumProcessModules(hCurrentProc, nullptr, 0, &dwProcModsBefore) == 0) {
		dwProcModsBefore = 0;
	}

	HMODULE h = nullptr;
	if (!path) {
		// GetModuleHandleExA increments refcount and reuires FreeLibrary;
		if (!GetModuleHandleExA(0, nullptr, &h)) {
			StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize,
					"Fail to dlopen ", path, ": ", status::lastErrorToStatus(GetLastError()));
			tl_errorData.errorIsSet = true;
			return nullptr;
		}
	} else {
		h = internal::performWithNativePath(path, [&](const char *target) {
			if ((__flags & __SPRT_RTLD_NOLOAD) != 0) {
				HMODULE hMod = nullptr;
				DWORD flags = 0;
				if ((__flags & __SPRT_RTLD_NODELETE) != 0) {
					flags |= GET_MODULE_HANDLE_EX_FLAG_PIN;
				}
				GetModuleHandleExA(flags, target, &hMod);
				return hMod;
			} else {
				HMODULE hMod = LoadLibraryExA(target, nullptr, 0);

				if (hMod && (__flags & __SPRT_RTLD_NODELETE) != 0) {
					GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN
									| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							target, &hMod);
				}

				return hMod;
			}
		}, (HMODULE) nullptr);
	}

	if (!h) {
		StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize, "Fail to dlopen ",
				path, ": ", status::lastErrorToStatus(GetLastError()));
		tl_errorData.errorIsSet = true;
		return nullptr;
	}

	if (__EnumProcessModules(hCurrentProc, nullptr, 0, &dwProcModsAfter) == 0) {
		dwProcModsAfter = 0;
	}

	/* If the object was loaded with RTLD_LOCAL, add it to list of local
	* objects, so that its symbols cannot be retrieved even if the handle for
	* the original program file is passed. POSIX says that if the same
	* file is specified in multiple invocations, and any of them are
	* RTLD_GLOBAL, even if any further invocations use RTLD_LOCAL, the
	* symbols will remain global. If number of loaded modules was not
	* changed after calling LoadLibraryEx(), it means that library was
	* already loaded.
	*/
	if ((__flags & __SPRT_RTLD_GLOBAL) == 0 && dwProcModsBefore != dwProcModsAfter) {
		s_locals.emplace(h);
	} else if ((__flags & __SPRT_RTLD_GLOBAL) != 0 && dwProcModsBefore == dwProcModsAfter) {
		// erase module marked as global from local list
		s_locals.erase(h);
	}

	return h;
}

static void *dlsym(void *__SPRT_RESTRICT __handle, const char *__SPRT_RESTRICT __name) {
	HMODULE hCaller = nullptr;
	auto self = GetModuleHandleA(nullptr);

	if (__handle != __SPRT_RTLD_NEXT) {
		if (__handle == __SPRT_RTLD_DEFAULT) {
			__handle = self;
		}

		// Do not validate inputs: let UCRT to do error processing
		auto s = GetProcAddress(HMODULE(__handle), __name);
		if (s) {
			return *(void **)(&s);
		} else if (self != __handle) {
			StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize,
					"Fail to dlsym ", __name, ": ", status::lastErrorToStatus(GetLastError()));
			tl_errorData.errorIsSet = true;
			return nullptr;
		}
	} else {
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
							| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCSTR)__builtin_extract_return_addr(__builtin_return_address(0)), &hCaller)) {
			StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize,
					"Fail to dlsym ", __name, ": ", status::lastErrorToStatus(GetLastError()));
			tl_errorData.errorIsSet = true;
			return nullptr;
		}
	}

	// enumerate all modules

	DWORD dwSize = 0;
	DWORD cbNeeded = 0;
	FARPROC symbol = nullptr;
	auto hCurrentProc = GetCurrentProcess();

	/* GetModuleHandleA( nullptr ) only returns the current program file. So
         * if we want to get ALL loaded module including those in linked DLLs,
         * we have to use EnumProcessModules( ).
         */
	if (__EnumProcessModules(hCurrentProc, nullptr, 0, &dwSize) != 0) {
		auto modules = (HMODULE *)LocalAlloc(LPTR, dwSize);
		if (!modules) {
			StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize,
					"Fail to dlsym ", __name, ": ",
					status::lastErrorToStatus(ERROR_NOT_ENOUGH_MEMORY));
			tl_errorData.errorIsSet = true;
			return nullptr;
		}

		if (__EnumProcessModules(hCurrentProc, modules, dwSize, &cbNeeded) != 0
				&& dwSize == cbNeeded) {
			for (size_t i = 0; i < dwSize / sizeof(HMODULE); i++) {
				if (__handle == __SPRT_RTLD_NEXT && hCaller) {
					/* Next modules can be used for RTLD_NEXT */
					if (hCaller == modules[i]) {
						hCaller = nullptr;
					}
					continue;
				}
				if (s_locals.find(modules[i]) != s_locals.end()) {
					continue;
				}

				symbol = GetProcAddress(modules[i], __name);
				if (symbol != nullptr) {
					break;
				}
			}
		}
		LocalFree(modules);
	}

	if (symbol == nullptr) {
		StreamTraits<char>::toStringBuf(tl_errorData.buffer, DlError::BufferSize, "Fail to dlsym ",
				__name, ": ", status::lastErrorToStatus(ERROR_PROC_NOT_FOUND));
		tl_errorData.errorIsSet = true;
		return nullptr;
	}

	return *(void **)(&symbol);
}


/* See https://docs.microsoft.com/en-us/archive/msdn-magazine/2002/march/inside-windows-an-in-depth-look-into-the-win32-portable-executable-file-format-part-2
 * for details */

/* Get specific image section */
static BOOL get_image_section(HMODULE module, int index, void **ptr, DWORD *size) {
	IMAGE_DOS_HEADER *dosHeader;
	IMAGE_NT_HEADERS *ntHeaders;
	IMAGE_OPTIONAL_HEADER *optionalHeader;

	dosHeader = (IMAGE_DOS_HEADER *)module;

	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		return FALSE;
	}

	ntHeaders = (IMAGE_NT_HEADERS *)((BYTE *)dosHeader + dosHeader->e_lfanew);

	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
		return FALSE;
	}

	optionalHeader = &ntHeaders->OptionalHeader;

	if (optionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) {
		return FALSE;
	}

	if (index < 0 || index >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES
			|| (DWORD)index >= optionalHeader->NumberOfRvaAndSizes) {
		return FALSE;
	}

	if (optionalHeader->DataDirectory[index].Size == 0
			|| optionalHeader->DataDirectory[index].VirtualAddress == 0) {
		return FALSE;
	}

	if (size != nullptr) {
		*size = optionalHeader->DataDirectory[index].Size;
	}

	*ptr = (void *)((BYTE *)module + optionalHeader->DataDirectory[index].VirtualAddress);

	return TRUE;
}

/* Return symbol name for a given address from export table */
static const char *get_export_symbol_name(HMODULE module, IMAGE_EXPORT_DIRECTORY *ied,
		const void *addr, void **func_address) {
	DWORD i;
	void *candidateAddr = nullptr;
	int candidateIndex = -1;
	BYTE *base = (BYTE *)module;
	DWORD *functionAddressesOffsets = (DWORD *)(base + (DWORD)ied->AddressOfFunctions);
	DWORD *functionNamesOffsets = (DWORD *)(base + (DWORD)ied->AddressOfNames);
	USHORT *functionNameOrdinalsIndexes = (USHORT *)(base + (DWORD)ied->AddressOfNameOrdinals);

	for (i = 0; i < ied->NumberOfFunctions; i++) {
		if ((void *)(base + functionAddressesOffsets[i]) > addr
				|| candidateAddr >= (void *)(base + functionAddressesOffsets[i])) {
			continue;
		}

		candidateAddr = (void *)(base + functionAddressesOffsets[i]);
		candidateIndex = i;
	}

	if (candidateIndex == -1) {
		return nullptr;
	}

	*func_address = candidateAddr;

	for (i = 0; i < ied->NumberOfNames; i++) {
		if (functionNameOrdinalsIndexes[i] == candidateIndex) {
			return (const char *)(base + functionNamesOffsets[i]);
		}
	}

	return nullptr;
}

static BOOL is_valid_address(const void *addr) {
	MEMORY_BASIC_INFORMATION info;
	size_t result;

	if (addr == nullptr) {
		return FALSE;
	}

	/* check valid pointer */
	result = VirtualQuery(addr, &info, sizeof(info));

	if (result != sizeof(info) || info.AllocationBase == nullptr || info.State == MEM_FREE
			|| info.State == MEM_RESERVE || info.Protect == 0 || info.Protect == PAGE_NOACCESS) {
		return FALSE;
	}

	return TRUE;
}

#if defined(_M_ARM64) || defined(__aarch64__)
static INT64 sign_extend(UINT64 value, UINT bits) {
	const UINT left = 64 - bits;
	const INT64 m1 = -1;
	const INT64 wide = (INT64)(value << left);
	const INT64 sign = (wide < 0) ? (m1 << left) : 0;

	return value | sign;
}
#endif

/* Return state if address points to an import thunk
 *
 * On x86, an import thunk is setup with a 'jmp' instruction followed by an
 * absolute address (32bit) or relative offset (64bit) pointing into
 * the import address table (iat), which is partially maintained by
 * the runtime linker.
 *
 * On ARM64, an import thunk is also a relative jump pointing into the
 * import address table, implemented by the following three instructions:
 * 
 *      adrp x16, [page_offset]
 * Calculates the page address (aligned to 4KB) the IAT is at, based 
 * on the value of x16, with page_offset. 
 *
 *      ldr  x16, [x16, offset]
 * Calculates the final IAT address, x16 <- x16 + offset.
 * 
 *      br   x16
 * Jump to the address in x16.
 * 
 * The register used here is hardcoded to be x16.
 */
static BOOL is_import_thunk(const void *addr) {
#if defined(_M_ARM64) || defined(__aarch64__)
	ULONG opCode1 = *(ULONG *)((BYTE *)addr);
	ULONG opCode2 = *(ULONG *)((BYTE *)addr + 4);
	ULONG opCode3 = *(ULONG *)((BYTE *)addr + 8);

	return (opCode1 & 0x9f00'001f) == 0x9000'0010 /* adrp x16, [page_offset] */
					&& (opCode2 & 0xffe0'03ff) == 0xf940'0210 /* ldr  x16, [x16, offset] */
					&& opCode3 == 0xd61f'0200 /* br   x16 */
			? TRUE
			: FALSE;
#elif defined(_M_AMD64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	return *(USHORT *)addr == 0x25ff ? TRUE : FALSE;
#else
	return FALSE;
#endif
}

/* Return adress from the import address table (iat),
 * if the original address points to a thunk table entry.
 */
static void *get_address_from_import_address_table(void *iat, DWORD iat_size, const void *addr) {
	BYTE *thkp = (BYTE *)addr;
#if defined(_M_ARM64) || defined(__aarch64__)
	/*
     *  typical import thunk in ARM64:
     *  0x7ff772ae78c0 <+25760>: adrp   x16, 1
     *  0x7ff772ae78c4 <+25764>: ldr    x16, [x16, #0xdc0]
     *  0x7ff772ae78c8 <+25768>: br     x16
     */
	ULONG opCode1 = *(ULONG *)((BYTE *)addr);
	ULONG opCode2 = *(ULONG *)((BYTE *)addr + 4);

	/* Extract the offset from adrp instruction */
	UINT64 pageLow2 = (opCode1 >> 29) & 3;
	UINT64 pageHigh19 = (opCode1 >> 5) & ~(~0ull << 19);
	INT64 page = sign_extend((pageHigh19 << 2) | pageLow2, 21) << 12;

	/* Extract the offset from ldr instruction */
	UINT64 offset = ((opCode2 >> 10) & ~(~0ull << 12)) << 3;

	/* Calculate the final address */
	BYTE *ptr = (BYTE *)((ULONG64)thkp & ~0xfffull) + page + offset;
#elif defined(_M_AMD64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	/* Get offset from thunk table (after instruction 0xff 0x25)
     *   4018c8 <_VirtualQuery>: ff 25 4a 8a 00 00
     */
	ULONG offset = *(ULONG *)(thkp + 2);
#if defined(_M_AMD64) || defined(__x86_64__)
	/* On 64 bit the offset is relative
     *      4018c8:   ff 25 4a 8a 00 00    jmpq    *0x8a4a(%rip)    # 40a318 <__imp_VirtualQuery>
     * And can be also negative (MSVC in WDK)
     *   100002f20:   ff 25 3a e1 ff ff    jmpq   *-0x1ec6(%rip)    # 0x100001060
     * So cast to signed LONG type
     */
	BYTE *ptr = (BYTE *)(thkp + 6 + (LONG)offset);
#else
	/* On 32 bit the offset is absolute
     *   4019b4:    ff 25 90 71 40 00    jmp    *0x40719
     */
	BYTE *ptr = (BYTE *)offset;
#endif
#else
	return nullptr;
#endif

	if (!is_valid_address(ptr) || ptr < (BYTE *)iat || ptr > (BYTE *)iat + iat_size) {
		return nullptr;
	}

	return *(void **)ptr;
}

static int dladdr(const void *__handle, __SPRT_ID(Dl_info) * __info) {
	if (__info == nullptr) {
		return 0;
	}

	if (!is_valid_address(__handle)) {
		return 0;
	}

	if (is_import_thunk(__handle)) {
		void *iat;
		DWORD iatSize;
		HMODULE hModule;

		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
							| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCSTR)__handle, &hModule)) {
			return 0;
		}

		if (!get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_IAT, &iat, &iatSize)) {
			/* Fallback for cases where the iat is not defined,
             * for example i586-mingw32msvc-gcc */
			IMAGE_IMPORT_DESCRIPTOR *iid;
			DWORD iidSize;

			if (!get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_IMPORT, (void **)&iid,
						&iidSize)) {
				return 0;
			}

			if (iid == nullptr || iid->Characteristics == 0 || iid->FirstThunk == 0) {
				return 0;
			}

			iat = (void *)((BYTE *)hModule + (DWORD)iid->FirstThunk);
			/* We assume that in this case iid and iat's are in linear order */
			iatSize = iidSize - (DWORD)((BYTE *)iat - (BYTE *)iid);
		}

		__handle = get_address_from_import_address_table(iat, iatSize, __handle);

		if (!is_valid_address(__handle)) {
			return 0;
		}
	}

	static thread_local char tl_moduleName[1'024];

	HMODULE hModule;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
						| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)__handle, &hModule)) {
		return 0;
	}

	auto dwSize = GetModuleFileNameA(hModule, tl_moduleName, sizeof(tl_moduleName));
	if (dwSize == 0 || dwSize == sizeof(tl_moduleName)) {
		return 0;
	}

	IMAGE_EXPORT_DIRECTORY *ied;
	void *funcAddress = nullptr;

	__info->dli_fname = tl_moduleName;
	__info->dli_fbase = (void *)hModule;

	/* Find function name and function address in module's export table */
	if (get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_EXPORT, (void **)&ied, nullptr)) {
		__info->dli_sname = get_export_symbol_name(hModule, ied, __handle, &funcAddress);
	} else {
		__info->dli_sname = nullptr;
	}

	if (__info->dli_sname == nullptr) {
		__info->dli_saddr = nullptr;
	} else {
		__info->dli_saddr = funcAddress != nullptr ? funcAddress : (void *)__handle;
	}
	return 1;
}

} // namespace sprt

#ifdef __clang__
#pragma clang diagnostic pop
#endif
