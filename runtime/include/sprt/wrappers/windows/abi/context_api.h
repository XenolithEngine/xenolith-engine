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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_CONTEXT_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_CONTEXT_API_H_

#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>
#include <sprt/wrappers/windows/abi/thread_api.h>

// clang-format off
#define __SPRT_IMAGE_SIZEOF_FILE_HEADER             20

#define __SPRT_IMAGE_FILE_RELOCS_STRIPPED           0x0001  // Relocation info stripped from file.
#define __SPRT_IMAGE_FILE_EXECUTABLE_IMAGE          0x0002  // File is executable  (i.e. no unresolved external references).
#define __SPRT_IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004  // Line nunbers stripped from file.
#define __SPRT_IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008  // Local symbols stripped from file.
#define __SPRT_IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010  // Aggressively trim working set
#define __SPRT_IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020  // App can handle >2gb addresses
#define __SPRT_IMAGE_FILE_BYTES_REVERSED_LO         0x0080  // Bytes of machine word are reversed.
#define __SPRT_IMAGE_FILE_32BIT_MACHINE             0x0100  // 32 bit word machine.
#define __SPRT_IMAGE_FILE_DEBUG_STRIPPED            0x0200  // Debugging info stripped from file in .DBG file
#define __SPRT_IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400  // If Image is on removable media, copy and run from the swap file.
#define __SPRT_IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800  // If Image is on Net, copy and run from the swap file.
#define __SPRT_IMAGE_FILE_SYSTEM                    0x1000  // System File.
#define __SPRT_IMAGE_FILE_DLL                       0x2000  // File is a DLL.
#define __SPRT_IMAGE_FILE_UP_SYSTEM_ONLY            0x4000  // File should only be run on a UP machine
#define __SPRT_IMAGE_FILE_BYTES_REVERSED_HI         0x8000  // Bytes of machine word are reversed.

#define __SPRT_IMAGE_FILE_MACHINE_UNKNOWN           0
#define __SPRT_IMAGE_FILE_MACHINE_TARGET_HOST       0x0001  // Useful for indicating we want to interact with the host and not a WoW guest.
#define __SPRT_IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.
#define __SPRT_IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define __SPRT_IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define __SPRT_IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define __SPRT_IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define __SPRT_IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define __SPRT_IMAGE_FILE_MACHINE_SH3               0x01a2  // SH3 little-endian
#define __SPRT_IMAGE_FILE_MACHINE_SH3DSP            0x01a3
#define __SPRT_IMAGE_FILE_MACHINE_SH3E              0x01a4  // SH3E little-endian
#define __SPRT_IMAGE_FILE_MACHINE_SH4               0x01a6  // SH4 little-endian
#define __SPRT_IMAGE_FILE_MACHINE_SH5               0x01a8  // SH5
#define __SPRT_IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define __SPRT_IMAGE_FILE_MACHINE_THUMB             0x01c2  // ARM Thumb/Thumb-2 Little-Endian
#define __SPRT_IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARM Thumb-2 Little-Endian
#define __SPRT_IMAGE_FILE_MACHINE_AM33              0x01d3
#define __SPRT_IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define __SPRT_IMAGE_FILE_MACHINE_POWERPCFP         0x01f1
#define __SPRT_IMAGE_FILE_MACHINE_IA64              0x0200  // Intel 64
#define __SPRT_IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS
#define __SPRT_IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define __SPRT_IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS
#define __SPRT_IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS
#define __SPRT_IMAGE_FILE_MACHINE_AXP64             __SPRT_IMAGE_FILE_MACHINE_ALPHA64
#define __SPRT_IMAGE_FILE_MACHINE_TRICORE           0x0520  // Infineon
#define __SPRT_IMAGE_FILE_MACHINE_CEF               0x0CEF
#define __SPRT_IMAGE_FILE_MACHINE_EBC               0x0EBC  // EFI Byte Code
#define __SPRT_IMAGE_FILE_MACHINE_AMD64             0x8664  // AMD64 (K8)
#define __SPRT_IMAGE_FILE_MACHINE_M32R              0x9041  // M32R little-endian
#define __SPRT_IMAGE_FILE_MACHINE_ARM64             0xAA64  // ARM64 Little-Endian
#define __SPRT_IMAGE_FILE_MACHINE_CEE               0xC0EE

