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
HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LocalAlloc, uFlags, uBytes);
}

HLOCAL LocalFree(HLOCAL hMem) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LocalFree, hMem);
}

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetEnvironmentVariableA, lpName, lpBuffer, nSize);
}

BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetEnvironmentVariableA, lpName, lpValue);
}

DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetEnvironmentVariableW, lpName, lpBuffer, nSize);
}

BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetEnvironmentVariableW, lpName, lpValue);
}

LPWCH GetEnvironmentStringsW() {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetEnvironmentStringsW);
}

VOID Sleep(DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, Sleep, dwMilliseconds);
}

WINAPI DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SleepEx, dwMilliseconds, bAlertable);
}

WINAPI DWORD GetCurrentProcessorNumber(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentProcessorNumber);
}

WINAPI VOID OutputDebugStringA(LPCSTR lpOutputString) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, OutputDebugStringA, lpOutputString);
}

WINAPI VOID OutputDebugStringW(LPCWSTR lpOutputString) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, OutputDebugStringW, lpOutputString);
}

PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64 ControlPc, PDWORD64 ImageBase,
		PUNWIND_HISTORY_TABLE HistoryTable) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RtlLookupFunctionEntry, ControlPc, ImageBase,
			HistoryTable);
}

VOID RtlUnwindEx(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord,
		PVOID ReturnValue, PCONTEXT ContextRecord, PUNWIND_HISTORY_TABLE HistoryTable) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, RtlUnwindEx, TargetFrame, TargetIp, ExceptionRecord,
			ReturnValue, ContextRecord, HistoryTable);
}

PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD HandlerType, DWORD64 ImageBase, DWORD64 ControlPc,
		PRUNTIME_FUNCTION FunctionEntry, PCONTEXT ContextRecord, PVOID *HandlerData,
		PDWORD64 EstablisherFrame, PVOID ContextPointers) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RtlVirtualUnwind, HandlerType, ImageBase, ControlPc,
			FunctionEntry, ContextRecord, HandlerData, EstablisherFrame, ContextPointers);
}

WORD RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture, PVOID *BackTrace,
		PDWORD BackTraceHash) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RtlCaptureStackBackTrace, FramesToSkip,
			FramesToCapture, BackTrace, BackTraceHash);
}

WINAPI PVOID AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, AddVectoredExceptionHandler, First, Handler);
}

WINAPI ULONG RemoveVectoredExceptionHandler(PVOID Handle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RemoveVectoredExceptionHandler, Handle);
}

WINAPI PVOID AddVectoredContinueHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, AddVectoredContinueHandler, First, Handler);
}

WINAPI ULONG RemoveVectoredContinueHandler(PVOID Handle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RemoveVectoredContinueHandler, Handle);
}

WINAPI VOID RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments,
		const ULONG_PTR *lpArguments) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RaiseException, dwExceptionCode, dwExceptionFlags,
			nNumberOfArguments, lpArguments);
}

SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, VirtualQuery, lpAddress, lpBuffer, dwLength);
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LoadLibraryA, lpLibFileName);
}

HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LoadLibraryExW, lpLibFileName, hFile, dwFlags);
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetModuleHandleW, lpModuleName);
}

BOOL GetModuleHandleExA(DWORD dwFlags, LPCSTR lpModuleName, HMODULE *phModule) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetModuleHandleExA, dwFlags, lpModuleName, phModule);
}

BOOL GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, PHMODULE phModule) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetModuleHandleExW, dwFlags, lpModuleName, phModule);
}

DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetModuleFileNameW, hModule, lpFilename, nSize);
}

WINAPI PVOID RtlPcToFileHeader(PVOID PcValue, PVOID *BaseOfImage) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RtlPcToFileHeader, PcValue, BaseOfImage);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ReadFile, hFile, lpBuffer, nNumberOfBytesToRead,
			lpNumberOfBytesRead, lpOverlapped);
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, WriteFile, hFile, lpBuffer, nNumberOfBytesToWrite,
			lpNumberOfBytesWritten, lpOverlapped);
}

BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer,
		DWORD dwMoveMethod) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetFilePointerEx, hFile, liDistanceToMove,
			lpNewFilePointer, dwMoveMethod);
}

BOOL SetEndOfFile(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetEndOfFile, hFile);
}

BOOL SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
		LPVOID lpFileInformation, DWORD dwBufferSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetFileInformationByHandle, hFile,
			FileInformationClass, lpFileInformation, dwBufferSize);
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG *lpDistanceToMoveHigh,
		DWORD dwMoveMethod) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetFilePointer, hFile, lDistanceToMove,
			lpDistanceToMoveHigh, dwMoveMethod);
}

HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateFileMappingA, hFile, lpFileMappingAttributes,
			flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, MoveFileExA, lpExistingFileName, lpNewFileName,
			dwFlags);
}

WINAPI BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ReadFileScatter, hFile, aSegmentArray,
			nNumberOfBytesToRead, lpReserved, lpOverlapped);
}

WINAPI BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, WriteFileGather, hFile, aSegmentArray,
			nNumberOfBytesToWrite, lpReserved, lpOverlapped);
}

WINAPI DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFileSize, hFile, lpFileSizeHigh);
}

WINAPI BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFileSizeEx, hFile, lpFileSize);
}

WINAPI DWORD GetFileType(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFileType, hFile);
}

WINAPI BOOL AreFileApisANSI(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, AreFileApisANSI);
}

WINAPI BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
		LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetDiskFreeSpaceA, lpRootPathName,
			lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

WINAPI BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
		LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetDiskFreeSpaceW, lpRootPathName,
			lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

WINAPI DWORD GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer,
		LPWSTR *lpFilePart) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFullPathNameW, lpFileName, nBufferLength,
			lpBuffer, lpFilePart);
}

WINAPI DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer,
		LPSTR *lpFilePart) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFullPathNameA, lpFileName, nBufferLength,
			lpBuffer, lpFilePart);
}

WINAPI BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LockFile, hFile, dwFileOffsetLow, dwFileOffsetHigh,
			nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);
}

WINAPI BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow,
		DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LockFileEx, hFile, dwFlags, dwReserved,
			nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

WINAPI BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, UnlockFile, hFile, dwFileOffsetLow, dwFileOffsetHigh,
			nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);
}

WINAPI BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
		DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, UnlockFileEx, hFile, dwReserved,
			nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);
}

WINAPI HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FindFirstFileW, lpFileName, lpFindFileData);
}

WINAPI HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FindFirstFileA, lpFileName, lpFindFileData);
}

WINAPI BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FindNextFileW, hFindFile, lpFindFileData);
}

WINAPI BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FindNextFileA, hFindFile, lpFindFileData);
}

WINAPI BOOL FindClose(HANDLE hFindFile) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FindClose, hFindFile);
}

WINAPI LONG CompareFileTime(const FILETIME *lpFileTime1, const FILETIME *lpFileTime2) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CompareFileTime, lpFileTime1, lpFileTime2);
}

WINAPI BOOL GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime,
		LPFILETIME lpLastWriteTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetFileTime, hFile, lpCreationTime, lpLastAccessTime,
			lpLastWriteTime);
}

BOOL GetSystemPowerStatus(LPSYSTEM_POWER_STATUS lpSystemPowerStatus) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetSystemPowerStatus, lpSystemPowerStatus);
}

HANDLE GetCurrentProcess(void) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentProcess);
}

DWORD GetCurrentProcessId(void) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentProcessId);
}

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, OpenProcess, dwDesiredAccess, bInheritHandle,
			dwProcessId);
}

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetExitCodeProcess, hProcess, lpExitCode);
}

DWORD GetPriorityClass(HANDLE hProcess) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetPriorityClass, hProcess);
}

BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
		DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, InitializeProcThreadAttributeList, lpAttributeList,
			dwAttributeCount, dwFlags, lpSize);
}

BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList, DWORD dwFlags,
		DWORD_PTR Attribute, PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue,
		PSIZE_T lpReturnSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, UpdateProcThreadAttribute, lpAttributeList, dwFlags,
			Attribute, lpValue, cbSize, lpPreviousValue, lpReturnSize);
}

VOID DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, DeleteProcThreadAttributeList, lpAttributeList);
}

LPWSTR GetCommandLineW(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCommandLineW);
}

BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
		LPPROCESS_INFORMATION lpProcessInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateProcessW, lpApplicationName, lpCommandLine,
			lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags,
			lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

WINAPI BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TerminateProcess, hProcess, uExitCode);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
		LPDWORD lpThreadId) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateThread, lpThreadAttributes, dwStackSize,
			lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, OpenThread, dwDesiredAccess, bInheritHandle,
			dwThreadId);
}

HANDLE GetCurrentThread(void) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentThread);
}

DWORD GetCurrentThreadId(void) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentThreadId);
}

VOID GetCurrentThreadStackLimits(PULONG_PTR LowLimit, PULONG_PTR HighLimit) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, GetCurrentThreadStackLimits, LowLimit, HighLimit);
}

int GetThreadPriority(HANDLE hThread) {
	auto loader = sprt::DllLoader::get();
	return (int)DLL_LOAD_AND_CALL(loader, kernel32, GetThreadPriority, hThread);
}

BOOL SetThreadPriority(HANDLE hThread, int nPriority) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetThreadPriority, hThread, (DWORD)nPriority);
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetExitCodeThread, hThread, lpExitCode);
}

void ExitThread(DWORD dwExitCode) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, ExitThread, dwExitCode);
}

DWORD ResumeThread(HANDLE hThread) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ResumeThread, hThread);
}

BOOL QueueUserAPC2(PAPCFUNC ApcRoutine, HANDLE Thread, ULONG_PTR Data, QUEUE_USER_APC_FLAGS Flags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, QueueUserAPC2, ApcRoutine, Thread, Data, Flags);
}

HRESULT SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetThreadDescription, hThread, lpThreadDescription);
}

BOOL GetSystemCpuSetInformation(PSYSTEM_CPU_SET_INFORMATION Information, ULONG BufferLength,
		PULONG ReturnedLength, HANDLE Process, ULONG Flags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetSystemCpuSetInformation, Information,
			BufferLength, ReturnedLength, Process, Flags);
}

BOOL SetThreadSelectedCpuSets(HANDLE Thread, const ULONG *CpuSetIds, ULONG CpuSetIdCount) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetThreadSelectedCpuSets, Thread, (ULONG *)CpuSetIds,
			CpuSetIdCount);
}

BOOL GetThreadSelectedCpuSets(HANDLE Thread, PULONG CpuSetIds, ULONG CpuSetIdCount,
		PULONG RequiredIdCount) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetThreadSelectedCpuSets, Thread, CpuSetIds,
			CpuSetIdCount, RequiredIdCount);
}

HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateToolhelp32Snapshot, dwFlags, th32ProcessID);
}

BOOL Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, Thread32First, hSnapshot, lpte);
}

BOOL Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, Thread32Next, hSnapshot, lpte);
}

WINAPI VOID InitializeSRWLock(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, InitializeSRWLock, SRWLock);
}

WINAPI VOID ReleaseSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, ReleaseSRWLockExclusive, SRWLock);
}

WINAPI VOID ReleaseSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, ReleaseSRWLockShared, SRWLock);
}

WINAPI VOID AcquireSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, AcquireSRWLockExclusive, SRWLock);
}

WINAPI VOID AcquireSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, AcquireSRWLockShared, SRWLock);
}

WINAPI BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TryAcquireSRWLockExclusive, SRWLock);
}

WINAPI BOOLEAN TryAcquireSRWLockShared(PSRWLOCK SRWLock) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TryAcquireSRWLockShared, SRWLock);
}

