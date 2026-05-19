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

#ifndef SPRT_WRAPPERS_WINDOWS_DEBUG_API_H_
#define SPRT_WRAPPERS_WINDOWS_DEBUG_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

// clang-format off
#define SYMOPT_CASE_INSENSITIVE          0x00000001
#define SYMOPT_UNDNAME                   0x00000002
#define SYMOPT_DEFERRED_LOADS            0x00000004
#define SYMOPT_NO_CPP                    0x00000008
#define SYMOPT_LOAD_LINES                0x00000010
#define SYMOPT_OMAP_FIND_NEAREST         0x00000020
#define SYMOPT_LOAD_ANYTHING             0x00000040
#define SYMOPT_IGNORE_CVREC              0x00000080
#define SYMOPT_NO_UNQUALIFIED_LOADS      0x00000100
#define SYMOPT_FAIL_CRITICAL_ERRORS      0x00000200
#define SYMOPT_EXACT_SYMBOLS             0x00000400
#define SYMOPT_ALLOW_ABSOLUTE_SYMBOLS    0x00000800
#define SYMOPT_IGNORE_NT_SYMPATH         0x00001000
#define SYMOPT_INCLUDE_32BIT_MODULES     0x00002000
#define SYMOPT_PUBLICS_ONLY              0x00004000
#define SYMOPT_NO_PUBLICS                0x00008000
#define SYMOPT_AUTO_PUBLICS              0x00010000
#define SYMOPT_NO_IMAGE_SEARCH           0x00020000
#define SYMOPT_SECURE                    0x00040000
#define SYMOPT_NO_PROMPTS                0x00080000
#define SYMOPT_OVERWRITE                 0x00100000
#define SYMOPT_IGNORE_IMAGEDIR           0x00200000
#define SYMOPT_FLAT_DIRECTORY            0x00400000
#define SYMOPT_FAVOR_COMPRESSED          0x00800000
#define SYMOPT_ALLOW_ZERO_ADDRESS        0x01000000
#define SYMOPT_DISABLE_SYMSRV_AUTODETECT 0x02000000
#define SYMOPT_READONLY_CACHE            0x04000000
#define SYMOPT_SYMPATH_LAST              0x08000000
#define SYMOPT_DISABLE_FAST_SYMBOLS      0x10000000
#define SYMOPT_DISABLE_SYMSRV_TIMEOUT    0x20000000
#define SYMOPT_DISABLE_SRVSTAR_ON_STARTUP 0x40000000
#define SYMOPT_DEBUG                     0x80000000
// clang-format on

typedef enum {
	AddrMode1616,
	AddrMode1632,
	AddrModeReal,
	AddrModeFlat
} ADDRESS_MODE;

typedef struct _tagADDRESS64 {
	DWORD64 Offset;
	WORD Segment;
	ADDRESS_MODE Mode;
} ADDRESS64, *LPADDRESS64;

typedef struct _KDHELP64 {
	DWORD64 Thread;
	DWORD ThCallbackStack;
	DWORD ThCallbackBStore;
	DWORD NextCallback;
	DWORD FramePointer;
	DWORD64 KiCallUserMode;
	DWORD64 KeUserCallbackDispatcher;
	DWORD64 SystemRangeStart;
	DWORD64 KiUserExceptionDispatcher;
	DWORD64 StackBase;
	DWORD64 StackLimit;
	DWORD BuildVersion;
	DWORD RetpolineStubFunctionTableSize;
	DWORD64 RetpolineStubFunctionTable;
	DWORD RetpolineStubOffset;
	DWORD RetpolineStubSize;
	DWORD64 Reserved0[2];
} KDHELP64, *PKDHELP64;

typedef struct _tagSTACKFRAME64 {
	ADDRESS64 AddrPC; // program counter
	ADDRESS64 AddrReturn; // return address
	ADDRESS64 AddrFrame; // frame pointer
	ADDRESS64 AddrStack; // stack pointer
	ADDRESS64 AddrBStore; // backing store pointer
	PVOID FuncTableEntry; // pointer to pdata/fpo or NULL
	DWORD64 Params[4]; // possible arguments to the function
	BOOL Far; // WOW far call
	BOOL Virtual; // is this a virtual frame?
	DWORD64 Reserved[3];
	KDHELP64 KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;

typedef struct _IMAGEHLP_SYMBOL64 {
	DWORD SizeOfStruct; // set to sizeof(IMAGEHLP_SYMBOL64)
	DWORD64 Address; // virtual address including dll base address
	DWORD Size; // estimated size of symbol, can be zero
	DWORD Flags; // info about the symbols, see the SYMF defines
	DWORD MaxNameLength; // maximum size of symbol name in Name
	CHAR Name[1]; // symbol name (null terminated string)
} IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;

typedef struct _IMAGEHLP_LINE64 {
	DWORD SizeOfStruct; // set to sizeof(IMAGEHLP_LINE64)
	PVOID Key; // internal
	DWORD LineNumber; // line number in file
	PCHAR FileName; // full filename
	DWORD64 Address; // first instruction of line
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

typedef BOOL(__stdcall *PREAD_PROCESS_MEMORY_ROUTINE64)(HANDLE hProcess, DWORD64 qwBaseAddress,
		PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

typedef PVOID(__stdcall *PFUNCTION_TABLE_ACCESS_ROUTINE64)(HANDLE ahProcess, DWORD64 AddrBase);

typedef DWORD64(__stdcall *PGET_MODULE_BASE_ROUTINE64)(HANDLE hProcess, DWORD64 Address);

typedef DWORD64(__stdcall *PTRANSLATE_ADDRESS_ROUTINE64)(HANDLE hProcess, HANDLE hThread,
		LPADDRESS64 lpaddr);

__SPRT_BEGIN_DECL

WINAPI DWORD SymSetOptions(DWORD SymOptions);

WINAPI BOOL SymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);

WINAPI DWORD SymGetOptions(VOID);

WINAPI BOOL SymCleanup(HANDLE hProcess);

WINAPI BOOL StackWalk64(DWORD MachineType, HANDLE hProcess, HANDLE hThread,
		LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
		PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
		PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
		PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
		PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);

WINAPI BOOL SymGetSymFromAddr64(HANDLE hProcess, DWORD64 qwAddr, PDWORD64 pdwDisplacement,
		PIMAGEHLP_SYMBOL64 Symbol);

WINAPI BOOL SymGetLineFromAddr64(HANDLE hProcess, DWORD64 qwAddr, PDWORD pdwDisplacement,
		PIMAGEHLP_LINE64 Line64);

WINAPI DWORD UnDecorateSymbolName(PCSTR name, PSTR outputString, DWORD maxStringLength,
		DWORD flags);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_DEBUG_API_H_