#define __SPRT_UNW_FLAG_NHANDLER       0x0
#define __SPRT_UNW_FLAG_EHANDLER       0x1
#define __SPRT_UNW_FLAG_UHANDLER       0x2
#define __SPRT_UNW_FLAG_CHAININFO      0x4

#define __SPRT_STILL_ACTIVE                        __SPRT_STATUS_PENDING
#define __SPRT_EXCEPTION_ACCESS_VIOLATION          __SPRT_STATUS_ACCESS_VIOLATION
#define __SPRT_EXCEPTION_DATATYPE_MISALIGNMENT     __SPRT_STATUS_DATATYPE_MISALIGNMENT
#define __SPRT_EXCEPTION_BREAKPOINT                __SPRT_STATUS_BREAKPOINT
#define __SPRT_EXCEPTION_SINGLE_STEP               __SPRT_STATUS_SINGLE_STEP
#define __SPRT_EXCEPTION_ARRAY_BOUNDS_EXCEEDED     __SPRT_STATUS_ARRAY_BOUNDS_EXCEEDED
#define __SPRT_EXCEPTION_FLT_DENORMAL_OPERAND      __SPRT_STATUS_FLOAT_DENORMAL_OPERAND
#define __SPRT_EXCEPTION_FLT_DIVIDE_BY_ZERO        __SPRT_STATUS_FLOAT_DIVIDE_BY_ZERO
#define __SPRT_EXCEPTION_FLT_INEXACT_RESULT        __SPRT_STATUS_FLOAT_INEXACT_RESULT
#define __SPRT_EXCEPTION_FLT_INVALID_OPERATION     __SPRT_STATUS_FLOAT_INVALID_OPERATION
#define __SPRT_EXCEPTION_FLT_OVERFLOW              __SPRT_STATUS_FLOAT_OVERFLOW
#define __SPRT_EXCEPTION_FLT_STACK_CHECK           __SPRT_STATUS_FLOAT_STACK_CHECK
#define __SPRT_EXCEPTION_FLT_UNDERFLOW             __SPRT_STATUS_FLOAT_UNDERFLOW
#define __SPRT_EXCEPTION_INT_DIVIDE_BY_ZERO        __SPRT_STATUS_INTEGER_DIVIDE_BY_ZERO
#define __SPRT_EXCEPTION_INT_OVERFLOW              __SPRT_STATUS_INTEGER_OVERFLOW
#define __SPRT_EXCEPTION_PRIV_INSTRUCTION          __SPRT_STATUS_PRIVILEGED_INSTRUCTION
#define __SPRT_EXCEPTION_IN_PAGE_ERROR             __SPRT_STATUS_IN_PAGE_ERROR
#define __SPRT_EXCEPTION_ILLEGAL_INSTRUCTION       __SPRT_STATUS_ILLEGAL_INSTRUCTION
#define __SPRT_EXCEPTION_NONCONTINUABLE_EXCEPTION  __SPRT_STATUS_NONCONTINUABLE_EXCEPTION
#define __SPRT_EXCEPTION_STACK_OVERFLOW            __SPRT_STATUS_STACK_OVERFLOW
#define __SPRT_EXCEPTION_INVALID_DISPOSITION       __SPRT_STATUS_INVALID_DISPOSITION
#define __SPRT_EXCEPTION_GUARD_PAGE                __SPRT_STATUS_GUARD_PAGE_VIOLATION
#define __SPRT_EXCEPTION_INVALID_HANDLE            __SPRT_STATUS_INVALID_HANDLE
#define __SPRT_EXCEPTION_POSSIBLE_DEADLOCK         __SPRT_STATUS_POSSIBLE_DEADLOCK
#define __SPRT_CONTROL_C_EXIT                      __SPRT_STATUS_CONTROL_C_EXIT

