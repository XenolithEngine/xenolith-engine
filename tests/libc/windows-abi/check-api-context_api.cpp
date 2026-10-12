// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/context_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/context_api.h>
#include "api_check.h"

#include <windows.h>

// skipped (not functions / not in the public SDK headers): NtContinueEx, __attribute__, longjmp
SPRT_FN(RtlCaptureContext);
SPRT_FN(GetThreadContext);
SPRT_FN(SetThreadContext);
SPRT_FN(Wow64GetThreadContext);
SPRT_FN(Wow64SetThreadContext);
SPRT_FN(RtlRestoreContext);
SPRT_FN(RtlLookupFunctionEntry);
SPRT_FN(RtlAddFunctionTable);
SPRT_FN(RtlDeleteFunctionTable);
SPRT_FN(RtlUnwindEx);
SPRT_FN(RtlVirtualUnwind);
SPRT_FN(RtlCaptureStackBackTrace);
SPRT_FN(AddVectoredExceptionHandler);
SPRT_FN(RemoveVectoredExceptionHandler);
SPRT_FN(AddVectoredContinueHandler);
SPRT_FN(RemoveVectoredContinueHandler);
SPRT_FN(WaitForDebugEvent);
SPRT_FN(ContinueDebugEvent);
SPRT_FN(DebugActiveProcess);
SPRT_FN(DebugActiveProcessStop);
SPRT_FN(DebugBreakProcess);
SPRT_FN(SetUnhandledExceptionFilter);
SPRT_FN(RaiseException);
