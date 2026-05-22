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

#include "dllloader.h"

extern "C" {

// ---- Dynamic Linkage Functions (kernel32/kernelbase) ----

SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&VirtualQuery)>(loader->kernel32.VirtualQuery, lpAddress,
			lpBuffer, dwLength);
}

__declspec(noreturn) VOID ExitProcess(UINT uExitCode) {
	auto loader = sprt::DllLoader::get();
	loader->__ExitProcess(uExitCode);
	__builtin_unreachable();
}

HMODULE LoadLibraryW(LPCWSTR lpLibFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->__LoadLibraryW(lpLibFileName);
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LoadLibraryA)>(loader->kernel32.LoadLibraryA,
			lpLibFileName);
}

HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LoadLibraryExW)>(loader->kernel32.LoadLibraryExW,
			lpLibFileName, hFile, dwFlags);
}

BOOL FreeLibrary(HMODULE hModule) {
	auto loader = sprt::DllLoader::get();
	return loader->__FreeLibrary(hModule);
}

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
	auto loader = sprt::DllLoader::get();
	return loader->__GetProcAddress(hModule, lpProcName);
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetModuleHandleW)>(loader->kernel32.GetModuleHandleW,
			lpModuleName);
}

BOOL GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, PHMODULE phModule) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetModuleHandleExW)>(loader->kernel32.GetModuleHandleExW,
			dwFlags, lpModuleName, phModule);
}

DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetModuleFileNameW)>(loader->kernel32.GetModuleFileNameW,
			hModule, lpFilename, nSize);
}

WINAPI PVOID RtlPcToFileHeader(PVOID PcValue, PVOID *BaseOfImage) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RtlPcToFileHeader)>(loader->kernel32.RtlPcToFileHeader,
			PcValue, BaseOfImage);
}

} // extern "C"
