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

#include <sprt/wrappers/windows/file_api.h>

#include "dllloader.h"

extern "C" {

// ---- File Creation/Open (kernelbase) ----

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateFileW)>(loader->kernelbase.CreateFileW,
			lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateFileA)>(loader->kernelbase.CreateFileA,
			lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);
}

// ---- File I/O (kernel32) ----

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&ReadFile)>(loader->kernel32.ReadFile, hFile, lpBuffer,
			nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&WriteFile)>(loader->kernel32.WriteFile, hFile, lpBuffer,
			nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer,
		DWORD dwMoveMethod) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetFilePointerEx)>(loader->kernel32.SetFilePointerEx,
			hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
}

BOOL SetEndOfFile(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetEndOfFile)>(loader->kernel32.SetEndOfFile, hFile);
}

// ---- File Buffer/Path (kernelbase) ----

BOOL FlushFileBuffers(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&FlushFileBuffers)>(loader->kernelbase.FlushFileBuffers,
			hFile);
}

DWORD GetFinalPathNameByHandleW(HANDLE hHandle, LPWSTR lpFilePath, DWORD nSize, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFinalPathNameByHandleW)>(
			loader->kernelbase.GetFinalPathNameByHandleW, hHandle, lpFilePath, nSize, dwFlags);
}

BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetFileAttributesW)>(
			loader->kernelbase.SetFileAttributesW, lpFileName, dwFileAttributes);
}

BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetFileAttributesA)>(
			loader->kernelbase.SetFileAttributesA, lpFileName, dwFileAttributes);
}

BOOL SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
		LPVOID lpFileInformation, DWORD dwBufferSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetFileInformationByHandle)>(
			loader->kernel32.SetFileInformationByHandle, hFile, FileInformationClass,
			lpFileInformation, dwBufferSize);
}

// ---- Directory Operations (kernelbase) ----

BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateDirectoryW)>(loader->kernelbase.CreateDirectoryW,
			lpPathName, lpSecurityAttributes);
}

DWORD GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpTemplateFile, UINT uUnique,
		LPWSTR lpTempFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetTempFileNameW)>(loader->kernelbase.GetTempFileNameW,
			lpPathName, lpTemplateFile, uUnique, lpTempFileName);
}

BOOL GetFileInformationByHandleEx(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
		LPVOID lpFileInformation, DWORD dwBufferSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileInformationByHandleEx)>(
			loader->kernelbase.GetFileInformationByHandleEx, hFile, FileInformationClass,
			lpFileInformation, dwBufferSize);
}

DWORD GetLogicalDrives() {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetLogicalDrives)>(
			loader->kernelbase.GetLogicalDrives);
}

// ---- Path Functions (kernelbase) ----

BOOL PathIsRelativeW(LPCWSTR pszPath) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&PathIsRelativeW)>(loader->kernelbase.PathIsRelativeW,
			pszPath);
}

// ---- Current Directory (kernelbase) ----

DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetCurrentDirectoryW)>(
			loader->kernelbase.GetCurrentDirectoryW, nBufferLength, lpBuffer);
}

// ---- File Pointer (kernel32) ----

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG *lpDistanceToMoveHigh,
		DWORD dwMoveMethod) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetFilePointer)>(loader->kernel32.SetFilePointer, hFile,
			lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

// ---- File Information (kernelbase) ----

BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileInformationByHandle)>(
			loader->kernelbase.GetFileInformationByHandle, hFile, lpFileInformation);
}

// ---- ReOpen File (kernelbase) ----

HANDLE ReOpenFile(HANDLE hOriginalFile, DWORD dwDesiredAccess, DWORD dwShareMode,
		DWORD dwFlagsAndAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&ReOpenFile)>(loader->kernelbase.ReOpenFile,
			hOriginalFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes);
}

// ---- File Mapping (kernelbase) ----

HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateFileMappingW)>(
			loader->kernelbase.CreateFileMappingW, hFile, lpFileMappingAttributes, flProtect,
			dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateFileMappingA)>(loader->kernel32.CreateFileMappingA,
			hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
		DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&MapViewOfFile)>(loader->kernelbase.MapViewOfFile,
			hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow,
			dwNumberOfBytesToMap);
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&UnmapViewOfFile)>(loader->kernelbase.UnmapViewOfFile,
			lpBaseAddress);
}

BOOL FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&FlushViewOfFile)>(loader->kernelbase.FlushViewOfFile,
			lpBaseAddress, dwNumberOfBytesToFlush);
}

// ---- File Deletion/Removal (kernelbase) ----

BOOL DeleteFileW(LPCWSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&DeleteFileW)>(loader->kernelbase.DeleteFileW,
			lpFileName);
}

WINAPI BOOL DeleteFileA(LPCSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&DeleteFileA)>(loader->kernelbase.DeleteFileA,
			lpFileName);
}

BOOL RemoveDirectoryW(LPCWSTR lpPathName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&RemoveDirectoryW)>(loader->kernelbase.RemoveDirectoryW,
			lpPathName);
}

DWORD GetFileAttributesW(LPCWSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileAttributesW)>(
			loader->kernelbase.GetFileAttributesW, lpFileName);
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileAttributesA)>(
			loader->kernelbase.GetFileAttributesA, lpFileName);
}

BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
		LPVOID lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileAttributesExW)>(
			loader->kernelbase.GetFileAttributesExW, lpFileName, fInfoLevelId, lpFileInformation);
}

BOOL GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
		LPVOID lpFileInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetFileAttributesExA)>(
			loader->kernelbase.GetFileAttributesExA, lpFileName, fInfoLevelId, lpFileInformation);
}

BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&MoveFileExW)>(loader->kernelbase.MoveFileExW,
			lpExistingFileName, lpNewFileName, dwFlags);
}

BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&MoveFileExA)>(loader->kernel32.MoveFileExA,
			lpExistingFileName, lpNewFileName, dwFlags);
}

BOOL SetCurrentDirectoryW(LPCWSTR lpPathName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetCurrentDirectoryW)>(
			loader->kernelbase.SetCurrentDirectoryW, lpPathName);
}

// ---- Volume Information (kernelbase) ----

BOOL GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize,
		LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
		LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetVolumeInformationW)>(
			loader->kernelbase.GetVolumeInformationW, lpRootPathName, lpVolumeNameBuffer,
			nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
			lpFileSystemNameBuffer, nFileSystemNameSize);
}

BOOL GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller,
		PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetDiskFreeSpaceExW)>(
			loader->kernelbase.GetDiskFreeSpaceExW, lpDirectoryName, lpFreeBytesAvailableToCaller,
			lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

// ---- Link Creation (kernelbase) ----

BOOL CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateHardLinkW)>(loader->kernelbase.CreateHardLinkW,
			lpFileName, lpExistingFileName, lpSecurityAttributes);
}

BOOLEAN CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateSymbolicLinkW)>(
			loader->kernelbase.CreateSymbolicLinkW, lpSymlinkFileName, lpTargetFileName, dwFlags);
}

// ---- Pipe Operations (kernelbase) ----

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes,
		DWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreatePipe)>(loader->kernelbase.CreatePipe, hReadPipe,
			hWritePipe, lpPipeAttributes, nSize);
}

// ---- Temporary Path (kernelbase) ----

DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetTempPathW)>(loader->kernelbase.GetTempPathW,
			nBufferLength, lpBuffer);
}

WINAPI DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&GetTempPathA)>(loader->kernelbase.GetTempPathA,
			nBufferLength, lpBuffer);
}

// ---- Path Canonicalization (kernelbase) ----

HRESULT PathAllocCanonicalize(PCWSTR pszPathIn, ULONG dwFlags, PWSTR *ppszPathOut) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&PathAllocCanonicalize)>(
			loader->kernelbase.PathAllocCanonicalize, pszPathIn, dwFlags, ppszPathOut);
}

WINAPI BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&ReadFileScatter)>(loader->kernel32.ReadFileScatter,
			hFile, aSegmentArray, nNumberOfBytesToRead, lpReserved, lpOverlapped);
}

WINAPI BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&WriteFileGather)>(loader->kernel32.WriteFileGather,
			hFile, aSegmentArray, nNumberOfBytesToWrite, lpReserved, lpOverlapped);
}

WINAPI DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFileSize)>(loader->kernel32.GetFileSize, hFile,
			lpFileSizeHigh);
}

WINAPI BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFileSizeEx)>(loader->kernel32.GetFileSizeEx, hFile,
			lpFileSize);
}

WINAPI DWORD GetFileType(HANDLE hFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFileType)>(loader->kernel32.GetFileType, hFile);
}

WINAPI BOOL AreFileApisANSI(VOID) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&AreFileApisANSI)>(loader->kernel32.AreFileApisANSI);
}

WINAPI BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
		LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetDiskFreeSpaceA)>(loader->kernel32.GetDiskFreeSpaceA,
			lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters,
			lpTotalNumberOfClusters);
}

WINAPI BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
		LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetDiskFreeSpaceW)>(loader->kernel32.GetDiskFreeSpaceW,
			lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters,
			lpTotalNumberOfClusters);
}

WINAPI DWORD GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer,
		LPWSTR *lpFilePart) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFullPathNameW)>(loader->kernel32.GetFullPathNameW,
			lpFileName, nBufferLength, lpBuffer, lpFilePart);
}

WINAPI DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer,
		LPSTR *lpFilePart) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFullPathNameA)>(loader->kernel32.GetFullPathNameA,
			lpFileName, nBufferLength, lpBuffer, lpFilePart);
}

WINAPI BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LockFile)>(loader->kernel32.LockFile, hFile,
			dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);
}

WINAPI BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow,
		DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LockFileEx)>(loader->kernel32.LockFileEx, hFile, dwFlags,
			dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

WINAPI BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&UnlockFile)>(loader->kernel32.UnlockFile, hFile,
			dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToUnlockLow,
			nNumberOfBytesToUnlockHigh);
}

WINAPI BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
		DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&UnlockFileEx)>(loader->kernel32.UnlockFileEx, hFile,
			dwReserved, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);
}

WINAPI HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FindFirstFileW)>(loader->kernel32.FindFirstFileW,
			lpFileName, lpFindFileData);
}

WINAPI HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FindFirstFileA)>(loader->kernel32.FindFirstFileA,
			lpFileName, lpFindFileData);
}

WINAPI BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FindNextFileW)>(loader->kernel32.FindNextFileW,
			hFindFile, lpFindFileData);
}

WINAPI BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FindNextFileA)>(loader->kernel32.FindNextFileA,
			hFindFile, lpFindFileData);
}

WINAPI BOOL FindClose(HANDLE hFindFile) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FindClose)>(loader->kernel32.FindClose, hFindFile);
}

WINAPI LONG CompareFileTime(const FILETIME *lpFileTime1, const FILETIME *lpFileTime2) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CompareFileTime)>(loader->kernel32.CompareFileTime,
			lpFileTime1, lpFileTime2);
}

WINAPI BOOL GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime,
		LPFILETIME lpLastWriteTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetFileTime)>(loader->kernel32.GetFileTime, hFile,
			lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}
} // extern "C"
