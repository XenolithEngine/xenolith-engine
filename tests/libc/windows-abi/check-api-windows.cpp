// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/windows.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// keep psapi.h on its plain (non-K32*) spellings
#define PSAPI_VERSION 1

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/windows.h>
#include "api_check.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>


// skipped (not functions / not in the public SDK headers): NtAssociateWaitCompletionPacket, NtCancelWaitCompletionPacket, NtClose, NtCompletionPacketAvailable, NtCreateWaitCompletionPacket, NtQueryObject
SPRT_FN(WaitForSingleObject);
SPRT_FN(WaitForSingleObjectEx);
SPRT_FN(OpenProcessToken);
SPRT_FN(GetTokenInformation);
SPRT_FN(Process32FirstW);
SPRT_FN(Process32NextW);
SPRT_FN(GetUserNameW);
SPRT_FN(CreateWaitableTimerW);
SPRT_FN(CreateWaitableTimerExW);
SPRT_FN(SetWaitableTimer);
SPRT_FN(SetWaitableTimerEx);
SPRT_FN(CancelWaitableTimer);
SPRT_FN(EnumProcessModules);
SPRT_FN(QueryWorkingSetEx);
SPRT_FN(RegOpenKeyExW);
SPRT_FN(RegOpenKeyExA);
SPRT_FN(RegQueryValueExW);
SPRT_FN(RegQueryValueExA);
SPRT_FN(RegEnumKeyExA);
SPRT_FN(RegEnumKeyExW);
SPRT_FN(RegGetValueW);
SPRT_FN(RegEnumValueW);
SPRT_FN(RegCloseKey);
SPRT_FN(ExpandEnvironmentStringsW);
SPRT_FN(GetComputerNameW);
SPRT_FN(GetNativeSystemInfo);
SPRT_FN(GetSystemInfo);
SPRT_FN(GetActiveProcessorCount);
SPRT_FN(GetLogicalProcessorInformation);
SPRT_FN(SetFileTime);
SPRT_FN(FindFirstFileExW);
SPRT_FN(GetLogicalProcessorInformationEx);
SPRT_FN(FlushInstructionCache);
SPRT_FN(GetLargePageMinimum);
SPRT_FN(GetLongPathNameW);
SPRT_FN(CreateJobObjectW);
SPRT_FN(AssignProcessToJobObject);
SPRT_FN(SetInformationJobObject);
SPRT_FN(GetThreadId);
SPRT_FN(GetProcessAffinityMask);
SPRT_FN(SetProcessAffinityMask);
SPRT_FN(GetProcessGroupAffinity);
SPRT_FN(SetThreadGroupAffinity);
SPRT_FN(GetThreadGroupAffinity);
SPRT_FN(SearchPathW);
SPRT_FN(SearchPathA);
SPRT_FN(GlobalMemoryStatusEx);
SPRT_FN(GetComputerNameExW);
SPRT_FN(SetErrorMode);
SPRT_FN(GetErrorMode);
SPRT_FN(CreateMemoryResourceNotification);
SPRT_FN(QueryMemoryResourceNotification);
SPRT_FN(CreateIoCompletionPort);
SPRT_FN(GetQueuedCompletionStatusEx);
SPRT_FN(PostQueuedCompletionStatus);
SPRT_FN(LCMapStringEx);
SPRT_FN(CompareStringEx);
SPRT_FN(CompareStringOrdinal);
SPRT_FN(MultiByteToWideChar);
SPRT_FN(WideCharToMultiByte);
SPRT_FN(GetLocaleInfoEx);
SPRT_FN(IsValidLocaleName);
SPRT_FN(GetUserDefaultLocaleName);
SPRT_FN(IdnToAscii);
SPRT_FN(IdnToUnicode);
SPRT_FN(GetACP);
SPRT_FN(ReadConsoleA);
SPRT_FN(ReadConsoleW);
SPRT_FN(GetSystemDirectoryA);
SPRT_FN(GetSystemDirectoryW);
SPRT_FN(GetWindowsDirectoryA);
SPRT_FN(GetWindowsDirectoryW);
SPRT_FN(GetEnvironmentStringsW);
SPRT_FN(ReadProcessMemory);
SPRT_FN(WriteProcessMemory);
SPRT_FN(GetLogicalDriveStringsW);
SPRT_FN(QueryDosDeviceW);
SPRT_FN(WaitForMultipleObjects);
SPRT_FN(PeekNamedPipe);
