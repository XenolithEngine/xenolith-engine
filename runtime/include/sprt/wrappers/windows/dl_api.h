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

#ifndef SPRT_WRAPPERS_WINDOWS_DL_API_H_
#define SPRT_WRAPPERS_WINDOWS_DL_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

#define IMAGE_DOS_SIGNATURE                 0x5A4D      // MZ
#define IMAGE_OS2_SIGNATURE                 0x454E      // NE
#define IMAGE_OS2_SIGNATURE_LE              0x454C      // LE
#define IMAGE_VXD_SIGNATURE                 0x454C      // LE
#define IMAGE_NT_SIGNATURE                  0x0000'4550  // PE00

#define IMAGE_NT_OPTIONAL_HDR64_MAGIC      0x20b

#define MEM_COMMIT                      0x0000'1000
#define MEM_RESERVE                     0x0000'2000
#define MEM_REPLACE_PLACEHOLDER         0x0000'4000
#define MEM_RESERVE_PLACEHOLDER         0x0004'0000
#define MEM_RESET                       0x0008'0000
#define MEM_TOP_DOWN                    0x0010'0000
#define MEM_WRITE_WATCH                 0x0020'0000
#define MEM_PHYSICAL                    0x0040'0000
#define MEM_ROTATE                      0x0080'0000
#define MEM_DIFFERENT_IMAGE_BASE_OK     0x0080'0000
#define MEM_RESET_UNDO                  0x0100'0000
#define MEM_LARGE_PAGES                 0x2000'0000
#define MEM_4MB_PAGES                   0x8000'0000
#define MEM_64K_PAGES                   (MEM_LARGE_PAGES | MEM_PHYSICAL)
#define MEM_UNMAP_WITH_TRANSIENT_BOOST  0x0000'0001
#define MEM_COALESCE_PLACEHOLDERS       0x0000'0001
#define MEM_PRESERVE_PLACEHOLDER        0x0000'0002
#define MEM_DECOMMIT                    0x0000'4000
#define MEM_RELEASE                     0x0000'8000
#define MEM_FREE                        0x0001'0000

#define PAGE_NOACCESS           0x01
#define PAGE_READONLY           0x02
#define PAGE_READWRITE          0x04
#define PAGE_WRITECOPY          0x08
#define PAGE_EXECUTE            0x10
#define PAGE_EXECUTE_READ       0x20
#define PAGE_EXECUTE_READWRITE  0x40
#define PAGE_EXECUTE_WRITECOPY  0x80
#define PAGE_GUARD             0x100
#define PAGE_NOCACHE           0x200
#define PAGE_WRITECOMBINE      0x400

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor

