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

// clang-format off
#define IMAGE_SIZEOF_FILE_HEADER             20

#define IMAGE_FILE_RELOCS_STRIPPED           0x0001  // Relocation info stripped from file.
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002  // File is executable  (i.e. no unresolved external references).
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004  // Line nunbers stripped from file.
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008  // Local symbols stripped from file.
#define IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010  // Aggressively trim working set
#define IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020  // App can handle >2gb addresses
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080  // Bytes of machine word are reversed.
#define IMAGE_FILE_32BIT_MACHINE             0x0100  // 32 bit word machine.
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200  // Debugging info stripped from file in .DBG file
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400  // If Image is on removable media, copy and run from the swap file.
#define IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800  // If Image is on Net, copy and run from the swap file.
#define IMAGE_FILE_SYSTEM                    0x1000  // System File.
#define IMAGE_FILE_DLL                       0x2000  // File is a DLL.
#define IMAGE_FILE_UP_SYSTEM_ONLY            0x4000  // File should only be run on a UP machine
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000  // Bytes of machine word are reversed.

#define IMAGE_FILE_MACHINE_UNKNOWN           0
#define IMAGE_FILE_MACHINE_TARGET_HOST       0x0001  // Useful for indicating we want to interact with the host and not a WoW guest.
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.
#define IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define IMAGE_FILE_MACHINE_SH3               0x01a2  // SH3 little-endian
#define IMAGE_FILE_MACHINE_SH3DSP            0x01a3
#define IMAGE_FILE_MACHINE_SH3E              0x01a4  // SH3E little-endian
#define IMAGE_FILE_MACHINE_SH4               0x01a6  // SH4 little-endian
#define IMAGE_FILE_MACHINE_SH5               0x01a8  // SH5
#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_THUMB             0x01c2  // ARM Thumb/Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARM Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_AM33              0x01d3
#define IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define IMAGE_FILE_MACHINE_POWERPCFP         0x01f1
#define IMAGE_FILE_MACHINE_IA64              0x0200  // Intel 64
#define IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS
#define IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS
#define IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS
#define IMAGE_FILE_MACHINE_AXP64             IMAGE_FILE_MACHINE_ALPHA64
#define IMAGE_FILE_MACHINE_TRICORE           0x0520  // Infineon
#define IMAGE_FILE_MACHINE_CEF               0x0CEF
#define IMAGE_FILE_MACHINE_EBC               0x0EBC  // EFI Byte Code
#define IMAGE_FILE_MACHINE_AMD64             0x8664  // AMD64 (K8)
#define IMAGE_FILE_MACHINE_M32R              0x9041  // M32R little-endian
#define IMAGE_FILE_MACHINE_ARM64             0xAA64  // ARM64 Little-Endian
#define IMAGE_FILE_MACHINE_CEE               0xC0EE

#define UNW_FLAG_NHANDLER       0x0
#define UNW_FLAG_EHANDLER       0x1
#define UNW_FLAG_UHANDLER       0x2
#define UNW_FLAG_CHAININFO      0x4

