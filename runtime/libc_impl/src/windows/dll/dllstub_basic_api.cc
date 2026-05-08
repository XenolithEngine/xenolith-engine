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

#include <sprt/wrappers/windows/basic_api.h>

#include "dllloader.h"

extern "C" {

// ---- Local Memory Functions (kernel32) ----

HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LocalAlloc)>(loader->kernel32.LocalAlloc, uFlags,
			uBytes);
}

HLOCAL LocalFree(HLOCAL hMem) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LocalFree)>(loader->kernel32.LocalFree, hMem);
}

// ---- Heap Functions (kernelbase/kernel32) ----

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&HeapAlloc)>(loader->kernelbase.HeapAlloc, hHeap,
			dwFlags, dwBytes);
}

LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&HeapReAlloc)>(loader->kernelbase.HeapReAlloc, hHeap,
			dwFlags, lpMem, dwBytes);
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&HeapFree)>(loader->kernelbase.HeapFree, hHeap, dwFlags,
			lpMem);
}

SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&HeapSize)>(loader->kernelbase.HeapSize, hHeap, dwFlags,
			lpMem);
}

HANDLE GetProcessHeap(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetProcessHeap)>(loader->kernelbase.GetProcessHeap);
}

// ---- Virtual Memory Functions (kernelbase) ----

LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&VirtualAlloc)>(loader->kernelbase.VirtualAlloc,
			lpAddress, dwSize, flAllocationType, flProtect);
}

BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&VirtualProtect)>(loader->kernelbase.VirtualProtect,
			lpAddress, dwSize, flNewProtect, lpflOldProtect);
}

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&VirtualFree)>(loader->kernelbase.VirtualFree,
			lpAddress, dwSize, dwFreeType);
}

DWORD DiscardVirtualMemory(PVOID VirtualAddress, SIZE_T Size) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&DiscardVirtualMemory)>(
			loader->kernelbase.DiscardVirtualMemory, VirtualAddress, Size);
}

// ---- Handle Functions (kernelbase/kernel32) ----

BOOL CloseHandle(HANDLE hObject) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CloseHandle)>(loader->kernelbase.CloseHandle, hObject);
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetHandleInformation)>(
			loader->kernelbase.GetHandleInformation, hObject, lpdwFlags);
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetHandleInformation)>(
			loader->kernelbase.SetHandleInformation, hObject, dwMask, dwFlags);
}

BOOL DuplicateHandle(HANDLE hSourceProcess, HANDLE hSourceHandle, HANDLE hTargetProcess,
		PHANDLE lpDuplicateHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&DuplicateHandle)>(loader->kernelbase.DuplicateHandle,
			hSourceProcess, hSourceHandle, hTargetProcess, lpDuplicateHandle, dwDesiredAccess,
			bInheritHandle, dwOptions);
}

// ---- Virtual Lock Functions (kernelbase/ntdll) ----

BOOL VirtualLock(LPVOID lpAddress, SIZE_T dwSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&VirtualLock)>(loader->kernelbase.VirtualLock,
			lpAddress, dwSize);
}

BOOL VirtualUnlock(LPVOID lpAddress, SIZE_T dwSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&VirtualUnlock)>(loader->kernelbase.VirtualUnlock,
			lpAddress, dwSize);
}

// ---- Environment Functions (kernelbase/kernel32) ----

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetEnvironmentVariableA)>(
			loader->kernelbase.GetEnvironmentVariableA, lpName, lpBuffer, nSize);
}

BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetEnvironmentVariableA)>(
			loader->kernelbase.SetEnvironmentVariableA, lpName, lpValue);
}

// ---- Sleep Function (kernel32) ----

VOID Sleep(DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&Sleep)>(loader->kernel32.Sleep, dwMilliseconds);
}

// ---- Address Wait/Wake Functions (kernelbase) ----

BOOL WaitOnAddress(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize,
		DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&WaitOnAddress)>(loader->kernelbase.WaitOnAddress,
			Address, CompareAddress, AddressSize, dwMilliseconds);
}

VOID WakeByAddressSingle(PVOID Address) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&WakeByAddressSingle)>(loader->kernelbase.WakeByAddressSingle,
			Address);
}

VOID WakeByAddressAll(PVOID Address) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&WakeByAddressAll)>(loader->kernelbase.WakeByAddressAll,
			Address);
}

// ---- Error Handling Functions (kernelbase/kernel32) ----

DWORD GetLastError(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetLastError)>(loader->kernelbase.GetLastError);
}

VOID SetLastError(DWORD dwErrCode) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&SetLastError)>(loader->kernelbase.SetLastError, dwErrCode);
}

// ---- Debugger Detection (kernelbase/kernel32) ----

BOOL IsDebuggerPresent() {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&IsDebuggerPresent)>(
			loader->kernelbase.IsDebuggerPresent);
}

HANDLE GetStdHandle(DWORD nStdHandle) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetStdHandle)>(loader->kernelbase.GetStdHandle,
			nStdHandle);
}

BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput,
		PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetConsoleScreenBufferInfo)>(
			loader->kernelbase.GetConsoleScreenBufferInfo, hConsoleOutput,
			lpConsoleScreenBufferInfo);
}

BOOL SetConsoleCP(UINT wCodePageID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetConsoleCP)>(loader->kernelbase.SetConsoleCP,
			wCodePageID);
}

BOOL SetConsoleOutputCP(UINT wCodePageID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetConsoleOutputCP)>(
			loader->kernelbase.SetConsoleOutputCP, wCodePageID);
}

BOOL GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetConsoleMode)>(loader->kernelbase.GetConsoleMode,
			hConsoleHandle, lpMode);
}

BOOL SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetConsoleMode)>(loader->kernelbase.SetConsoleMode,
			hConsoleHandle, dwMode);
}

WINAPI DWORD GetCurrentProcessorNumber(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetCurrentProcessorNumber)>(
			loader->kernel32.GetCurrentProcessorNumber);
}

} // extern "C"
