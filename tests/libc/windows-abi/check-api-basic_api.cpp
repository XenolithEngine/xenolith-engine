// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/basic_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/basic_api.h>
#include "api_check.h"

#include <windows.h>

// skipped (not functions / not in the public SDK headers): BOOL
SPRT_FN(LocalAlloc);
SPRT_FN(GlobalAlloc);
SPRT_FN(GlobalFree);
SPRT_FN(GlobalLock);
SPRT_FN(GlobalUnlock);
SPRT_FN(GlobalSize);
SPRT_FN(LocalFree);
SPRT_FN(HeapAlloc);
SPRT_FN(HeapReAlloc);
SPRT_FN(HeapFree);
SPRT_FN(HeapSize);
SPRT_FN(HeapCreate);
SPRT_FN(HeapDestroy);
SPRT_FN(HeapValidate);
SPRT_FN(HeapCompact);
SPRT_FN(GetProcessHeap);
SPRT_FN(VirtualAlloc);
SPRT_FN(VirtualProtect);
SPRT_FN(VirtualFree);
SPRT_FN(VirtualAllocEx);
SPRT_FN(VirtualFreeEx);
SPRT_FN(VirtualProtectEx);
SPRT_FN(DiscardVirtualMemory);
SPRT_FN(GetLastError);
SPRT_FN(SetLastError);
SPRT_FN(CloseHandle);
SPRT_FN(GetHandleInformation);
SPRT_FN(SetHandleInformation);
SPRT_FN(DuplicateHandle);
SPRT_FN(VirtualLock);
SPRT_FN(VirtualUnlock);
SPRT_FN(GetEnvironmentVariableA);
SPRT_FN(SetEnvironmentVariableA);
SPRT_FN(GetEnvironmentVariableW);
SPRT_FN(SetEnvironmentVariableW);
SPRT_FN(GetEnvironmentStringsW);
SPRT_FN(FreeEnvironmentStringsW);
SPRT_FN(Sleep);
SPRT_FN(SleepEx);
SPRT_FN(WaitOnAddress);
SPRT_FN(WakeByAddressSingle);
SPRT_FN(WakeByAddressAll);
SPRT_FN(IsDebuggerPresent);
SPRT_FN(DebugBreak);
SPRT_FN(GetStdHandle);
SPRT_FN(SetStdHandle);
SPRT_FN(GetConsoleScreenBufferInfo);
SPRT_FN(SetConsoleCP);
SPRT_FN(SetConsoleOutputCP);
SPRT_FN(GetConsoleMode);
SPRT_FN(SetConsoleMode);
SPRT_FN(FlushConsoleInputBuffer);
SPRT_FN(AllocConsole);
SPRT_FN(FreeConsole);
SPRT_FN(SetConsoleTextAttribute);
SPRT_FN(SetConsoleCursorPosition);
SPRT_FN(WriteConsoleW);
SPRT_FN(SetConsoleCtrlHandler);
SPRT_FN(GetCurrentProcessorNumber);
SPRT_FN(OutputDebugStringA);
SPRT_FN(OutputDebugStringW);