#define __SPRT_EXCEPTION_NONCONTINUABLE      0x1

#define __SPRT_EXCEPTION_EXECUTE_HANDLER      1
#define __SPRT_EXCEPTION_CONTINUE_SEARCH      0
#define __SPRT_EXCEPTION_CONTINUE_EXECUTION (-1)

#define __SPRT_EXCEPTION_MAXIMUM_PARAMETERS 15 // maximum number of exception parameters

/* WoW64 (32-bit-on-64-bit) CONTEXT flags — used by the WoW64 register context. */
#define __SPRT_WOW64_CONTEXT_i386 0x00010000L
#define __SPRT_WOW64_CONTEXT_CONTROL (__SPRT_WOW64_CONTEXT_i386 | 0x00000001L)
#define __SPRT_WOW64_CONTEXT_INTEGER (__SPRT_WOW64_CONTEXT_i386 | 0x00000002L)
#define __SPRT_WOW64_CONTEXT_SEGMENTS (__SPRT_WOW64_CONTEXT_i386 | 0x00000004L)
#define __SPRT_WOW64_CONTEXT_FLOATING_POINT (__SPRT_WOW64_CONTEXT_i386 | 0x00000008L)
#define __SPRT_WOW64_CONTEXT_DEBUG_REGISTERS (__SPRT_WOW64_CONTEXT_i386 | 0x00000010L)
#define __SPRT_WOW64_CONTEXT_EXTENDED_REGISTERS (__SPRT_WOW64_CONTEXT_i386 | 0x00000020L)
#define __SPRT_WOW64_CONTEXT_FULL \
	(__SPRT_WOW64_CONTEXT_CONTROL | __SPRT_WOW64_CONTEXT_INTEGER | __SPRT_WOW64_CONTEXT_SEGMENTS)
#define __SPRT_WOW64_CONTEXT_ALL \
	(__SPRT_WOW64_CONTEXT_CONTROL | __SPRT_WOW64_CONTEXT_INTEGER | __SPRT_WOW64_CONTEXT_SEGMENTS \
			| __SPRT_WOW64_CONTEXT_FLOATING_POINT | __SPRT_WOW64_CONTEXT_DEBUG_REGISTERS \
			| __SPRT_WOW64_CONTEXT_EXTENDED_REGISTERS)

#define __SPRT_WOW64_SIZE_OF_80387_REGISTERS 80
#define __SPRT_WOW64_MAXIMUM_SUPPORTED_EXTENSION 512

// ---- [debugapi] the native debug loop: DEBUG_EVENT and its payload unions ----
#define __SPRT_EXCEPTION_DEBUG_EVENT 1
#define __SPRT_CREATE_THREAD_DEBUG_EVENT 2
#define __SPRT_CREATE_PROCESS_DEBUG_EVENT 3
#define __SPRT_EXIT_THREAD_DEBUG_EVENT 4
#define __SPRT_EXIT_PROCESS_DEBUG_EVENT 5
#define __SPRT_LOAD_DLL_DEBUG_EVENT 6
#define __SPRT_UNLOAD_DLL_DEBUG_EVENT 7
#define __SPRT_OUTPUT_DEBUG_STRING_EVENT 8
#define __SPRT_RIP_EVENT 9
// clang-format on

/* 32-bit (i386) CONTEXT as seen from a 64-bit debugger via Wow64GetThreadContext. */
typedef struct _WOW64_FLOATING_SAVE_AREA {
	DWORD ControlWord;
	DWORD StatusWord;
	DWORD TagWord;
	DWORD ErrorOffset;
	DWORD ErrorSelector;
	DWORD DataOffset;
	DWORD DataSelector;
	BYTE RegisterArea[__SPRT_WOW64_SIZE_OF_80387_REGISTERS];
	DWORD Cr0NpxState;
} WOW64_FLOATING_SAVE_AREA;