#define STILL_ACTIVE                        STATUS_PENDING
#define EXCEPTION_ACCESS_VIOLATION          STATUS_ACCESS_VIOLATION
#define EXCEPTION_DATATYPE_MISALIGNMENT     STATUS_DATATYPE_MISALIGNMENT
#define EXCEPTION_BREAKPOINT                STATUS_BREAKPOINT
#define EXCEPTION_SINGLE_STEP               STATUS_SINGLE_STEP
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     STATUS_ARRAY_BOUNDS_EXCEEDED
#define EXCEPTION_FLT_DENORMAL_OPERAND      STATUS_FLOAT_DENORMAL_OPERAND
#define EXCEPTION_FLT_DIVIDE_BY_ZERO        STATUS_FLOAT_DIVIDE_BY_ZERO
#define EXCEPTION_FLT_INEXACT_RESULT        STATUS_FLOAT_INEXACT_RESULT
#define EXCEPTION_FLT_INVALID_OPERATION     STATUS_FLOAT_INVALID_OPERATION
#define EXCEPTION_FLT_OVERFLOW              STATUS_FLOAT_OVERFLOW
#define EXCEPTION_FLT_STACK_CHECK           STATUS_FLOAT_STACK_CHECK
#define EXCEPTION_FLT_UNDERFLOW             STATUS_FLOAT_UNDERFLOW
#define EXCEPTION_INT_DIVIDE_BY_ZERO        STATUS_INTEGER_DIVIDE_BY_ZERO
#define EXCEPTION_INT_OVERFLOW              STATUS_INTEGER_OVERFLOW
#define EXCEPTION_PRIV_INSTRUCTION          STATUS_PRIVILEGED_INSTRUCTION
#define EXCEPTION_IN_PAGE_ERROR             STATUS_IN_PAGE_ERROR
#define EXCEPTION_ILLEGAL_INSTRUCTION       STATUS_ILLEGAL_INSTRUCTION
#define EXCEPTION_NONCONTINUABLE_EXCEPTION  STATUS_NONCONTINUABLE_EXCEPTION
#define EXCEPTION_STACK_OVERFLOW            STATUS_STACK_OVERFLOW
#define EXCEPTION_INVALID_DISPOSITION       STATUS_INVALID_DISPOSITION
#define EXCEPTION_GUARD_PAGE                STATUS_GUARD_PAGE_VIOLATION
#define EXCEPTION_INVALID_HANDLE            STATUS_INVALID_HANDLE
#define EXCEPTION_POSSIBLE_DEADLOCK         STATUS_POSSIBLE_DEADLOCK
#define CONTROL_C_EXIT                      STATUS_CONTROL_C_EXIT

#define EXCEPTION_EXECUTE_HANDLER      1
#define EXCEPTION_CONTINUE_SEARCH      0
#define EXCEPTION_CONTINUE_EXECUTION (-1)

#define EXCEPTION_MAXIMUM_PARAMETERS 15 // maximum number of exception parameters
// clang-format on


typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

