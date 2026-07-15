// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/dbghelp.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// This is the StackWalk64 / MiniDumpWriteDump surface used by llvm's Signals.inc.
// The structs are load-bearing (passed by pointer to dbghelp.dll), so their size
// and every field offset must match the SDK. dbghelp.h supplies SYM_TYPE /
// ADDRESS_MODE / ADDRESS64 / KDHELP64 / STACKFRAME64 / IMAGEHLP_* / SYMOPT_*;
// minidumpapiset.h supplies MINIDUMP_TYPE / MINIDUMP_EXCEPTION_INFORMATION.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/dbghelp.h>
#include "abi_check.h"

#include <windows.h>
#include <dbghelp.h>
#include <minidumpapiset.h>

// enum-member parity: the abi spells enumerators as plain names (sprt_abi::NAME).
#define SPRT_ENUM(name) \
	static_assert((long long)(sprt_abi::name) == (long long)(name), \
			"sprt_abi::" #name " != SDK " #name)

// === SYMOPT_* subset used by Signals.inc ===================================
SPRT_CONST(SYMOPT_DEFERRED_LOADS);
SPRT_CONST(SYMOPT_LOAD_LINES);

// === enum SYM_TYPE =========================================================
SPRT_SIZE(SYM_TYPE);
SPRT_ENUM(SymNone);
SPRT_ENUM(SymCoff);
SPRT_ENUM(SymCv);
SPRT_ENUM(SymPdb);
SPRT_ENUM(SymExport);
SPRT_ENUM(SymDeferred);
SPRT_ENUM(SymSym);
SPRT_ENUM(SymDia);
SPRT_ENUM(SymVirtual);
SPRT_ENUM(NumSymTypes);

// === enum ADDRESS_MODE =====================================================
SPRT_SIZE(ADDRESS_MODE);
SPRT_ENUM(AddrMode1616);
SPRT_ENUM(AddrMode1632);
SPRT_ENUM(AddrModeReal);
SPRT_ENUM(AddrModeFlat);

// === enum MINIDUMP_TYPE (value parity per member; SDK has extra tail members
// so the underlying width is not compared) =================================
SPRT_ENUM(MiniDumpNormal);
SPRT_ENUM(MiniDumpWithDataSegs);
SPRT_ENUM(MiniDumpWithFullMemory);
SPRT_ENUM(MiniDumpWithHandleData);
SPRT_ENUM(MiniDumpFilterMemory);
SPRT_ENUM(MiniDumpScanMemory);
SPRT_ENUM(MiniDumpWithUnloadedModules);
SPRT_ENUM(MiniDumpWithIndirectlyReferencedMemory);
SPRT_ENUM(MiniDumpFilterModulePaths);
SPRT_ENUM(MiniDumpWithProcessThreadData);
SPRT_ENUM(MiniDumpWithPrivateReadWriteMemory);
SPRT_ENUM(MiniDumpWithoutOptionalData);
SPRT_ENUM(MiniDumpWithFullMemoryInfo);
SPRT_ENUM(MiniDumpWithThreadInfo);
SPRT_ENUM(MiniDumpWithCodeSegs);

// === ADDRESS64 =============================================================
SPRT_SIZE(ADDRESS64);
SPRT_OFFSET(ADDRESS64, Offset);
SPRT_OFFSET(ADDRESS64, Segment);
SPRT_OFFSET(ADDRESS64, Mode);

// === KDHELP64 ==============================================================
SPRT_SIZE(KDHELP64);
SPRT_OFFSET(KDHELP64, Thread);
SPRT_OFFSET(KDHELP64, ThCallbackStack);
SPRT_OFFSET(KDHELP64, KiCallUserMode);
SPRT_OFFSET(KDHELP64, StackBase);
SPRT_OFFSET(KDHELP64, StackLimit);
SPRT_OFFSET(KDHELP64, BuildVersion);
SPRT_OFFSET(KDHELP64, RetpolineStubFunctionTable);
SPRT_OFFSET(KDHELP64, Reserved0);

// === STACKFRAME64 ==========================================================
SPRT_SIZE(STACKFRAME64);
SPRT_OFFSET(STACKFRAME64, AddrPC);
SPRT_OFFSET(STACKFRAME64, AddrReturn);
SPRT_OFFSET(STACKFRAME64, AddrFrame);
SPRT_OFFSET(STACKFRAME64, AddrStack);
SPRT_OFFSET(STACKFRAME64, AddrBStore);
SPRT_OFFSET(STACKFRAME64, FuncTableEntry);
SPRT_OFFSET(STACKFRAME64, Params);
SPRT_OFFSET(STACKFRAME64, Far);
SPRT_OFFSET(STACKFRAME64, Virtual);
SPRT_OFFSET(STACKFRAME64, Reserved);
SPRT_OFFSET(STACKFRAME64, KdHelp);

// === IMAGEHLP_MODULE64 =====================================================
SPRT_SIZE(IMAGEHLP_MODULE64);
SPRT_OFFSET(IMAGEHLP_MODULE64, SizeOfStruct);
SPRT_OFFSET(IMAGEHLP_MODULE64, BaseOfImage);
SPRT_OFFSET(IMAGEHLP_MODULE64, ImageSize);
SPRT_OFFSET(IMAGEHLP_MODULE64, SymType);
SPRT_OFFSET(IMAGEHLP_MODULE64, ModuleName);
SPRT_OFFSET(IMAGEHLP_MODULE64, ImageName);
SPRT_OFFSET(IMAGEHLP_MODULE64, LoadedImageName);
SPRT_OFFSET(IMAGEHLP_MODULE64, LoadedPdbName);
SPRT_OFFSET(IMAGEHLP_MODULE64, CVData);
SPRT_OFFSET(IMAGEHLP_MODULE64, PdbSig70);
SPRT_OFFSET(IMAGEHLP_MODULE64, MachineType);

// === IMAGEHLP_SYMBOL64 =====================================================
SPRT_SIZE(IMAGEHLP_SYMBOL64);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, SizeOfStruct);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, Address);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, Size);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, Flags);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, MaxNameLength);
SPRT_OFFSET(IMAGEHLP_SYMBOL64, Name);