typedef struct _WOW64_CONTEXT {
	DWORD ContextFlags;
	DWORD Dr0;
	DWORD Dr1;
	DWORD Dr2;
	DWORD Dr3;
	DWORD Dr6;
	DWORD Dr7;
	WOW64_FLOATING_SAVE_AREA FloatSave;
	DWORD SegGs;
	DWORD SegFs;
	DWORD SegEs;
	DWORD SegDs;
	DWORD Edi;
	DWORD Esi;
	DWORD Ebx;
	DWORD Edx;
	DWORD Ecx;
	DWORD Eax;
	DWORD Ebp;
	DWORD Eip;
	DWORD SegCs;
	DWORD EFlags;
	DWORD Esp;
	DWORD SegSs;
	BYTE ExtendedRegisters[__SPRT_WOW64_MAXIMUM_SUPPORTED_EXTENSION];
} WOW64_CONTEXT, *PWOW64_CONTEXT;

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
#define __SPRT_CONTEXT_AMD64                            0x00100000L
#define __SPRT_CONTEXT_CONTROL         (__SPRT_CONTEXT_AMD64 | 0x00000001L)
#define __SPRT_CONTEXT_INTEGER         (__SPRT_CONTEXT_AMD64 | 0x00000002L)
#define __SPRT_CONTEXT_SEGMENTS        (__SPRT_CONTEXT_AMD64 | 0x00000004L)
#define __SPRT_CONTEXT_FLOATING_POINT  (__SPRT_CONTEXT_AMD64 | 0x00000008L)
#define __SPRT_CONTEXT_DEBUG_REGISTERS (__SPRT_CONTEXT_AMD64 | 0x00000010L)

#define __SPRT_CONTEXT_FULL (__SPRT_CONTEXT_CONTROL | __SPRT_CONTEXT_INTEGER | __SPRT_CONTEXT_FLOATING_POINT)

#define __SPRT_CONTEXT_ALL  (__SPRT_CONTEXT_CONTROL | __SPRT_CONTEXT_INTEGER | __SPRT_CONTEXT_SEGMENTS | __SPRT_CONTEXT_FLOATING_POINT | __SPRT_CONTEXT_DEBUG_REGISTERS)

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

#define __SPRT__JBLEN  16   // x64: jmp_buf == _JBTYPE[16] == 256 bytes == sizeof(_JUMP_BUFFER)

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

// clang-format off
#define __SPRT_CONTEXT_ARM64           0x00400000L
#define __SPRT_CONTEXT_CONTROL         (__SPRT_CONTEXT_ARM64 | 0x00000001L)
#define __SPRT_CONTEXT_INTEGER         (__SPRT_CONTEXT_ARM64 | 0x00000002L)
#define __SPRT_CONTEXT_FLOATING_POINT  (__SPRT_CONTEXT_ARM64 | 0x00000004L)
#define __SPRT_CONTEXT_DEBUG_REGISTERS (__SPRT_CONTEXT_ARM64 | 0x00000008L)

#define __SPRT_CONTEXT_FULL (__SPRT_CONTEXT_CONTROL | __SPRT_CONTEXT_INTEGER | __SPRT_CONTEXT_FLOATING_POINT)

#define __SPRT_CONTEXT_ALL  (__SPRT_CONTEXT_CONTROL | __SPRT_CONTEXT_INTEGER | __SPRT_CONTEXT_FLOATING_POINT | __SPRT_CONTEXT_DEBUG_REGISTERS \
                             | (__SPRT_CONTEXT_ARM64 | 0x10L) /*X18*/ | (__SPRT_CONTEXT_ARM64 | 0x40L) /*FP_LOW*/ | (__SPRT_CONTEXT_ARM64 | 0x80L) /*FP_HIGH*/)
// clang-format on

#define __SPRT__JBLEN  24
#define _JBTYPE __uint64

#define __SPRT_ARM64_MAX_BREAKPOINTS   8
#define __SPRT_ARM64_MAX_WATCHPOINTS   2

typedef union _NEON128 {
	struct {
		ULONGLONG Low;
		LONGLONG High;
	};
	double D[2];
	float S[4];
} NEON128, *PNEON128;

