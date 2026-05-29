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

#include <sprt/wrappers/windows/windows.h>

#include "dllloader.h"

extern "C" {
LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapAlloc, hHeap, dwFlags, dwBytes);
}

LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapReAlloc, hHeap, dwFlags, lpMem, dwBytes);
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapFree, hHeap, dwFlags, lpMem);
}

SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapSize, hHeap, dwFlags, lpMem);
}

HANDLE HeapCreate(DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapCreate, flOptions, dwInitialSize,
			dwMaximumSize);
}

BOOL HeapDestroy(HANDLE hHeap) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapDestroy, hHeap);
}

BOOL HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapValidate, hHeap, dwFlags, lpMem);
}

SIZE_T HeapCompact(HANDLE hHeap, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, HeapCompact, hHeap, dwFlags);
}

HANDLE GetProcessHeap(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetProcessHeap);
}

LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, VirtualAlloc, lpAddress, dwSize, flAllocationType,
			flProtect);
}

BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, VirtualProtect, lpAddress, dwSize, flNewProtect,
			lpflOldProtect);
}

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, VirtualFree, lpAddress, dwSize, dwFreeType);
}

DWORD DiscardVirtualMemory(PVOID VirtualAddress, SIZE_T Size) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, DiscardVirtualMemory, VirtualAddress, Size);
}

BOOL CloseHandle(HANDLE hObject) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CloseHandle, hObject);
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetHandleInformation, hObject, lpdwFlags);
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetHandleInformation, hObject, dwMask, dwFlags);
}

BOOL DuplicateHandle(HANDLE hSourceProcess, HANDLE hSourceHandle, HANDLE hTargetProcess,
		PHANDLE lpDuplicateHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, DuplicateHandle, hSourceProcess, hSourceHandle,
			hTargetProcess, lpDuplicateHandle, dwDesiredAccess, bInheritHandle, dwOptions);
}

BOOL VirtualLock(LPVOID lpAddress, SIZE_T dwSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, VirtualLock, lpAddress, dwSize);
}

BOOL VirtualUnlock(LPVOID lpAddress, SIZE_T dwSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, VirtualUnlock, lpAddress, dwSize);
}

BOOL WaitOnAddress(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize,
		DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, WaitOnAddress, Address, CompareAddress,
			AddressSize, dwMilliseconds);
}

VOID WakeByAddressSingle(PVOID Address) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernelbase, WakeByAddressSingle, Address);
}

VOID WakeByAddressAll(PVOID Address) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernelbase, WakeByAddressAll, Address);
}

DWORD GetLastError(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetLastError);
}

VOID SetLastError(DWORD dwErrCode) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernelbase, SetLastError, dwErrCode);
}

BOOL IsDebuggerPresent() {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, IsDebuggerPresent);
}

HANDLE GetStdHandle(DWORD nStdHandle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetStdHandle, nStdHandle);
}

BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput,
		PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetConsoleScreenBufferInfo, hConsoleOutput,
			lpConsoleScreenBufferInfo);
}

BOOL SetConsoleCP(UINT wCodePageID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetConsoleCP, wCodePageID);
}

BOOL SetConsoleOutputCP(UINT wCodePageID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetConsoleOutputCP, wCodePageID);
}

BOOL GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetConsoleMode, hConsoleHandle, lpMode);
}

BOOL SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetConsoleMode, hConsoleHandle, dwMode);
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateFileW, lpFileName, dwDesiredAccess,
			dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
			hTemplateFile);
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateFileA, lpFileName, dwDesiredAccess,
			dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
			hTemplateFile);
}

BOOL FlushFileBuffers(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, FlushFileBuffers, hFile);
}

DWORD GetFinalPathNameByHandleW(HANDLE hHandle, LPWSTR lpFilePath, DWORD nSize, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFinalPathNameByHandleW, hHandle, lpFilePath,
			nSize, dwFlags);
}

BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetFileAttributesW, lpFileName, dwFileAttributes);
}

BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetFileAttributesA, lpFileName, dwFileAttributes);
}

BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateDirectoryW, lpPathName,
			lpSecurityAttributes);
}

DWORD GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpTemplateFile, UINT uUnique,
		LPWSTR lpTempFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetTempFileNameW, lpPathName, lpTemplateFile,
			uUnique, lpTempFileName);
}

BOOL GetFileInformationByHandleEx(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
		LPVOID lpFileInformation, DWORD dwBufferSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileInformationByHandleEx, hFile,
			FileInformationClass, lpFileInformation, dwBufferSize);
}

DWORD GetLogicalDrives() {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetLogicalDrives);
}

