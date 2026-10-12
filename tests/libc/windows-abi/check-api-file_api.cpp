// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/file_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/file_api.h>
#include "api_check.h"

#include <windows.h>
#include <shlwapi.h>
#include <pathcch.h>

// skipped (not functions / not in the public SDK headers): void
SPRT_FN(CreateFileW);
SPRT_FN(CreateFileA);
SPRT_FN(ReadFile);
SPRT_FN(WriteFile);
SPRT_FN(GetOverlappedResult);
SPRT_FN(ReadDirectoryChangesW);
SPRT_FN(CancelIoEx);
SPRT_FN(SetFilePointerEx);
SPRT_FN(SetEndOfFile);
SPRT_FN(FlushFileBuffers);
SPRT_FN(GetFinalPathNameByHandleW);
SPRT_FN(SetFileAttributesW);
SPRT_FN(SetFileAttributesA);
SPRT_FN(SetFileInformationByHandle);
SPRT_FN(CreateDirectoryW);
SPRT_FN(GetTempFileNameW);
SPRT_FN(GetTempFileNameA);
SPRT_FN(GetFileInformationByHandleEx);
SPRT_FN(GetLogicalDrives);
SPRT_FN(PathIsRelativeW);
SPRT_FN(GetCurrentDirectoryW);
SPRT_FN(GetCurrentDirectoryA);
SPRT_FN(SetFilePointer);
SPRT_FN(GetFileInformationByHandle);
SPRT_FN(ReOpenFile);
SPRT_FN(CreateFileMappingW);
SPRT_FN(CreateFileMappingA);
SPRT_FN(OpenFileMappingW);
SPRT_FN(MapViewOfFile);
SPRT_FN(UnmapViewOfFile);
SPRT_FN(FlushViewOfFile);
SPRT_FN(DeleteFileW);
SPRT_FN(DeleteFileA);
SPRT_FN(RemoveDirectoryW);
SPRT_FN(GetFileAttributesW);
SPRT_FN(GetFileAttributesA);
SPRT_FN(GetFileAttributesExW);
SPRT_FN(GetFileAttributesExA);
SPRT_FN(MoveFileExW);
SPRT_FN(MoveFileExA);
SPRT_FN(SetCurrentDirectoryW);
SPRT_FN(SetCurrentDirectoryA);
SPRT_FN(GetVolumeInformationW);
SPRT_FN(GetDiskFreeSpaceExW);
SPRT_FN(GetDiskFreeSpaceExA);
SPRT_FN(GetVolumePathNameW);
SPRT_FN(GetDriveTypeW);
SPRT_FN(CreateHardLinkW);
SPRT_FN(CreateSymbolicLinkW);
SPRT_FN(CreatePipe);
SPRT_FN(CreateNamedPipeW);
SPRT_FN(CreateNamedPipeA);
SPRT_FN(SetNamedPipeHandleState);
SPRT_FN(GetTempPathW);
SPRT_FN(GetTempPathA);
SPRT_FN(PathAllocCanonicalize);
SPRT_FN(ReadFileScatter);
SPRT_FN(WriteFileGather);
SPRT_FN(GetFileSize);
SPRT_FN(GetFileSizeEx);
SPRT_FN(GetFileType);
SPRT_FN(AreFileApisANSI);
SPRT_FN(GetDiskFreeSpaceA);
SPRT_FN(GetDiskFreeSpaceW);
SPRT_FN(GetFullPathNameW);
SPRT_FN(GetFullPathNameA);
SPRT_FN(GetShortPathNameW);
SPRT_FN(GetShortPathNameA);
SPRT_FN(LockFile);
SPRT_FN(LockFileEx);
SPRT_FN(UnlockFile);
SPRT_FN(UnlockFileEx);
SPRT_FN(FindFirstFileW);
SPRT_FN(FindFirstFileA);
SPRT_FN(FindNextFileW);
SPRT_FN(FindNextFileA);
SPRT_FN(FindClose);
SPRT_FN(CompareFileTime);
SPRT_FN(GetFileTime);
