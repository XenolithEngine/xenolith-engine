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

#include <sprt/wrappers/windows/debug_api.h>

#include "dllloader.h"

extern "C" {
WINAPI DWORD SymSetOptions(DWORD SymOptions) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymSetOptions, SymOptions);
}

WINAPI BOOL SymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymInitialize, hProcess, UserSearchPath,
			fInvadeProcess);
}

WINAPI DWORD SymGetOptions(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymGetOptions);
}

WINAPI BOOL SymCleanup(HANDLE hProcess) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymCleanup, hProcess);
}

WINAPI BOOL StackWalk64(DWORD MachineType, HANDLE hProcess, HANDLE hThread,
		LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
		PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
		PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
		PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
		PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, StackWalk64, MachineType, hProcess, hThread,
			StackFrame, ContextRecord, ReadMemoryRoutine, FunctionTableAccessRoutine,
			GetModuleBaseRoutine, TranslateAddress);
}

WINAPI BOOL SymGetSymFromAddr64(HANDLE hProcess, DWORD64 qwAddr, PDWORD64 pdwDisplacement,
		PIMAGEHLP_SYMBOL64 Symbol) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymGetSymFromAddr64, hProcess, qwAddr,
			pdwDisplacement, Symbol);
}

WINAPI BOOL SymGetLineFromAddr64(HANDLE hProcess, DWORD64 qwAddr, PDWORD pdwDisplacement,
		PIMAGEHLP_LINE64 Line64) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, SymGetLineFromAddr64, hProcess, qwAddr,
			pdwDisplacement, Line64);
}

WINAPI DWORD UnDecorateSymbolName(PCSTR name, PSTR outputString, DWORD maxStringLength,
		DWORD flags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, dbghelp, UnDecorateSymbolName, name, outputString,
			maxStringLength, flags);
}
}