// === IMAGEHLP_LINE64 =======================================================
SPRT_SIZE(IMAGEHLP_LINE64);
SPRT_OFFSET(IMAGEHLP_LINE64, SizeOfStruct);
SPRT_OFFSET(IMAGEHLP_LINE64, Key);
SPRT_OFFSET(IMAGEHLP_LINE64, LineNumber);
SPRT_OFFSET(IMAGEHLP_LINE64, FileName);
SPRT_OFFSET(IMAGEHLP_LINE64, Address);

// === MINIDUMP_EXCEPTION_INFORMATION ========================================
SPRT_SIZE(MINIDUMP_EXCEPTION_INFORMATION);
SPRT_OFFSET(MINIDUMP_EXCEPTION_INFORMATION, ThreadId);
SPRT_OFFSET(MINIDUMP_EXCEPTION_INFORMATION, ExceptionPointers);
SPRT_OFFSET(MINIDUMP_EXCEPTION_INFORMATION, ClientPointers);

// === new values (wrapper completion): MINIDUMP_EXCEPTION ===================
// The exception record embedded in a minidump stream; fixed field widths per the
// minidump file format, so size and every offset must match the SDK.
SPRT_SIZE(MINIDUMP_EXCEPTION);
SPRT_OFFSET(MINIDUMP_EXCEPTION, ExceptionCode);
SPRT_OFFSET(MINIDUMP_EXCEPTION, ExceptionFlags);
SPRT_OFFSET(MINIDUMP_EXCEPTION, ExceptionRecord);
SPRT_OFFSET(MINIDUMP_EXCEPTION, ExceptionAddress);
SPRT_OFFSET(MINIDUMP_EXCEPTION, NumberParameters);
SPRT_OFFSET(MINIDUMP_EXCEPTION, __unusedAlignment);
SPRT_OFFSET(MINIDUMP_EXCEPTION, ExceptionInformation);