#define GET_MODULE_HANDLE_EX_FLAG_PIN                 (0x0000'0001)
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT  (0x0000'0002)
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS        (0x0000'0004)

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
	union {
		DWORD Characteristics; // 0 for terminating null import descriptor
		DWORD OriginalFirstThunk; // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
	};
	DWORD TimeDateStamp; // 0 if not bound,
	// -1 if bound, and real date\time stamp
	//     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
	// O.W. date/time stamp of DLL bound to (Old BIND)

	DWORD ForwarderChain; // -1 if no forwarders
	DWORD Name;
	DWORD FirstThunk; // RVA to IAT (if bound this IAT has actual addresses)
} IMAGE_IMPORT_DESCRIPTOR;

typedef struct _MEMORY_BASIC_INFORMATION {
	PVOID BaseAddress;
	PVOID AllocationBase;
	DWORD AllocationProtect;
	WORD PartitionId;
	SIZE_T RegionSize;
	DWORD State;
	DWORD Protect;
	DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

typedef struct _IMAGE_EXPORT_DIRECTORY {
	DWORD Characteristics;
	DWORD TimeDateStamp;
	WORD MajorVersion;
	WORD MinorVersion;
	DWORD Name;
	DWORD Base;
	DWORD NumberOfFunctions;
	DWORD NumberOfNames;
	DWORD AddressOfFunctions; // RVA from base of image
	DWORD AddressOfNames; // RVA from base of image
	DWORD AddressOfNameOrdinals; // RVA from base of image
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_DOS_HEADER { // DOS .EXE header
	WORD e_magic; // Magic number
	WORD e_cblp; // Bytes on last page of file
	WORD e_cp; // Pages in file
	WORD e_crlc; // Relocations
	WORD e_cparhdr; // Size of header in paragraphs
	WORD e_minalloc; // Minimum extra paragraphs needed
	WORD e_maxalloc; // Maximum extra paragraphs needed
	WORD e_ss; // Initial (relative) SS value
	WORD e_sp; // Initial SP value
	WORD e_csum; // Checksum
	WORD e_ip; // Initial IP value
	WORD e_cs; // Initial (relative) CS value
	WORD e_lfarlc; // File address of relocation table
	WORD e_ovno; // Overlay number
	WORD e_res[4]; // Reserved words
	WORD e_oemid; // OEM identifier (for e_oeminfo)
	WORD e_oeminfo; // OEM information; e_oemid specific
	WORD e_res2[10]; // Reserved words
	LONG e_lfanew; // File address of new exe header
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
	DWORD VirtualAddress;
	DWORD Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
	WORD Magic;
	BYTE MajorLinkerVersion;
	BYTE MinorLinkerVersion;
	DWORD SizeOfCode;
	DWORD SizeOfInitializedData;
	DWORD SizeOfUninitializedData;
	DWORD AddressOfEntryPoint;
	DWORD BaseOfCode;
	ULONGLONG ImageBase;
	DWORD SectionAlignment;
	DWORD FileAlignment;
	WORD MajorOperatingSystemVersion;
	WORD MinorOperatingSystemVersion;
	WORD MajorImageVersion;
	WORD MinorImageVersion;
	WORD MajorSubsystemVersion;
	WORD MinorSubsystemVersion;
	DWORD Win32VersionValue;
	DWORD SizeOfImage;
	DWORD SizeOfHeaders;
	DWORD CheckSum;
	WORD Subsystem;
	WORD DllCharacteristics;
	ULONGLONG SizeOfStackReserve;
	ULONGLONG SizeOfStackCommit;
	ULONGLONG SizeOfHeapReserve;
	ULONGLONG SizeOfHeapCommit;
	DWORD LoaderFlags;
	DWORD NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_FILE_HEADER {
	WORD Machine;
	WORD NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD SizeOfOptionalHeader;
	WORD Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_NT_HEADERS64 {
	DWORD Signature;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;


__SPRT_BEGIN_DECL

WINAPI SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);

/**
 * Loads the specified module into the address space of the calling process.
 * @param lpLibFileName Module filename (UTF-16).
 * @return Handle to the loaded module, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryw
 */
WINAPI HMODULE LoadLibraryW(LPCWSTR lpLibFileName);

WINAPI HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

/**
 * Freels a loaded dynamic-link library.
 * @param hModule Handle to the library being unloaded.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibrary
 */
WINAPI BOOL FreeLibrary(HMODULE hModule);

/**
 * Retrieves the address of an exported function or variable from the specified module.
 * @param hModule Handle to the DLL module containing the function.
 * @param lpProcName Function name or ordinal.
 * @return Address of the exported function/variable, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress
 */
WINAPI FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

WINAPI HMODULE GetModuleHandleW(LPCWSTR lpModuleName);

/**
 * Retrieves a module handle for the specified module.
 * @param dwFlags Flags controlling behavior.
 * @param lpModuleName Module name (NULL = calling process).
 * @param phModule Pointer to receive module handle.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw
 */
WINAPI BOOL GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, PHMODULE phModule);

WINAPI DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

__SPRT_END_DECL


#endif // SPRT_WRAPPERS_WINDOWS_DL_API_H_