WINAPI BOOL SwitchToThread(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SwitchToThread);
}

WINAPI HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner,
		LPCWSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateMutexW, lpMutexAttributes, bInitialOwner,
			lpName);
}

WINAPI HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount,
		LONG lMaximumCount, LPCWSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateSemaphoreW, lpSemaphoreAttributes,
			lInitialCount, lMaximumCount, lpName);
}

WINAPI HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount,
		LONG lMaximumCount, LPCSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateSemaphoreA, lpSemaphoreAttributes,
			lInitialCount, lMaximumCount, lpName);
}

WINAPI HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
		BOOL bInitialState, LPCWSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateEventW, lpEventAttributes, bManualReset,
			bInitialState, lpName);
}

WINAPI BOOL SetEvent(HANDLE hEvent) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetEvent, hEvent);
}

WINAPI BOOL ResetEvent(HANDLE hEvent) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ResetEvent, hEvent);
}

WINAPI BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ReleaseSemaphore, hSemaphore, lReleaseCount,
			lpPreviousCount);
}

WINAPI VOID SwitchToFiber(LPVOID lpFiber) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SwitchToFiber, lpFiber);
}

WINAPI VOID DeleteFiber(LPVOID lpFiber) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, DeleteFiber, lpFiber);
}

WINAPI BOOL ConvertFiberToThread(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ConvertFiberToThread);
}

WINAPI LPVOID CreateFiberEx(SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize, DWORD dwFlags,
		LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateFiberEx, dwStackCommitSize, dwStackReserveSize,
			dwFlags, lpStartAddress, lpParameter);
}

WINAPI LPVOID ConvertThreadToFiberEx(LPVOID lpParameter, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ConvertThreadToFiberEx, lpParameter, dwFlags);
}

WINAPI LPVOID CreateFiber(SIZE_T dwStackSize, LPFIBER_START_ROUTINE lpStartAddress,
		LPVOID lpParameter) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateFiber, dwStackSize, lpStartAddress,
			lpParameter);
}

WINAPI LPVOID ConvertThreadToFiber(LPVOID lpParameter) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ConvertThreadToFiber, lpParameter);
}

WINAPI DWORD TlsAlloc(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TlsAlloc);
}

WINAPI LPVOID TlsGetValue(DWORD dwTlsIndex) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TlsGetValue, dwTlsIndex);
}

WINAPI BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TlsSetValue, dwTlsIndex, lpTlsValue);
}

WINAPI BOOL TlsFree(DWORD dwTlsIndex) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, TlsFree, dwTlsIndex);
}

WINAPI DWORD GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetDynamicTimeZoneInformation, pTimeZoneInformation);
}

WINAPI DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetTimeZoneInformation, lpTimeZoneInformation);
}

WINAPI BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, QueryPerformanceFrequency, lpFrequency);
}

WINAPI BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, QueryPerformanceCounter, lpPerformanceCount);
}

WINAPI VOID GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, GetSystemTimePreciseAsFileTime, lpSystemTimeAsFileTime);
}

WINAPI VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, GetSystemTimeAsFileTime, lpSystemTimeAsFileTime);
}

WINAPI VOID GetSystemTime(LPSYSTEMTIME lpSystemTime) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, kernel32, GetSystemTime, lpSystemTime);
}

WINAPI DWORD GetTickCount() {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetTickCount);
}

WINAPI ULONGLONG GetTickCount64() {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetTickCount64);
}

WINAPI BOOL GetProcessTimes(HANDLE hProcess, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetProcessTimes, hProcess, lpCreationTime,
			lpExitTime, lpKernelTime, lpUserTime);
}

WINAPI BOOL GetThreadTimes(HANDLE hThread, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetThreadTimes, hThread, lpCreationTime, lpExitTime,
			lpKernelTime, lpUserTime);
}

WINAPI BOOL SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SystemTimeToFileTime, lpSystemTime, lpFileTime);
}