typedef enum _EXCEPTION_DISPOSITION {
	ExceptionContinueExecution,
	ExceptionContinueSearch,
	ExceptionNestedException,
	ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

struct _EXCEPTION_RECORD;
struct _CONTEXT;

typedef EXCEPTION_DISPOSITION EXCEPTION_ROUTINE(struct _EXCEPTION_RECORD *ExceptionRecord,
		PVOID EstablisherFrame, struct _CONTEXT *ContextRecord, PVOID DispatcherContext);

typedef EXCEPTION_ROUTINE *PEXCEPTION_ROUTINE;

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

// clang-format off
#define CONTEXT_AMD64                            0x00100000L
#define CONTEXT_CONTROL         (CONTEXT_AMD64 | 0x00000001L)
#define CONTEXT_INTEGER         (CONTEXT_AMD64 | 0x00000002L)
#define CONTEXT_SEGMENTS        (CONTEXT_AMD64 | 0x00000004L)
#define CONTEXT_FLOATING_POINT  (CONTEXT_AMD64 | 0x00000008L)
#define CONTEXT_DEBUG_REGISTERS (CONTEXT_AMD64 | 0x00000010L)

#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)

#define CONTEXT_ALL  (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS)

// clang-format on

typedef union _UNWIND_CODE {
	struct {
		unsigned char CodeOffset;
		unsigned char UnwindOp : 4;
		unsigned char OpInfo   : 4;
	};
	unsigned short FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
	unsigned char Version : 3;
	unsigned char Flags	  : 5;
	unsigned char SizeOfProlog;
	unsigned char CountOfCodes;
	unsigned char FrameRegister : 4;
	unsigned char FrameOffset	: 4;
	UNWIND_CODE UnwindCode[1];
} UNWIND_INFO, *PUNWIND_INFO;

typedef struct SPRT_ALIGNAS(16) _SETJMP_FLOAT128 {
	__uint64 Part[2];
} SETJMP_FLOAT128;

#define _JBLEN  24

typedef SETJMP_FLOAT128 _JBTYPE;

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
	DWORD64 P1Home; /* 000 */
	DWORD64 P2Home; /* 008 */
	DWORD64 P3Home; /* 010 */
	DWORD64 P4Home; /* 018 */
	DWORD64 P5Home; /* 020 */
	DWORD64 P6Home; /* 028 */

	/* Control flags */
	DWORD ContextFlags; /* 030 */
	DWORD MxCsr; /* 034 */

	/* Segment */
	WORD SegCs; /* 038 */
	WORD SegDs; /* 03a */
	WORD SegEs; /* 03c */
	WORD SegFs; /* 03e */
	WORD SegGs; /* 040 */
	WORD SegSs; /* 042 */
	DWORD EFlags; /* 044 */

	/* Debug */
	DWORD64 Dr0; /* 048 */
	DWORD64 Dr1; /* 050 */
	DWORD64 Dr2; /* 058 */
	DWORD64 Dr3; /* 060 */
	DWORD64 Dr6; /* 068 */
	DWORD64 Dr7; /* 070 */

	/* Integer */
	DWORD64 Rax; /* 078 */
	DWORD64 Rcx; /* 080 */
	DWORD64 Rdx; /* 088 */
	DWORD64 Rbx; /* 090 */
	DWORD64 Rsp; /* 098 */
	DWORD64 Rbp; /* 0a0 */
	DWORD64 Rsi; /* 0a8 */
	DWORD64 Rdi; /* 0b0 */
	DWORD64 R8; /* 0b8 */
	DWORD64 R9; /* 0c0 */
	DWORD64 R10; /* 0c8 */
	DWORD64 R11; /* 0d0 */
	DWORD64 R12; /* 0d8 */
	DWORD64 R13; /* 0e0 */
	DWORD64 R14; /* 0e8 */
	DWORD64 R15; /* 0f0 */

	/* Counter */
	DWORD64 Rip; /* 0f8 */

	/* Floating point */
	union {
		XMM_SAVE_AREA32 FltSave; /* 100 */
		struct {
			M128A Header[2]; /* 100 */
			M128A Legacy[8]; /* 120 */
			M128A Xmm0; /* 1a0 */
			M128A Xmm1; /* 1b0 */
			M128A Xmm2; /* 1c0 */
			M128A Xmm3; /* 1d0 */
			M128A Xmm4; /* 1e0 */
			M128A Xmm5; /* 1f0 */
			M128A Xmm6; /* 200 */
			M128A Xmm7; /* 210 */
			M128A Xmm8; /* 220 */
			M128A Xmm9; /* 230 */
			M128A Xmm10; /* 240 */
			M128A Xmm11; /* 250 */
			M128A Xmm12; /* 260 */
			M128A Xmm13; /* 270 */
			M128A Xmm14; /* 280 */
			M128A Xmm15; /* 290 */
		};
	};

	M128A VectorRegister[26]; /* 300 */
	DWORD64 VectorControl; /* 4a0 */

	DWORD64 DebugControl; /* 4a8 */
	DWORD64 LastBranchToRip; /* 4b0 */
	DWORD64 LastBranchFromRip; /* 4b8 */
	DWORD64 LastExceptionToRip; /* 4c0 */
	DWORD64 LastExceptionFromRip; /* 4c8 */
} CONTEXT, *PCONTEXT;

typedef struct _DISPATCHER_CONTEXT {
	DWORD64 ControlPc;
	DWORD64 ImageBase;
	PRUNTIME_FUNCTION FunctionEntry;
	DWORD64 EstablisherFrame;
	DWORD64 TargetIp;
	PCONTEXT ContextRecord;
	PEXCEPTION_ROUTINE LanguageHandler;
	PVOID HandlerData;
	struct _UNWIND_HISTORY_TABLE *HistoryTable;
	DWORD ScopeIndex;
	DWORD Fill0;
} DISPATCHER_CONTEXT, *PDISPATCHER_CONTEXT;

struct SPRT_ALIGNAS(16) _JUMP_BUFFER {
	__uint64 Frame;
	__uint64 Rbx;
	__uint64 Rsp;
	__uint64 Rbp;
	__uint64 Rsi;
	__uint64 Rdi;
	__uint64 R12;
	__uint64 R13;
	__uint64 R14;
	__uint64 R15;
	__uint64 Rip;
	unsigned long MxCsr;
	unsigned short FpCsr;
	unsigned short Spare;

