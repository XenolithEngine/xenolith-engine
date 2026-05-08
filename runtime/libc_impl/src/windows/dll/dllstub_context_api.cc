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

VOID RtlCaptureContext(PCONTEXT ContextRecord) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&RtlCaptureContext)>(loader->kernel32.RtlCaptureContext,
			ContextRecord);
}

VOID RtlRestoreContext(PCONTEXT ContextRecord, _EXCEPTION_RECORD *ExceptionRecord) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&RtlRestoreContext)>(loader->kernel32.RtlRestoreContext,
			ContextRecord, ExceptionRecord);
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

int setjmp(jmp_buf _Buf) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&setjmp)>(loader->ntdll._setjmp, _Buf);
}

__SPRT_NORETURN void __cdecl longjmp(jmp_buf _Buf, int _Value) __SPRT_NOEXCEPT {
	auto loader = sprt::DllLoader::get();
	loader->ntdll.call<decltype(&longjmp)>(loader->ntdll.longjmp, _Buf, _Value);
	__builtin_unreachable();
}
}
