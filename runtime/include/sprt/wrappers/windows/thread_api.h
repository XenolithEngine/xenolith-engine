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

#ifndef SPRT_WRAPPERS_WINDOWS_THREAD_API_H_
#define SPRT_WRAPPERS_WINDOWS_THREAD_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/abi/thread_api.h>

/* Clean public names (materialized __SPRT_ values live in abi/thread_api.h) */
#define THREAD_TERMINATE __SPRT_THREAD_TERMINATE
#define THREAD_SUSPEND_RESUME __SPRT_THREAD_SUSPEND_RESUME
#define THREAD_GET_CONTEXT __SPRT_THREAD_GET_CONTEXT
#define THREAD_SET_CONTEXT __SPRT_THREAD_SET_CONTEXT
#define THREAD_QUERY_INFORMATION __SPRT_THREAD_QUERY_INFORMATION
#define THREAD_SET_INFORMATION __SPRT_THREAD_SET_INFORMATION
#define THREAD_SET_THREAD_TOKEN __SPRT_THREAD_SET_THREAD_TOKEN
#define THREAD_IMPERSONATE __SPRT_THREAD_IMPERSONATE
#define THREAD_DIRECT_IMPERSONATION __SPRT_THREAD_DIRECT_IMPERSONATION
#define THREAD_SET_LIMITED_INFORMATION __SPRT_THREAD_SET_LIMITED_INFORMATION
#define THREAD_QUERY_LIMITED_INFORMATION __SPRT_THREAD_QUERY_LIMITED_INFORMATION
#define THREAD_RESUME __SPRT_THREAD_RESUME
#define THREAD_BASE_PRIORITY_LOWRT __SPRT_THREAD_BASE_PRIORITY_LOWRT
#define THREAD_BASE_PRIORITY_MAX __SPRT_THREAD_BASE_PRIORITY_MAX
#define THREAD_BASE_PRIORITY_MIN __SPRT_THREAD_BASE_PRIORITY_MIN
#define THREAD_BASE_PRIORITY_IDLE __SPRT_THREAD_BASE_PRIORITY_IDLE
#define THREAD_PRIORITY_LOWEST __SPRT_THREAD_PRIORITY_LOWEST
#define THREAD_PRIORITY_BELOW_NORMAL __SPRT_THREAD_PRIORITY_BELOW_NORMAL
#define THREAD_PRIORITY_NORMAL __SPRT_THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_HIGHEST __SPRT_THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_ABOVE_NORMAL __SPRT_THREAD_PRIORITY_ABOVE_NORMAL
#define THREAD_PRIORITY_TIME_CRITICAL __SPRT_THREAD_PRIORITY_TIME_CRITICAL
#define THREAD_PRIORITY_IDLE __SPRT_THREAD_PRIORITY_IDLE
#define TH32CS_SNAPHEAPLIST __SPRT_TH32CS_SNAPHEAPLIST
#define TH32CS_SNAPPROCESS __SPRT_TH32CS_SNAPPROCESS
#define TH32CS_SNAPTHREAD __SPRT_TH32CS_SNAPTHREAD
#define TH32CS_SNAPMODULE __SPRT_TH32CS_SNAPMODULE
#define TH32CS_SNAPMODULE32 __SPRT_TH32CS_SNAPMODULE32
#define TH32CS_SNAPALL __SPRT_TH32CS_SNAPALL
#define TH32CS_INHERIT __SPRT_TH32CS_INHERIT
#define WT_EXECUTEDEFAULT __SPRT_WT_EXECUTEDEFAULT
#define WT_EXECUTEINIOTHREAD __SPRT_WT_EXECUTEINIOTHREAD
#define WT_EXECUTEINUITHREAD __SPRT_WT_EXECUTEINUITHREAD
#define WT_EXECUTEINWAITTHREAD __SPRT_WT_EXECUTEINWAITTHREAD
#define WT_EXECUTEONLYONCE __SPRT_WT_EXECUTEONLYONCE
#define WT_EXECUTEINTIMERTHREAD __SPRT_WT_EXECUTEINTIMERTHREAD
#define WT_EXECUTELONGFUNCTION __SPRT_WT_EXECUTELONGFUNCTION
#define WT_EXECUTEINPERSISTENTIOTHREAD __SPRT_WT_EXECUTEINPERSISTENTIOTHREAD
#define WT_EXECUTEINPERSISTENTTHREAD __SPRT_WT_EXECUTEINPERSISTENTTHREAD
#define WT_TRANSFER_IMPERSONATION __SPRT_WT_TRANSFER_IMPERSONATION
#define RTL_SRWLOCK_INIT __SPRT_RTL_SRWLOCK_INIT
#define SRWLOCK_INIT __SPRT_SRWLOCK_INIT
#define FIBER_FLAG_FLOAT_SWITCH __SPRT_FIBER_FLAG_FLOAT_SWITCH
#define TLS_OUT_OF_INDEXES __SPRT_TLS_OUT_OF_INDEXES