WINAPI BOOL FileTimeToSystemTime(const FILETIME *lpFileTime, LPSYSTEMTIME lpSystemTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, FileTimeToSystemTime, lpFileTime, lpSystemTime);
}

WINAPI BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION *lpTimeZoneInformation,
		const SYSTEMTIME *lpUniversalTime, LPSYSTEMTIME lpLocalTime) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SystemTimeToTzSpecificLocalTime,
			lpTimeZoneInformation, lpUniversalTime, lpLocalTime);
}

WINAPI BOOL GetTimeZoneInformationForYear(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi,
		LPTIME_ZONE_INFORMATION ptzi) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetTimeZoneInformationForYear, wYear, pdtzi, ptzi);
}

WINAPI UINT timeBeginPeriod(UINT uPeriod) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, timeBeginPeriod, uPeriod);
}

WINAPI VOID timeEndPeriod(UINT uPeriod) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, timeEndPeriod, uPeriod);
}

WINAPI BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lpEntry) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, Process32FirstW, hSnapshot, lpEntry);
}

WINAPI BOOL Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, Process32NextW, hSnapshot, lppe);
}

WINAPI HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpAttributes, BOOL bManualReset,
		LPCWSTR lpTimerName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateWaitableTimerW, lpAttributes, bManualReset,
			lpTimerName);
}

WINAPI BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER *ftDueTime, LONG dwPeriod,
		PTIMERAPCROUTINE lpCompletionRoutine, LPVOID lpArgument, BOOL fResume) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, SetWaitableTimer, hTimer, ftDueTime, dwPeriod,
			lpCompletionRoutine, lpArgument, fResume);
}

WINAPI BOOL CancelWaitableTimer(HANDLE hTimer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CancelWaitableTimer, hTimer);
}

WINAPI LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, DWORD samDesired,
		PHANDLE phkResult) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RegOpenKeyExW, hKey, lpSubKey, Reserved, samDesired,
			phkResult);
}

WINAPI LSTATUS RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
		LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RegGetValueW, hkey, lpSubKey, lpValue, dwFlags,
			pdwType, pvData, pcbData);
}

WINAPI LSTATUS RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName, LPDWORD lpcchValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RegEnumValueW, hKey, dwIndex, lpValueName,
			lpcchValueName, lpReserved, lpType, lpData, lpcbData);
}

WINAPI LONG RegCloseKey(HKEY hKey) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, RegCloseKey, hKey);
}

WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetComputerNameW, lpBuffer, nSize);
}

WINAPI DWORD GetActiveProcessorCount(WORD GroupNumber) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetActiveProcessorCount, GroupNumber);
}

WINAPI BOOL GetLogicalProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer,
		PDWORD ReturnedLength) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetLogicalProcessorInformation, Buffer,
			ReturnedLength);
}

WINAPI BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GlobalMemoryStatusEx, lpBuffer);
}

WINAPI BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetComputerNameExW, NameType, lpBuffer, nSize);
}

WINAPI HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
		ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateIoCompletionPort, FileHandle,
			ExistingCompletionPort, CompletionKey, NumberOfConcurrentThreads);
}

WINAPI BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort,
		LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved,
		DWORD dwMilliseconds, BOOL fAlertable) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetQueuedCompletionStatusEx, CompletionPort,
			lpCompletionPortEntries, ulCount, ulNumEntriesRemoved, dwMilliseconds, fAlertable);
}

WINAPI BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
		ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, PostQueuedCompletionStatus, CompletionPort,
			dwNumberOfBytesTransferred, dwCompletionKey, lpOverlapped);
}

WINAPI int LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc,
		LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM sortHandle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, LCMapStringEx, lpLocaleName, dwMapFlags, lpSrcStr,
			cchSrc, lpDestStr, cchDest, lpVersionInformation, lpReserved, sortHandle);
}

