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

#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/context_api.h>

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_setjmp.h>

#include <sprt/cxx/new>       // std::bad_alloc / std::bad_array_new_length
#include <sprt/cxx/typeinfo>  // std::bad_cast / std::bad_typeid
#include <sprt/cxx/exception> // std::terminate

#include <stdio.h>
#include <string.h>

#include "../../include/__impl_libc.h"
#include "dll/dllloader.h"

/*
	Windows excemtion handling integration.

	We don't use c++ exceptions in SPRT, but we must handle extended longjmp,
	which on Windows implemented through SEH exceptions.

	Also we setup common VEH handler, which does noting but captures unhandled exceptions
	with the breakpoint.
*/

static_assert(sizeof(__SPRT_ID(native_jmp_buf)) == sizeof(_JUMP_BUFFER));
static_assert(alignof(__SPRT_ID(native_jmp_buf)) == alignof(_JUMP_BUFFER));

namespace sprt {

static decltype(&setjmp) s_setjmp;
static decltype(&longjmp) s_longjmp;
static decltype(&RtlCaptureContext) s_RtlCaptureContext;
static decltype(&RtlRestoreContext) s_RtlRestoreContext;
static decltype(&RtlUnwindEx) s_RtlUnwindEx;
static decltype(&RtlVirtualUnwind) s_RtlVirtualUnwind;

#define EXCEPTION_UNWINDING 0x2             // Unwind is in progress
#define EXCEPTION_EXIT_UNWIND 0x4           // Exit unwind is in progress

struct UnwindMapEntry {
	int32_t state;
	uint32_t action;
};

struct IpToStateMapEntry {
	uint32_t ip;
	int32_t state;
};

struct FuncInfo {
	uint32_t magic	   : 29;
	uint32_t bbt_flags : 3;
	int32_t last_state;