BOOL PathIsRelativeW(LPCWSTR pszPath) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, PathIsRelativeW, pszPath);
}

DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetCurrentDirectoryW, nBufferLength, lpBuffer);
}

BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileInformationByHandle, hFile,
			lpFileInformation);
}

HANDLE ReOpenFile(HANDLE hOriginalFile, DWORD dwDesiredAccess, DWORD dwShareMode,
		DWORD dwFlagsAndAttributes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, ReOpenFile, hOriginalFile, dwDesiredAccess,
			dwShareMode, dwFlagsAndAttributes);
}

HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateFileMappingW, hFile, lpFileMappingAttributes,
			flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
		DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, MapViewOfFile, hFileMappingObject, dwDesiredAccess,
			dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, UnmapViewOfFile, lpBaseAddress);
}

BOOL FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, FlushViewOfFile, lpBaseAddress,
			dwNumberOfBytesToFlush);
}

BOOL DeleteFileW(LPCWSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, DeleteFileW, lpFileName);
}

WINAPI BOOL DeleteFileA(LPCSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, DeleteFileA, lpFileName);
}

BOOL RemoveDirectoryW(LPCWSTR lpPathName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, RemoveDirectoryW, lpPathName);
}

DWORD GetFileAttributesW(LPCWSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileAttributesW, lpFileName);
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileAttributesA, lpFileName);
}

BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
		LPVOID lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileAttributesExW, lpFileName, fInfoLevelId,
			lpFileInformation);
}

BOOL GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
		LPVOID lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetFileAttributesExA, lpFileName, fInfoLevelId,
			lpFileInformation);
}

BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, MoveFileExW, lpExistingFileName, lpNewFileName,
			dwFlags);
}

BOOL SetCurrentDirectoryW(LPCWSTR lpPathName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetCurrentDirectoryW, lpPathName);
}

BOOL GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize,
		LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
		LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetVolumeInformationW, lpRootPathName,
			lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength,
			lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

BOOL GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller,
		PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetDiskFreeSpaceExW, lpDirectoryName,
			lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

BOOL CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateHardLinkW, lpFileName, lpExistingFileName,
			lpSecurityAttributes);
}

BOOLEAN CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateSymbolicLinkW, lpSymlinkFileName,
			lpTargetFileName, dwFlags);
}

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes,
		DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreatePipe, hReadPipe, hWritePipe,
			lpPipeAttributes, nSize);
}

DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetTempPathW, nBufferLength, lpBuffer);
}

WINAPI DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, GetTempPathA, nBufferLength, lpBuffer);
}

HRESULT PathAllocCanonicalize(PCWSTR pszPathIn, ULONG dwFlags, PWSTR *ppszPathOut) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, PathAllocCanonicalize, pszPathIn, dwFlags,
			ppszPathOut);
}

DWORD FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
		LPWSTR lpBuffer, DWORD nSize, __builtin_va_list *Arguments) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, FormatMessageW, dwFlags, lpSource, dwMessageId,
			dwLanguageId, lpBuffer, nSize, Arguments);
}

DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
		LPSTR lpBuffer, DWORD nSize, __builtin_va_list *Arguments) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, FormatMessageA, dwFlags, lpSource, dwMessageId,
			dwLanguageId, lpBuffer, nSize, Arguments);
}

WINAPI DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, WaitForSingleObject, hHandle, dwMilliseconds);
}

WINAPI DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, WaitForSingleObjectEx, hHandle, dwMilliseconds,
			bAlertable);
}

WINAPI HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName,
		DWORD dwFlags, DWORD dwDesiredAccess) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, CreateWaitableTimerExW, lpTimerAttributes,
			lpTimerName, dwFlags, dwDesiredAccess);
}

WINAPI BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
		PREASON_CONTEXT WakeContext, ULONG TolerableDelay) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetWaitableTimerEx, hTimer, lpDueTime, lPeriod,
			pfnCompletionRoutine, lpArgToCompletionRoutine, WakeContext, TolerableDelay);
}

WINAPI BOOL EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, EnumProcessModules, hProcess, lphModule, cb,
			lpcbNeeded);
}

WINAPI BOOL QueryWorkingSetEx(HANDLE hProcess, PVOID pWsInfo, DWORD cb) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, QueryWorkingSetEx, hProcess, pWsInfo, cb);
}

WINAPI void GetNativeSystemInfo(LPSYSTEM_INFO pSystemInfo) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernelbase, GetNativeSystemInfo, pSystemInfo);
}

WINAPI void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernelbase, GetSystemInfo, lpSystemInfo);
}

WINAPI UINT SetErrorMode(UINT uMode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernelbase, SetErrorMode, uMode);
}
}