WINAPI int CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWCH lpString1, int cchCount1,
		LPCWCH lpString2, int cchCount2, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CompareStringEx, lpLocaleName, dwCmpFlags, lpString1,
			cchCount1, lpString2, cchCount2, lpVersionInformation, lpReserved, lParam);
}

WINAPI int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
		LPWSTR lpWideCharStr, int cchWideChar) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, MultiByteToWideChar, CodePage, dwFlags,
			lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
}

WINAPI int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, WideCharToMultiByte, CodePage, dwFlags,
			lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar,
			lpUsedDefaultChar);
}

WINAPI int GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetLocaleInfoEx, lpLocaleName, LCType, lpLCData,
			cchData);
}

WINAPI BOOL IsValidLocaleName(LPCWSTR lpLocaleName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, IsValidLocaleName, lpLocaleName);
}

WINAPI int GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetUserDefaultLocaleName, lpLocaleName,
			cchLocaleName);
}

WINAPI int IdnToAscii(DWORD dwFlags, LPCWSTR lpUnicodeCharStr, int cchUnicodeChar,
		LPWSTR lpASCIICharStr, int cchASCIIChar) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, IdnToAscii, dwFlags, lpUnicodeCharStr,
			cchUnicodeChar, lpASCIICharStr, cchASCIIChar);
}

WINAPI int IdnToUnicode(DWORD dwFlags, LPCWSTR lpASCIICharStr, int cchASCIIChar,
		LPWSTR lpUnicodeCharStr, int cchUnicodeChar) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, IdnToUnicode, dwFlags, lpASCIICharStr, cchASCIIChar,
			lpUnicodeCharStr, cchUnicodeChar);
}

WINAPI BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetVersionExA, lpVersionInformation);
}

WINAPI BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetVersionExW, lpVersionInformation);
}

WINAPI UINT GetACP(void) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetACP);
}

WINAPI BOOL ReadConsoleA(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
		LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL pInputControl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ReadConsoleA, hConsoleInput, lpBuffer,
			nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
}

WINAPI BOOL ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
		LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL pInputControl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ReadConsoleW, hConsoleInput, lpBuffer,
			nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
}

WINAPI UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetSystemDirectoryA, lpBuffer, uSize);
}

WINAPI UINT GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, GetSystemDirectoryW, lpBuffer, uSize);
}

WINAPI DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll,
		DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, WaitForMultipleObjects, nCount, lpHandles, bWaitAll,
			dwMilliseconds);
}

WINAPI BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize,
		LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, PeekNamedPipe, hNamedPipe, lpBuffer, nBufferSize,
			lpBytesRead, lpTotalBytesAvail, lpBytesLeftThisMessage);
}

WINAPI ULONGLONG VerSetConditionMask(ULONGLONG ConditionMask, DWORD TypeMask, BYTE Condition) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, VerSetConditionMask, ConditionMask, TypeMask,
			Condition);
}

WINAPI BOOL VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask,
		DWORDLONG dwlConditionMask) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, VerifyVersionInfoW, lpVersionInformation, dwTypeMask,
			dwlConditionMask);
}

WINAPI HANDLE CreateTimerQueue(VOID) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateTimerQueue);
}

WINAPI BOOL CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue,
		WAITORTIMERCALLBACK Callback, PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, CreateTimerQueueTimer, phNewTimer, TimerQueue,
			Callback, Parameter, DueTime, Period, Flags);
}

WINAPI BOOL ChangeTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, ULONG DueTime, ULONG Period) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, ChangeTimerQueueTimer, TimerQueue, Timer, DueTime,
			Period);
}

BOOL WINAPI DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, DeleteTimerQueueTimer, TimerQueue, Timer,
			CompletionEvent);
}

BOOL WINAPI DeleteTimerQueue(HANDLE TimerQueue) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, DeleteTimerQueue, TimerQueue);
}

BOOL WINAPI DeleteTimerQueueEx(HANDLE TimerQueue, HANDLE CompletionEvent) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, kernel32, DeleteTimerQueueEx, TimerQueue, CompletionEvent);
}
}
