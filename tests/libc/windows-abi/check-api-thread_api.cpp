// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/thread_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/thread_api.h>
#include "api_check.h"

#include <windows.h>
#include <tlhelp32.h>  // CreateToolhelp32Snapshot / Thread32First / Thread32Next

SPRT_FN(CreateThread);
SPRT_FN(OpenThread);
SPRT_FN(GetCurrentThread);
SPRT_FN(GetCurrentThreadId);
SPRT_FN(GetCurrentThreadStackLimits);
SPRT_FN(GetThreadPriority);
SPRT_FN(SetThreadPriority);
SPRT_FN(GetExitCodeThread);
SPRT_FN(ExitThread);
SPRT_FN(ResumeThread);
SPRT_FN(SuspendThread);
SPRT_FN(QueueUserAPC);
SPRT_FN(GetProcessId);
SPRT_FN(QueueUserAPC2);
SPRT_FN(SetThreadDescription);
SPRT_FN(GetSystemCpuSetInformation);
SPRT_FN(SetThreadSelectedCpuSets);
SPRT_FN(GetThreadSelectedCpuSets);
SPRT_FN(CreateToolhelp32Snapshot);
SPRT_FN(Thread32First);
SPRT_FN(Thread32Next);
SPRT_FN(InitializeSRWLock);
SPRT_FN(ReleaseSRWLockExclusive);
SPRT_FN(ReleaseSRWLockShared);
SPRT_FN(AcquireSRWLockExclusive);
SPRT_FN(AcquireSRWLockShared);
SPRT_FN(TryAcquireSRWLockExclusive);
SPRT_FN(TryAcquireSRWLockShared);
SPRT_FN(SwitchToThread);
SPRT_FN(CreateMutexW);
SPRT_FN(CreateSemaphoreW);
SPRT_FN(CreateSemaphoreA);
SPRT_FN(OpenSemaphoreW);
SPRT_FN(OpenSemaphoreA);
SPRT_FN(CreateEventW);
SPRT_FN(CreateEventA);
SPRT_FN(SetEvent);
SPRT_FN(ResetEvent);
SPRT_FN(ReleaseSemaphore);
SPRT_FN(SwitchToFiber);
SPRT_FN(DeleteFiber);
SPRT_FN(ConvertFiberToThread);
SPRT_FN(CreateFiberEx);
SPRT_FN(ConvertThreadToFiberEx);
SPRT_FN(CreateFiber);
SPRT_FN(ConvertThreadToFiber);
SPRT_FN(TlsAlloc);
SPRT_FN(TlsGetValue);
SPRT_FN(TlsSetValue);
SPRT_FN(TlsFree);
SPRT_FN(CreateTimerQueue);
SPRT_FN(CreateTimerQueueTimer);
SPRT_FN(ChangeTimerQueueTimer);
SPRT_FN(DeleteTimerQueueTimer);
SPRT_FN(DeleteTimerQueue);
SPRT_FN(DeleteTimerQueueEx);
SPRT_FN(RegisterWaitForSingleObject);
SPRT_FN(UnregisterWaitEx);
SPRT_FN(UnregisterWait);
