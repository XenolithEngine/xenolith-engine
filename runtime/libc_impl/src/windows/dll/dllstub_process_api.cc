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

#include <sprt/wrappers/windows/process_api.h>

#include "dllloader.h"

extern "C" {

// ---- Process Information Functions (kernel32) ----

HANDLE GetCurrentProcess(void) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCurrentProcess)>(loader->kernel32.GetCurrentProcess);
}

DWORD GetCurrentProcessId(void) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCurrentProcessId)>(
			loader->kernel32.GetCurrentProcessId);
}

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&OpenProcess)>(loader->kernel32.OpenProcess,
			dwDesiredAccess, bInheritHandle, dwProcessId);
}

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetExitCodeProcess)>(loader->kernel32.GetExitCodeProcess,
			hProcess, lpExitCode);
}

DWORD GetPriorityClass(HANDLE hProcess) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetPriorityClass)>(loader->kernel32.GetPriorityClass,
			hProcess);
}

// ---- Processor Synchronization (ntdll) ----

void _mm_pause(void);

VOID YieldProcessor() { _mm_pause(); }

// ---- Process Thread Attribute List Functions (kernel32) ----

BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
		DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&InitializeProcThreadAttributeList)>(
			loader->kernel32.InitializeProcThreadAttributeList, lpAttributeList, dwAttributeCount,
			dwFlags, lpSize);
}

BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList, DWORD dwFlags,
		DWORD_PTR Attribute, PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue,
		PSIZE_T lpReturnSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&UpdateProcThreadAttribute)>(
			loader->kernel32.UpdateProcThreadAttribute, lpAttributeList, dwFlags, Attribute,
			lpValue, cbSize, lpPreviousValue, lpReturnSize);
}

VOID DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&DeleteProcThreadAttributeList)>(
			loader->kernel32.DeleteProcThreadAttributeList, lpAttributeList);
}

// ---- Command Line Function (kernel32) ----

LPWSTR GetCommandLineW(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCommandLineW)>(loader->kernel32.GetCommandLineW);
}

// ---- Process Creation Function (kernel32) ----

BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
		LPPROCESS_INFORMATION lpProcessInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateProcessW)>(loader->kernel32.CreateProcessW,
			lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
			bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
			lpProcessInformation);
}

} // extern "C"