__SPRT_BEGIN_DECL

/* ---- Process and Thread Management (processthreadsapi.h) ---- */
/**
 * Creates a thread to execute within the virtual address space of another process.
 * @param lpThreadAttributes Security attributes (NULL = default).
 * @param dwStackSize Initial stack size in bytes (0 = use default).
 * @param lpStartAddress Function to execute.
 * @param lpParameter Parameter passed to the function.
 * @param dwCreationFlags Creation flags.
 * @param lpThreadId Pointer to receive thread identifier.
 * @return Handle to the new thread, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
 */
__SPRT_WIN_IMPORT WINAPI HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
		SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
		DWORD dwCreationFlags, LPDWORD lpThreadId);

/**
 * Retrieves a handle to the specified thread object.
 * @param dwThreadId Thread identifier.
 * @param dwDesiredAccess Desired access (THREAD_QUERY_INFORMATION).
 * @return Handle to the thread, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthread
 */
__SPRT_WIN_IMPORT WINAPI HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle,
		DWORD dwThreadId);

__SPRT_WIN_IMPORT WINAPI HANDLE GetCurrentThread(void);

__SPRT_WIN_IMPORT WINAPI DWORD GetCurrentThreadId(void);

__SPRT_WIN_IMPORT WINAPI VOID GetCurrentThreadStackLimits(PULONG_PTR LowLimit,
		PULONG_PTR HighLimit);

__SPRT_WIN_IMPORT WINAPI int GetThreadPriority(HANDLE hThread);

__SPRT_WIN_IMPORT WINAPI BOOL SetThreadPriority(HANDLE hThread, int nPriority);

__SPRT_WIN_IMPORT WINAPI BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);

__SPRT_WIN_IMPORT WINAPI void ExitThread(DWORD dwExitCode);

__SPRT_WIN_IMPORT WINAPI DWORD ResumeThread(HANDLE hThread);

__SPRT_WIN_IMPORT WINAPI DWORD SuspendThread(HANDLE hThread);

// GetThreadContext/SetThreadContext live in context_api.h — the CONTEXT type is
// defined there and that header is included after this one in the umbrella.

__SPRT_WIN_IMPORT WINAPI DWORD QueueUserAPC(PAPCFUNC pfnAPC, HANDLE hThread, ULONG_PTR dwData);

__SPRT_WIN_IMPORT WINAPI DWORD GetProcessId(HANDLE Process);

BOOL QueueUserAPC2(PAPCFUNC ApcRoutine, HANDLE Thread, ULONG_PTR Data, QUEUE_USER_APC_FLAGS Flags);

__SPRT_WIN_IMPORT WINAPI HRESULT SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);

__SPRT_WIN_IMPORT WINAPI BOOL GetSystemCpuSetInformation(PSYSTEM_CPU_SET_INFORMATION Information,
		ULONG BufferLength, PULONG ReturnedLength, HANDLE Process, ULONG Flags);

__SPRT_WIN_IMPORT WINAPI BOOL SetThreadSelectedCpuSets(HANDLE Thread, const ULONG *CpuSetIds,
		ULONG CpuSetIdCount);