// AArch64 CONTEXT (ARM64_NT_CONTEXT) -- see splat sdk/include/um/winnt.h
typedef struct SPRT_ALIGNAS(16) _CONTEXT {

	//
	// Control flags.
	//

	DWORD ContextFlags;

	//
	// Integer registers
	//

	DWORD Cpsr; // NZVF + DAIF + CurrentEL + SPSel
	union {
		struct {
			DWORD64 X0;
			DWORD64 X1;
			DWORD64 X2;
			DWORD64 X3;
			DWORD64 X4;
			DWORD64 X5;
			DWORD64 X6;
			DWORD64 X7;
			DWORD64 X8;
			DWORD64 X9;
			DWORD64 X10;
			DWORD64 X11;
			DWORD64 X12;
			DWORD64 X13;
			DWORD64 X14;
			DWORD64 X15;
			DWORD64 X16;
			DWORD64 X17;
			DWORD64 X18;
			DWORD64 X19;
			DWORD64 X20;
			DWORD64 X21;
			DWORD64 X22;
			DWORD64 X23;
			DWORD64 X24;
			DWORD64 X25;
			DWORD64 X26;
			DWORD64 X27;
			DWORD64 X28;
			DWORD64 Fp; // x29 frame pointer
			DWORD64 Lr; // x30 link register
		};
		DWORD64 X[31];
	}; // anonymous (ARM64_NT_CONTEXT's DUMMYUNIONNAME) -- exposes Lr/Fp/X[] as Context.<reg>
	DWORD64 Sp;
	DWORD64 Pc;

	//
	// Floating Point/NEON Registers
	//

	NEON128 V[32];
	DWORD Fpcr;
	DWORD Fpsr;

	//
	// Debug registers
	//

	DWORD Bcr[__SPRT_ARM64_MAX_BREAKPOINTS];
	DWORD64 Bvr[__SPRT_ARM64_MAX_BREAKPOINTS];
	DWORD Wcr[__SPRT_ARM64_MAX_WATCHPOINTS];
	DWORD64 Wvr[__SPRT_ARM64_MAX_WATCHPOINTS];

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

typedef struct _DISPATCHER_CONTEXT {
	ULONG_PTR ControlPc;
	ULONG_PTR ImageBase;
	PRUNTIME_FUNCTION FunctionEntry;
	ULONG_PTR EstablisherFrame;
	ULONG_PTR TargetPc;
	PCONTEXT ContextRecord;
	PEXCEPTION_ROUTINE LanguageHandler;
	PVOID HandlerData;
	struct _UNWIND_HISTORY_TABLE *HistoryTable;
	DWORD ScopeIndex;
	BYTE ControlPcIsUnwound;
	BYTE *NonVolatileRegisters;
} DISPATCHER_CONTEXT, *PDISPATCHER_CONTEXT;

// AArch64 _JUMP_BUFFER -- see splat crt/include/setjmp.h (192 bytes, 8-aligned)
struct _JUMP_BUFFER {
	__uint64 Frame;
	__uint64 Reserved;
	__uint64 X19; // x19 -- x28: callee saved registers
	__uint64 X20;
	__uint64 X21;
	__uint64 X22;
	__uint64 X23;
	__uint64 X24;
	__uint64 X25;
	__uint64 X26;
	__uint64 X27;
	__uint64 X28;
	__uint64 Fp; // x29 frame pointer
	__uint64 Lr; // x30 link register
	__uint64 Sp; // x31 stack pointer
	unsigned int Fpcr; // fp control register
	unsigned int Fpsr; // fp status register

	double D[8]; // D8-D15 FP regs
};

#else

#error Not implemented

#endif

typedef struct _EXCEPTION_RECORD {
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	struct _EXCEPTION_RECORD *ExceptionRecord;
	PVOID ExceptionAddress;
	DWORD NumberParameters;
	ULONG_PTR ExceptionInformation[__SPRT_EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD;

#define __SPRT_UNWIND_HISTORY_TABLE_SIZE 12

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
	UNWIND_HISTORY_TABLE_ENTRY Entry[__SPRT_UNWIND_HISTORY_TABLE_SIZE];
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

/* LPCONTEXT alias (PCONTEXT is materialized in abi/context_api.h). */
typedef PCONTEXT LPCONTEXT;


// Top-level (unhandled) exception filter. LPEXCEPTION_POINTERS is the pointer alias
// llvm's Signals.inc uses; PTOP_LEVEL_EXCEPTION_FILTER must carry WINAPI so it binds
// a `LONG WINAPI(...)` filter.
typedef EXCEPTION_POINTERS *LPEXCEPTION_POINTERS;
typedef LONG(WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS *ExceptionInfo);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

typedef struct _EXCEPTION_DEBUG_INFO {
	EXCEPTION_RECORD ExceptionRecord;
	DWORD dwFirstChance;
} EXCEPTION_DEBUG_INFO, *LPEXCEPTION_DEBUG_INFO;

typedef struct _CREATE_THREAD_DEBUG_INFO {
	HANDLE hThread;
	LPVOID lpThreadLocalBase;
	LPTHREAD_START_ROUTINE lpStartAddress;
} CREATE_THREAD_DEBUG_INFO, *LPCREATE_THREAD_DEBUG_INFO;

typedef struct _CREATE_PROCESS_DEBUG_INFO {
	HANDLE hFile;
	HANDLE hProcess;
	HANDLE hThread;
	LPVOID lpBaseOfImage;
	DWORD dwDebugInfoFileOffset;
	DWORD nDebugInfoSize;
	LPVOID lpThreadLocalBase;
	LPTHREAD_START_ROUTINE lpStartAddress;
	LPVOID lpImageName;
	WORD fUnicode;
} CREATE_PROCESS_DEBUG_INFO, *LPCREATE_PROCESS_DEBUG_INFO;

typedef struct _EXIT_THREAD_DEBUG_INFO {
	DWORD dwExitCode;
} EXIT_THREAD_DEBUG_INFO, *LPEXIT_THREAD_DEBUG_INFO;

typedef struct _EXIT_PROCESS_DEBUG_INFO {
	DWORD dwExitCode;
} EXIT_PROCESS_DEBUG_INFO, *LPEXIT_PROCESS_DEBUG_INFO;

typedef struct _LOAD_DLL_DEBUG_INFO {
	HANDLE hFile;
	LPVOID lpBaseOfDll;
	DWORD dwDebugInfoFileOffset;
	DWORD nDebugInfoSize;
	LPVOID lpImageName;
	WORD fUnicode;
} LOAD_DLL_DEBUG_INFO, *LPLOAD_DLL_DEBUG_INFO;

typedef struct _UNLOAD_DLL_DEBUG_INFO {
	LPVOID lpBaseOfDll;
} UNLOAD_DLL_DEBUG_INFO, *LPUNLOAD_DLL_DEBUG_INFO;

typedef struct _OUTPUT_DEBUG_STRING_INFO {
	LPSTR lpDebugStringData;
	WORD fUnicode;
	WORD nDebugStringLength;
} OUTPUT_DEBUG_STRING_INFO, *LPOUTPUT_DEBUG_STRING_INFO;

typedef struct _RIP_INFO {
	DWORD dwError;
	DWORD dwType;
} RIP_INFO, *LPRIP_INFO;

typedef struct _DEBUG_EVENT {
	DWORD dwDebugEventCode;
	DWORD dwProcessId;
	DWORD dwThreadId;
	union {
		EXCEPTION_DEBUG_INFO Exception;
		CREATE_THREAD_DEBUG_INFO CreateThread;
		CREATE_PROCESS_DEBUG_INFO CreateProcessInfo;
		EXIT_THREAD_DEBUG_INFO ExitThread;
		EXIT_PROCESS_DEBUG_INFO ExitProcess;
		LOAD_DLL_DEBUG_INFO LoadDll;
		UNLOAD_DLL_DEBUG_INFO UnloadDll;
		OUTPUT_DEBUG_STRING_INFO DebugString;
		RIP_INFO RipInfo;
	} u;
} DEBUG_EVENT, *LPDEBUG_EVENT;

#endif // SPRT_WRAPPERS_WINDOWS_ABI_CONTEXT_API_H_
