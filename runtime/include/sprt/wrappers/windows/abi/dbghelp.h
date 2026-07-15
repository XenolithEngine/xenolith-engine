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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_DBGHELP_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_DBGHELP_H_

#include <sprt/wrappers/windows/abi/structures.h> // GUID + basic scalar/handle types
#include <sprt/wrappers/windows/abi/constants.h> // MAX_PATH
#include <sprt/wrappers/windows/abi/context_api.h> // CONTEXT / PEXCEPTION_POINTERS

// clang-format off

// ---- symbol type ([dbghelp] SYM_TYPE) -------------------------------------
typedef enum {
	SymNone = 0,
	SymCoff,
	SymCv,
	SymPdb,
	SymExport,
	SymDeferred,
	SymSym,
	SymDia,
	SymVirtual,
	NumSymTypes
} SYM_TYPE;

// ---- addressing mode + address ([dbghelp] ADDRESS64) ----------------------
typedef enum {
	AddrMode1616,
	AddrMode1632,
	AddrModeReal,
	AddrModeFlat
} ADDRESS_MODE;

// ---- SymSetOptions flags --------------------------------------------------
#define __SPRT_SYMOPT_DEFERRED_LOADS 0x00000004
#define __SPRT_SYMOPT_LOAD_LINES 0x00000010

// ---- minidump ([minidumpapiset] MINIDUMP_TYPE + info structs) -------------
typedef enum _MINIDUMP_TYPE {
	MiniDumpNormal = 0x00000000,
	MiniDumpWithDataSegs = 0x00000001,
	MiniDumpWithFullMemory = 0x00000002,
	MiniDumpWithHandleData = 0x00000004,
	MiniDumpFilterMemory = 0x00000008,
	MiniDumpScanMemory = 0x00000010,
	MiniDumpWithUnloadedModules = 0x00000020,
	MiniDumpWithIndirectlyReferencedMemory = 0x00000040,
	MiniDumpFilterModulePaths = 0x00000080,
	MiniDumpWithProcessThreadData = 0x00000100,
	MiniDumpWithPrivateReadWriteMemory = 0x00000200,
	MiniDumpWithoutOptionalData = 0x00000400,
	MiniDumpWithFullMemoryInfo = 0x00000800,
	MiniDumpWithThreadInfo = 0x00001000,
	MiniDumpWithCodeSegs = 0x00002000
} MINIDUMP_TYPE;

// clang-format on

// Pointer helper typedefs (C++ permits identical duplicate typedefs, so these are
// safe even if another wrapper also spells them).
typedef DWORD64 *PDWORD64;
typedef CHAR *PCHAR;

typedef struct _tagADDRESS64 {
	DWORD64 Offset;
	WORD Segment;
	ADDRESS_MODE Mode;
} ADDRESS64, *LPADDRESS64;

// ---- KDHELP64 (embedded in STACKFRAME64; layout is load-bearing) ----------
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
	ADDRESS64 AddrPC;
	ADDRESS64 AddrReturn;
	ADDRESS64 AddrFrame;
	ADDRESS64 AddrStack;
	ADDRESS64 AddrBStore;
	PVOID FuncTableEntry;
	DWORD64 Params[4];
	BOOL Far;
	BOOL Virtual;
	DWORD64 Reserved[3];
	KDHELP64 KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;

// ---- module / symbol / line info ------------------------------------------
typedef struct _IMAGEHLP_MODULE64 {
	DWORD SizeOfStruct;
	DWORD64 BaseOfImage;
	DWORD ImageSize;
	DWORD TimeDateStamp;
	DWORD CheckSum;
	DWORD NumSyms;
	SYM_TYPE SymType;
	CHAR ModuleName[32];
	CHAR ImageName[256];
	CHAR LoadedImageName[256];
	CHAR LoadedPdbName[256];
	DWORD CVSig;
	CHAR CVData[__SPRT_MAX_PATH * 3];
	DWORD PdbSig;
	GUID PdbSig70;
	DWORD PdbAge;
	BOOL PdbUnmatched;
	BOOL DbgUnmatched;
	BOOL LineNumbers;
	BOOL GlobalSymbols;
	BOOL TypeInfo;
	BOOL SourceIndexed;
	BOOL Publics;
	DWORD MachineType;
	DWORD Reserved;
} IMAGEHLP_MODULE64, *PIMAGEHLP_MODULE64;

typedef struct _IMAGEHLP_SYMBOL64 {
	DWORD SizeOfStruct;
	DWORD64 Address;
	DWORD Size;
	DWORD Flags;
	DWORD MaxNameLength;
	CHAR Name[1];
} IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;

typedef struct _IMAGEHLP_LINE64 {
	DWORD SizeOfStruct;
	PVOID Key;
	DWORD LineNumber;
	PCHAR FileName;
	DWORD64 Address;
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

// ---- StackWalk64 callback routine typedefs --------------------------------
typedef BOOL(__SPRT_WINAPI *PREAD_PROCESS_MEMORY_ROUTINE64)(HANDLE hProcess, DWORD64 qwBaseAddress,
		PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);
typedef PVOID(__SPRT_WINAPI *PFUNCTION_TABLE_ACCESS_ROUTINE64)(HANDLE ahProcess, DWORD64 AddrBase);
typedef DWORD64(__SPRT_WINAPI *PGET_MODULE_BASE_ROUTINE64)(HANDLE hProcess, DWORD64 Address);
typedef DWORD64(__SPRT_WINAPI *PTRANSLATE_ADDRESS_ROUTINE64)(HANDLE hProcess, HANDLE hThread,
		LPADDRESS64 lpaddr);

// EnumerateLoadedModules64 callback (also GetProcAddress-loaded by Signals.inc).
typedef BOOL(__SPRT_WINAPI *PENUMLOADED_MODULES_CALLBACK64)(PCSTR ModuleName, DWORD64 ModuleBase,
		ULONG ModuleSize, PVOID UserContext);

// The minidump structures are 4-byte packed in the SDK (minidumpapiset.h wraps them
// in <pshpack4.h>), so ExceptionPointers sits at offset 4, not 8. Match that layout
// exactly — this struct is passed by pointer to MiniDumpWriteDump.
#pragma pack(push, 4)
typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
	DWORD ThreadId;
	PEXCEPTION_POINTERS ExceptionPointers;
	BOOL ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;
#pragma pack(pop)

// Only ever passed as a null pointer to MiniDumpWriteDump; kept opaque.
typedef struct _MINIDUMP_USER_STREAM_INFORMATION MINIDUMP_USER_STREAM_INFORMATION,
		*PMINIDUMP_USER_STREAM_INFORMATION;
typedef struct _MINIDUMP_CALLBACK_INFORMATION MINIDUMP_CALLBACK_INFORMATION,
		*PMINIDUMP_CALLBACK_INFORMATION;

// The exception record embedded in a minidump exception stream (fixed 32/64-bit
// field widths per the minidump file format).
typedef struct _MINIDUMP_EXCEPTION {
	unsigned int ExceptionCode;
	unsigned int ExceptionFlags;
	unsigned long long ExceptionRecord;
	unsigned long long ExceptionAddress;
	unsigned int NumberParameters;
	unsigned int __unusedAlignment;
	unsigned long long ExceptionInformation[15];
} MINIDUMP_EXCEPTION, *PMINIDUMP_EXCEPTION;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_DBGHELP_H_