	SETJMP_FLOAT128 Xmm6;
	SETJMP_FLOAT128 Xmm7;
	SETJMP_FLOAT128 Xmm8;
	SETJMP_FLOAT128 Xmm9;
	SETJMP_FLOAT128 Xmm10;
	SETJMP_FLOAT128 Xmm11;
	SETJMP_FLOAT128 Xmm12;
	SETJMP_FLOAT128 Xmm13;
	SETJMP_FLOAT128 Xmm14;
	SETJMP_FLOAT128 Xmm15;
};

typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY {
	DWORD BeginAddress;
	DWORD EndAddress;
	union {
		DWORD UnwindInfoAddress;
		DWORD UnwindData;
	};
} IMAGE_RUNTIME_FUNCTION_ENTRY, *PIMAGE_RUNTIME_FUNCTION_ENTRY;

#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64

#define _JBLEN  24
#define _JBTYPE __uint64

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

typedef struct _EXCEPTION_POINTERS {
	PEXCEPTION_RECORD ExceptionRecord;
	PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

typedef LONG (*PVECTORED_EXCEPTION_HANDLER)(PEXCEPTION_POINTERS ExceptionInfo);

typedef struct _NT_TIB {
	struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID SubSystemTib;
	PVOID FiberData;
	PVOID ArbitraryUserPointer;
	struct _NT_TIB *Self;
} NT_TIB;
typedef NT_TIB *PNT_TIB;

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI VOID RtlCaptureContext(PCONTEXT ContextRecord);

__SPRT_WIN_IMPORT WINAPI VOID RtlRestoreContext(PCONTEXT ContextRecord,
		EXCEPTION_RECORD *ExceptionRecord);

__SPRT_WIN_IMPORT WINAPI PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64 ControlPc,
		PDWORD64 ImageBase, PUNWIND_HISTORY_TABLE HistoryTable);

__SPRT_WIN_IMPORT WINAPI VOID RtlUnwindEx(PVOID TargetFrame, PVOID TargetIp,
		PEXCEPTION_RECORD ExceptionRecord, PVOID ReturnValue, PCONTEXT ContextRecord,
		PUNWIND_HISTORY_TABLE HistoryTable);

__SPRT_WIN_IMPORT WINAPI PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase,
		DWORD64 ControlPc, PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord,
		PVOID *HandlerData, PDWORD64 EstablisherFrame, PVOID ContextPointers);

__SPRT_WIN_IMPORT WINAPI WORD RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture,
		PVOID *BackTrace, PDWORD BackTraceHash);

__SPRT_WIN_IMPORT WINAPI PVOID AddVectoredExceptionHandler(ULONG First,
		PVECTORED_EXCEPTION_HANDLER Handler);

__SPRT_WIN_IMPORT WINAPI ULONG RemoveVectoredExceptionHandler(PVOID Handle);

__SPRT_WIN_IMPORT WINAPI PVOID AddVectoredContinueHandler(ULONG First,
		PVECTORED_EXCEPTION_HANDLER Handler);

__SPRT_WIN_IMPORT WINAPI ULONG RemoveVectoredContinueHandler(PVOID Handle);

__SPRT_WIN_IMPORT WINAPI NTSTATUS NtContinueEx(CONTEXT *context, void *args);

__SPRT_WIN_IMPORT WINAPI VOID RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags,
		DWORD nNumberOfArguments, const ULONG_PTR *lpArguments);

SPRT_FORCEINLINE PVOID GetCurrentFiber(VOID) {
	return (PVOID)__readgsqword(FIELD_OFFSET(NT_TIB, FiberData));
}

SPRT_FORCEINLINE PVOID GetFiberData(VOID) { return *(PVOID *)GetCurrentFiber(); }

// Function prototypes
// When <setjmp is included - this should not be visible
#ifndef CORE_RUNTIME_INCLUDE_LIBC_SETJMP_H_
__attribute__((returns_twice)) int setjmp(struct _JUMP_BUFFER *_Buf, void *);

__SPRT_NORETURN void __cdecl longjmp(struct _JUMP_BUFFER *, int _Value);
#endif

__SPRT_END_DECL

#endif