__SPRT_WIN_IMPORT WINAPI BOOL GetThreadSelectedCpuSets(HANDLE Thread, PULONG CpuSetIds,
		ULONG CpuSetIdCount, PULONG RequiredIdCount);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID);

__SPRT_WIN_IMPORT WINAPI BOOL Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

__SPRT_WIN_IMPORT WINAPI BOOL Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

__SPRT_WIN_IMPORT WINAPI VOID InitializeSRWLock(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI VOID ReleaseSRWLockExclusive(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI VOID ReleaseSRWLockShared(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI VOID AcquireSRWLockExclusive(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI VOID AcquireSRWLockShared(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI BOOLEAN TryAcquireSRWLockShared(PSRWLOCK SRWLock);

__SPRT_WIN_IMPORT WINAPI BOOL SwitchToThread(VOID);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes,
		BOOL bInitialOwner, LPCWSTR lpName);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
		LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
		LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes,
		BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes,
		BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);

__SPRT_WIN_IMPORT WINAPI BOOL SetEvent(HANDLE hEvent);

__SPRT_WIN_IMPORT WINAPI BOOL ResetEvent(HANDLE hEvent);

__SPRT_WIN_IMPORT WINAPI BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount,
		LPLONG lpPreviousCount);

__SPRT_WIN_IMPORT WINAPI VOID SwitchToFiber(LPVOID lpFiber);

__SPRT_WIN_IMPORT WINAPI VOID DeleteFiber(LPVOID lpFiber);

__SPRT_WIN_IMPORT WINAPI BOOL ConvertFiberToThread(VOID);

__SPRT_WIN_IMPORT WINAPI LPVOID CreateFiberEx(SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize,
		DWORD dwFlags, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);

__SPRT_WIN_IMPORT WINAPI LPVOID ConvertThreadToFiberEx(LPVOID lpParameter, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI LPVOID CreateFiber(SIZE_T dwStackSize,
		LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);

__SPRT_WIN_IMPORT WINAPI LPVOID ConvertThreadToFiber(LPVOID lpParameter);

__SPRT_WIN_IMPORT WINAPI DWORD TlsAlloc(VOID);

__SPRT_WIN_IMPORT WINAPI LPVOID TlsGetValue(DWORD dwTlsIndex);

__SPRT_WIN_IMPORT WINAPI BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);

__SPRT_WIN_IMPORT WINAPI BOOL TlsFree(DWORD dwTlsIndex);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateTimerQueue(VOID);

__SPRT_WIN_IMPORT WINAPI BOOL CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue,
		WAITORTIMERCALLBACK Callback, PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags);

__SPRT_WIN_IMPORT WINAPI BOOL ChangeTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, ULONG DueTime,
		ULONG Period);

__SPRT_WIN_IMPORT BOOL WINAPI DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer,
		HANDLE CompletionEvent);

__SPRT_WIN_IMPORT BOOL WINAPI DeleteTimerQueue(HANDLE TimerQueue);

__SPRT_WIN_IMPORT BOOL WINAPI DeleteTimerQueueEx(HANDLE TimerQueue, HANDLE CompletionEvent);

// Legacy thread-pool wait: invokes Callback (on a pool thread) when hObject is signalled. Used as
// the fallback for waiting on a process/event when the NtAssociateWaitCompletionPacket family is
// unavailable (e.g. Wine); see dispatch's process backend.
__SPRT_WIN_IMPORT WINAPI BOOL RegisterWaitForSingleObject(PHANDLE phNewWaitObject, HANDLE hObject,
		WAITORTIMERCALLBACK Callback, PVOID Context, ULONG dwMilliseconds, ULONG dwFlags);

__SPRT_WIN_IMPORT WINAPI BOOL UnregisterWaitEx(HANDLE WaitHandle, HANDLE CompletionEvent);

__SPRT_WIN_IMPORT WINAPI BOOL UnregisterWait(HANDLE WaitHandle);


#ifdef UNICODE
#define CreateSemaphore  CreateSemaphoreW
#define CreateEvent  CreateEventW
#endif

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_THREAD_API_H_
