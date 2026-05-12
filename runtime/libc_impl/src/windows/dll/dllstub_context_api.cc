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

#include <sprt/wrappers/windows/context_api.h>

#include "dllloader.h"

extern "C" {

int printf(const char *, ...) __SPRT_NOEXCEPT;
__SPRT_NORETURN void abort() __SPRT_NOEXCEPT;

VOID RtlCaptureContext(PCONTEXT ContextRecord) {
	printf("FATAL: RtlCaptureContext function used directly, without SPRT wrapper. This is "
		   "prohibited, as it leads to stack smashing or other similar errors\n");
	abort();
}

VOID RtlRestoreContext(PCONTEXT ContextRecord, _EXCEPTION_RECORD *ExceptionRecord) {
	auto loader = sprt::DllLoader::get();
	loader->ntdll.call<decltype(&RtlRestoreContext)>(loader->ntdll.RtlRestoreContext, ContextRecord,
			ExceptionRecord);
}

PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64 ControlPc, PDWORD64 ImageBase,
		PUNWIND_HISTORY_TABLE HistoryTable) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RtlLookupFunctionEntry)>(
			loader->kernel32.RtlLookupFunctionEntry, ControlPc, ImageBase, HistoryTable);
}

VOID RtlUnwindEx(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord,
		PVOID ReturnValue, PCONTEXT ContextRecord, PUNWIND_HISTORY_TABLE HistoryTable) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&RtlUnwindEx)>(loader->kernel32.RtlUnwindEx, TargetFrame,
			TargetIp, ExceptionRecord, ReturnValue, ContextRecord, HistoryTable);
}

PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc,
		PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID *HandlerData,
		PDWORD64 EstablisherFrame, PVOID ContextPointers) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RtlVirtualUnwind)>(loader->kernel32.RtlVirtualUnwind,
			HandlerType, ImageBase, ControlPc, FunctionEntry, ContextRecord, HandlerData,
			EstablisherFrame, ContextPointers);
}

WORD RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture, PVOID *BackTrace,
		PDWORD BackTraceHash) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RtlCaptureStackBackTrace)>(
			loader->kernel32.RtlCaptureStackBackTrace, FramesToSkip, FramesToCapture, BackTrace,
			BackTraceHash);
}

WINAPI PVOID AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&AddVectoredExceptionHandler)>(
			loader->kernel32.AddVectoredExceptionHandler, First, Handler);
}

WINAPI ULONG RemoveVectoredExceptionHandler(PVOID Handle) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RemoveVectoredExceptionHandler)>(
			loader->kernel32.RemoveVectoredExceptionHandler, Handle);
}

WINAPI PVOID AddVectoredContinueHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&AddVectoredContinueHandler)>(
			loader->kernel32.AddVectoredContinueHandler, First, Handler);
}

WINAPI ULONG RemoveVectoredContinueHandler(PVOID Handle) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RemoveVectoredContinueHandler)>(
			loader->kernel32.RemoveVectoredContinueHandler, Handle);
}

WINAPI NTSTATUS NtContinueEx(CONTEXT *context, void *args) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtContinueEx)>(loader->ntdll.NtContinueEx, context, args);
}

__attribute__((returns_twice)) int setjmp(_JUMP_BUFFER *_Buf, void *ptr) {
	printf("FATAL: setjmp function used directly, without SPRT wrapper. This is prohibited, as it "
		   "leads to stack smashing or other similar errors\n");
	abort();
}

// longjmp itself replaced with SPRT's wrapper
/*__SPRT_NORETURN void __cdecl longjmp(_JUMP_BUFFER *_Buf, int _Value) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	loader->ntdll.call<decltype(&longjmp)>(loader->ntdll.longjmp, _Buf, _Value);
	__builtin_unreachable();
}*/

EXCEPTION_DISPOSITION __C_specific_handler(struct _EXCEPTION_RECORD *ExceptionRecord,
		void *EstablisherFrame, struct _CONTEXT *ContextRecord,
		struct _DISPATCHER_CONTEXT *DispatcherContext) {
	auto loader = sprt::DllLoader::get();
	auto ptr = reinterpret_cast<decltype(&__C_specific_handler)>(
			loader->ntdll.__C_specific_handler.fn);
	return ptr(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}
}