	uint32_t rva_unwind_table;
	int32_t ntryblocks;
	uint32_t rva_tryblock_map;
	int32_t nips;
	uint32_t rva_ipmap;
	uint32_t rel_unwind_helpers; // Frame-relative
	uint32_t rva_expect_list;
	uint32_t flags;
};

static void do_unwind(PVOID EstablisherFrame, PDISPATCHER_CONTEXT DispatcherContext, FuncInfo *desc,
		int targetLevel) {
	auto unwind_table = reinterpret_cast<const UnwindMapEntry *>(
			(const uint8_t *)DispatcherContext->ImageBase + desc->rva_unwind_table);

	int *unwind_help = (int *)((uintptr_t)EstablisherFrame + desc->rel_unwind_helpers);
	int initLevel = unwind_help[0];

	if (initLevel == -2) {
		auto ipmap = reinterpret_cast<const IpToStateMapEntry *>(
				(const uint8_t *)DispatcherContext->ImageBase + desc->rva_ipmap);

		// Bound the scan by the (signed) entry count; a malformed/negative nips
		// must not be promoted to a huge unsigned bound and read past the table.
		int32_t i;
		for (i = 0; i < desc->nips; i++) {
			if (DispatcherContext->ImageBase + ipmap[i].ip > DispatcherContext->ControlPc) {
				break;
			}
		}
		initLevel = i ? ipmap[i - 1].state : -1;
	}

	while (initLevel > targetLevel) {
		if (initLevel < 0 || initLevel >= desc->last_state) {
			break;
		}
		if (unwind_table[initLevel].action) {
			auto handler = reinterpret_cast<void (*)()>(
					(uint8_t *)DispatcherContext->ImageBase + unwind_table[initLevel].action);

			handler();
		}
		initLevel = unwind_table[initLevel].state;
	}

	// mark for next handlers, on what level we stopper at
	unwind_help[0] = initLevel;
}

__SPRT_C_FUNC EXCEPTION_DISPOSITION __CxxFrameHandler3(PEXCEPTION_RECORD ExceptionRecord,
		PVOID EstablisherFrame, PCONTEXT ContextRecord, PDISPATCHER_CONTEXT DispatcherContext) {
	if (ExceptionRecord->ExceptionCode != STATUS_LONGJUMP) {
		// we do not support exceptions, but we need proper handling for longjmp
		return ExceptionContinueSearch;
	}

	FuncInfo *desc = reinterpret_cast<FuncInfo *>(
			(uint8_t *)DispatcherContext->ImageBase + *(UINT *)DispatcherContext->HandlerData);

	// VC8 format (produced by curent clang at time of writing)
	if (0x1993'0522 != desc->magic) {
		return ExceptionContinueSearch;
	}

	// check if unwind is actuually requested
	if (ExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) {
		do_unwind(EstablisherFrame, DispatcherContext, desc, -1);
	}

	return ExceptionContinueSearch;
}


__SPRT_ID(setjmp_fn) get_setjmp_fn() { return reinterpret_cast<__SPRT_ID(setjmp_fn)>(s_setjmp); }

__SPRT_ID(sigsetjmp_fn) get_sigsetjmp_fn() {
	return reinterpret_cast<__SPRT_ID(setjmp_fn)>(s_setjmp);
}

__SPRT_C_FUNC __attribute__((noreturn)) void longjmp(_JUMP_BUFFER *buf, int value) {
	CONTEXT ctx;
	ctx.ContextFlags = CONTEXT_FULL;
	s_RtlCaptureContext(&ctx);

	UNWIND_HISTORY_TABLE historyTable;
	::memset(&historyTable, 0, sizeof(historyTable));

	EXCEPTION_RECORD exceptionRecord;
	::memset(&exceptionRecord, 0, sizeof(exceptionRecord));
	exceptionRecord.ExceptionCode = STATUS_LONGJUMP;
	exceptionRecord.NumberParameters = 1;
	exceptionRecord.ExceptionInformation[0] = (ULONG_PTR)buf;

#if defined(_M_ARM64) || defined(__aarch64__)
	// AArch64 _JUMP_BUFFER: Sp is the target frame, Lr the resume address
	s_RtlUnwindEx((VOID *)buf->Sp, (VOID *)buf->Lr, &exceptionRecord, (void *)(intptr_t)value,
			&ctx, &historyTable);
#else
	s_RtlUnwindEx((VOID *)buf->Rsp, (VOID *)buf->Rip, &exceptionRecord, (void *)(intptr_t)value,
			&ctx, &historyTable);
#endif

	__builtin_unreachable();
}

static PVOID g_vehHandler = NULL;
static BOOL g_exceptionInitialized = FALSE;

// Exception handler function
static LONG __libc_exception_handler(PEXCEPTION_POINTERS ExceptionInfo) {
	DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;

	// Handle common exceptions
	switch (exceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		// Access violation - could be null pointer dereference
		//crt_log_error("Access Violation at 0x%p",
		//		ExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
		break;

	case EXCEPTION_ILLEGAL_INSTRUCTION:
		// Invalid instruction
		//crt_log_error("Illegal Instruction");
		break;

	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		// Division by zero
		//crt_log_error("Division by Zero");
		break;

	case EXCEPTION_STACK_OVERFLOW:
		// Stack overflow
		//crt_log_error("Stack Overflow");
		break;

	case 0xE06D'7363: // C++ exception
		// C++ exception translation
		return EXCEPTION_CONTINUE_SEARCH;

	default: break;
	}

	// For fatal exceptions, call default handler or terminate
	if (exceptionCode == EXCEPTION_ACCESS_VIOLATION || exceptionCode == EXCEPTION_STACK_OVERFLOW) {
		// Could call SetUnhandledExceptionFilter or generate minidump
		return EXCEPTION_EXECUTE_HANDLER;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

bool __init_exceptions(void) {
	if (g_exceptionInitialized) {
		return TRUE;
	}

	/*
		Note that all of this functions are preloaded, startup will fail otherwise
	*/

	auto loader = DllLoader::get();

	s_setjmp = reinterpret_cast<decltype(s_setjmp)>(loader->ntdll._setjmpex.fn);
	s_longjmp = reinterpret_cast<decltype(s_longjmp)>(loader->ntdll.longjmp.fn);
	s_RtlCaptureContext =
			reinterpret_cast<decltype(s_RtlCaptureContext)>(loader->ntdll.RtlCaptureContext.fn);
	s_RtlRestoreContext =
			reinterpret_cast<decltype(s_RtlRestoreContext)>(loader->ntdll.RtlRestoreContext.fn);
	s_RtlUnwindEx = reinterpret_cast<decltype(s_RtlUnwindEx)>(loader->ntdll.RtlUnwindEx.fn);
	s_RtlVirtualUnwind =
			reinterpret_cast<decltype(s_RtlVirtualUnwind)>(loader->ntdll.RtlVirtualUnwind.fn);

	// Register VEH handler (as first handler to catch everything)
	g_vehHandler = AddVectoredExceptionHandler(1, __libc_exception_handler);

	if (!g_vehHandler) {
		return FALSE;
	}

	g_exceptionInitialized = TRUE;

	return TRUE;
}

void __cleanup_exceptions(void) {
	if (g_vehHandler) {
		RemoveVectoredExceptionHandler(g_vehHandler);
		g_vehHandler = NULL;
	}
	g_exceptionInitialized = FALSE;
}

} // namespace sprt

// =========================================================================
//  C++ exception-ABI entities the ported libc++ needs on the MSVC ABI.
//
//  On the Itanium targets these come from libc++abi; the MSVC ABI has no
//  libc++abi, so the runtime supplies them here (the same place std::exception /
//  std::bad_exception / std::type_info are provided in windows/libcxx.cc). They
//  are the canonical, unversioned std:: entities (libc++ declares them
//  EXPORTED_FROM_ABI, not in the versioned __sprt namespace), so define them in
//  the canonical namespace std to match the manglings libc++ emits.
// =========================================================================

namespace std {

// <new> allocation-failure exceptions.
bad_alloc::bad_alloc() noexcept { }
bad_alloc::~bad_alloc() noexcept { }
const char *bad_alloc::what() const noexcept { return "std::bad_alloc"; }

bad_array_new_length::bad_array_new_length() noexcept { }
bad_array_new_length::~bad_array_new_length() noexcept { }
const char *bad_array_new_length::what() const noexcept { return "std::bad_array_new_length"; }

// <typeinfo> RTTI exceptions.
bad_cast::bad_cast() noexcept { }
bad_cast::~bad_cast() noexcept { }
const char *bad_cast::what() const noexcept { return "std::bad_cast"; }

bad_typeid::bad_typeid() noexcept { }
bad_typeid::~bad_typeid() noexcept { }
const char *bad_typeid::what() const noexcept { return "std::bad_typeid"; }

} // namespace std

// The MSVC C++ ABI throw entry point. clang lowers every `throw` to a call to this
// symbol. The runtime is built without a C++ exception runtime, so a thrown exception
// can never be delivered to a catch clause: the only correct behaviour is to terminate
// (identical to rethrow_exception on the no-EH exception_ptr, see SPRTCxxException.cpp).
// The parameters (the exception object and its _ThrowInfo) are irrelevant here; using
// void* keeps this free of any dependency on the MSVC vcruntime throw-info layout.
extern "C" [[noreturn]] void _CxxThrowException(void *__exception_object, void *__throw_info) {
	(void)__exception_object;
	(void)__throw_info;
	std::terminate();
}
