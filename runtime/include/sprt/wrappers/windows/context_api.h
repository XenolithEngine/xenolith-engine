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

#ifndef SPRT_WRAPPERS_WINDOWS_CONTEXT_API_H_
#define SPRT_WRAPPERS_WINDOWS_CONTEXT_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

typedef int NTSTATUS;

#define UNW_FLAG_NHANDLER       0x0
#define UNW_FLAG_EHANDLER       0x1
#define UNW_FLAG_UHANDLER       0x2
#define UNW_FLAG_CHAININFO      0x4

#define STATUS_LONGJUMP                  ((NTSTATUS)0x8000'0026L)    // winnt

#define EXCEPTION_MAXIMUM_PARAMETERS 15 // maximum number of exception parameters

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

typedef struct SPRT_ALIGNAS(16) _M128A {
	ULONGLONG Low;
	LONGLONG High;
} M128A, *PM128A;

typedef struct SPRT_ALIGNAS(16) _XSAVE_FORMAT {
	WORD ControlWord;
	WORD StatusWord;
	BYTE TagWord;
	BYTE Reserved1;
	WORD ErrorOpcode;
	DWORD ErrorOffset;
	WORD ErrorSelector;
	WORD Reserved2;
	DWORD DataOffset;
	WORD DataSelector;
	WORD Reserved3;
	DWORD MxCsr;
	DWORD MxCsr_Mask;
	M128A FloatRegisters[8];
	M128A XmmRegisters[16];
	BYTE Reserved4[96];
} XSAVE_FORMAT, *PXSAVE_FORMAT;

typedef XSAVE_FORMAT XMM_SAVE_AREA32, *PXMM_SAVE_AREA32;

typedef struct SPRT_ALIGNAS(16) _CONTEXT {

	//
	// Register parameter home addresses.
	//
	// N.B. These fields are for convience - they could be used to extend the
	//      context record in the future.
	//

	DWORD64 P1Home;
	DWORD64 P2Home;
	DWORD64 P3Home;
	DWORD64 P4Home;
	DWORD64 P5Home;
	DWORD64 P6Home;

	//
	// Control flags.
	//

	DWORD ContextFlags;
	DWORD MxCsr;

	//
	// Segment Registers and processor flags.
	//

	WORD SegCs;
	WORD SegDs;
	WORD SegEs;
	WORD SegFs;
	WORD SegGs;
	WORD SegSs;
	DWORD EFlags;

	//
	// Debug registers
	//

	DWORD64 Dr0;
	DWORD64 Dr1;
	DWORD64 Dr2;
	DWORD64 Dr3;
	DWORD64 Dr6;
	DWORD64 Dr7;

	//
	// Integer registers.
	//

	DWORD64 Rax;
	DWORD64 Rcx;
	DWORD64 Rdx;
	DWORD64 Rbx;
	DWORD64 Rsp;
	DWORD64 Rbp;
	DWORD64 Rsi;
	DWORD64 Rdi;
	DWORD64 R8;
	DWORD64 R9;
	DWORD64 R10;
	DWORD64 R11;
	DWORD64 R12;
	DWORD64 R13;
	DWORD64 R14;
	DWORD64 R15;

	//
	// Program counter.
	//

	DWORD64 Rip;

	//
	// Floating point state.
	//

	union {
		XMM_SAVE_AREA32 FltSave;
		struct {
			M128A Header[2];
			M128A Legacy[8];
			M128A Xmm0;
			M128A Xmm1;
			M128A Xmm2;
			M128A Xmm3;
			M128A Xmm4;
			M128A Xmm5;
			M128A Xmm6;
			M128A Xmm7;
			M128A Xmm8;
			M128A Xmm9;
			M128A Xmm10;
			M128A Xmm11;
			M128A Xmm12;
			M128A Xmm13;
			M128A Xmm14;
			M128A Xmm15;
		};
	};

	//
	// Vector registers.
	//

	M128A VectorRegister[26];
	DWORD64 VectorControl;

	//
	// Special debug control registers.
	//

	DWORD64 DebugControl;
	DWORD64 LastBranchToRip;
	DWORD64 LastBranchFromRip;
	DWORD64 LastExceptionToRip;
	DWORD64 LastExceptionFromRip;
} CONTEXT, *PCONTEXT;

typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY {
	DWORD BeginAddress;
	DWORD EndAddress;
	union {
		DWORD UnwindInfoAddress;
		DWORD UnwindData;
	} DUMMYUNIONNAME;
} IMAGE_RUNTIME_FUNCTION_ENTRY, *PIMAGE_RUNTIME_FUNCTION_ENTRY;

#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64

#define ARM_MAX_BREAKPOINTS     8
#define ARM_MAX_WATCHPOINTS     1

typedef struct _NEON128 {
	ULONGLONG Low;
	LONGLONG High;
} NEON128, *PNEON128;

typedef struct SPRT_ALIGNAS(8) _CONTEXT {

	//
	// Control flags.
	//

	DWORD ContextFlags;

	//
	// Integer registers
	//

	DWORD R0;
	DWORD R1;
	DWORD R2;
	DWORD R3;
	DWORD R4;
	DWORD R5;
	DWORD R6;
	DWORD R7;
	DWORD R8;
	DWORD R9;
	DWORD R10;
	DWORD R11;
	DWORD R12;

	//
	// Control Registers
	//

	DWORD Sp;
	DWORD Lr;
	DWORD Pc;
	DWORD Cpsr;

	//
	// Floating Point/NEON Registers
	//

	DWORD Fpscr;
	DWORD Padding;
	union {
		NEON128 Q[16];
		ULONGLONG D[32];
		DWORD S[32];
	} DUMMYUNIONNAME;

	//
	// Debug registers
	//

	DWORD Bvr[ARM_MAX_BREAKPOINTS];
	DWORD Bcr[ARM_MAX_BREAKPOINTS];
	DWORD Wvr[ARM_MAX_WATCHPOINTS];
	DWORD Wcr[ARM_MAX_WATCHPOINTS];

	DWORD Padding2[2];

} CONTEXT, *PCONTEXT;

typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY {
	DWORD BeginAddress;
	union {
		DWORD UnwindData;
		struct {
			DWORD Flag			 : 2;
			DWORD FunctionLength : 11;
			DWORD RegF			 : 3;
			DWORD RegI			 : 4;
			DWORD H				 : 1;
			DWORD CR			 : 2;
			DWORD FrameSize		 : 9;
		};
	};
} IMAGE_RUNTIME_FUNCTION_ENTRY, *PIMAGE_RUNTIME_FUNCTION_ENTRY;

#else

#error Not implemented

#endif

typedef struct _EXCEPTION_RECORD {
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	struct _EXCEPTION_RECORD *ExceptionRecord;
	PVOID ExceptionAddress;
	DWORD NumberParameters;
	ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD;

#define UNWIND_HISTORY_TABLE_SIZE 12

typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
	ULONG_PTR ImageBase;
	PRUNTIME_FUNCTION FunctionEntry;
} UNWIND_HISTORY_TABLE_ENTRY, *PUNWIND_HISTORY_TABLE_ENTRY;

typedef struct _UNWIND_HISTORY_TABLE {
	DWORD Count;
	BYTE LocalHint;
	BYTE GlobalHint;
	BYTE Search;
	BYTE Once;
	ULONG_PTR LowAddress;
	ULONG_PTR HighAddress;
	UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;

typedef EXCEPTION_RECORD *PEXCEPTION_RECORD;

typedef enum _EXCEPTION_DISPOSITION {
	ExceptionContinueExecution,
	ExceptionContinueSearch,
	ExceptionNestedException,
	ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

typedef EXCEPTION_DISPOSITION EXCEPTION_ROUTINE(struct _EXCEPTION_RECORD *ExceptionRecord,
		PVOID EstablisherFrame, struct _CONTEXT *ContextRecord, PVOID DispatcherContext);

typedef EXCEPTION_ROUTINE *PEXCEPTION_ROUTINE;

__SPRT_BEGIN_DECL

VOID RtlCaptureContext(PCONTEXT ContextRecord);

VOID RtlRestoreContext(PCONTEXT ContextRecord, _EXCEPTION_RECORD *ExceptionRecord);

PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64 ControlPc, PDWORD64 ImageBase,
		PUNWIND_HISTORY_TABLE HistoryTable);

VOID RtlUnwindEx(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord,
		PVOID ReturnValue, PCONTEXT ContextRecord, PUNWIND_HISTORY_TABLE HistoryTable);

PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc,
		PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID *HandlerData,
		PDWORD64 EstablisherFrame, PVOID ContextPointers);

__SPRT_END_DECL

#endif
