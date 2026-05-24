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

/*
 * Windows threading and timer types
 */

// clang-format off
#define THREAD_TERMINATE                 (0x0001)
#define THREAD_SUSPEND_RESUME            (0x0002)
#define THREAD_GET_CONTEXT               (0x0008)
#define THREAD_SET_CONTEXT               (0x0010)
#define THREAD_QUERY_INFORMATION         (0x0040)
#define THREAD_SET_INFORMATION           (0x0020)
#define THREAD_SET_THREAD_TOKEN          (0x0080)
#define THREAD_IMPERSONATE               (0x0100)
#define THREAD_DIRECT_IMPERSONATION      (0x0200)
#define THREAD_SET_LIMITED_INFORMATION   (0x0400)  // winnt
#define THREAD_QUERY_LIMITED_INFORMATION (0x0800)  // winnt
#define THREAD_RESUME                    (0x1000)  // winnt

#define THREAD_BASE_PRIORITY_LOWRT  15  // value that gets a thread to LowRealtime-1
#define THREAD_BASE_PRIORITY_MAX    2   // maximum thread base priority boost
#define THREAD_BASE_PRIORITY_MIN    (-2)  // minimum thread base priority boost
#define THREAD_BASE_PRIORITY_IDLE   (-15) // value that gets a thread to idle

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)

#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

#define TH32CS_SNAPHEAPLIST 0x00000001
#define TH32CS_SNAPPROCESS  0x00000002
#define TH32CS_SNAPTHREAD   0x00000004
#define TH32CS_SNAPMODULE   0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
#define TH32CS_SNAPALL      (TH32CS_SNAPHEAPLIST | TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE)
#define TH32CS_INHERIT      0x80000000

#define RTL_SRWLOCK_INIT {0}
#define SRWLOCK_INIT RTL_SRWLOCK_INIT

#define FIBER_FLAG_FLOAT_SWITCH 0x1     // context switch floating point

#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

// clang-format on

/* Timer APC routine callback type */
typedef void (*PTIMERAPCROUTINE)(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue,
		DWORD dwTimerHighValue);

/* Thread start routine - function pointer for thread entry points */
typedef DWORD(WINAPI *LPTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);

typedef void(WINAPI *PAPCFUNC)(DWORD_PTR dwParam);

// clang-format off
typedef enum _QUEUE_USER_APC_FLAGS {
	QUEUE_USER_APC_FLAGS_NONE = 0x00000000,
	QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC = 0x00000001,
	QUEUE_USER_APC_CALLBACK_DATA_CONTEXT = 0x00010000,
} QUEUE_USER_APC_FLAGS;
// clang-format on


typedef enum _CPU_SET_INFORMATION_TYPE {
	CpuSetInformation
} CPU_SET_INFORMATION_TYPE, *PCPU_SET_INFORMATION_TYPE;

struct _SYSTEM_CPU_SET_INFORMATION {
	DWORD Size;
	CPU_SET_INFORMATION_TYPE Type;
	union {
		struct {
			DWORD Id;
			WORD Group;
			BYTE LogicalProcessorIndex;
			BYTE CoreIndex;
			BYTE LastLevelCacheIndex;
			BYTE NumaNodeIndex;
			BYTE EfficiencyClass;
			union {
				BYTE AllFlags;
				struct {
					BYTE Parked					  : 1;
					BYTE Allocated				  : 1;
					BYTE AllocatedToTargetProcess : 1;
					BYTE RealTime				  : 1;
					BYTE ReservedFlags			  : 4;
				};
			};

			union {
				DWORD Reserved;
				BYTE SchedulingClass;
			};

			DWORD64 AllocationTag;
		} CpuSet;
	};
};

typedef struct tagTHREADENTRY32 {
	DWORD dwSize;
	DWORD cntUsage;
	DWORD th32ThreadID; // this thread
	DWORD th32OwnerProcessID; // Process this thread is associated with
	LONG tpBasePri;
	LONG tpDeltaPri;
	DWORD dwFlags;
} THREADENTRY32;
typedef THREADENTRY32 *PTHREADENTRY32;
typedef THREADENTRY32 *LPTHREADENTRY32;

typedef struct _SYSTEM_CPU_SET_INFORMATION SYSTEM_CPU_SET_INFORMATION, *PSYSTEM_CPU_SET_INFORMATION;

typedef struct _RTL_SRWLOCK {
	PVOID Ptr;
} RTL_SRWLOCK, *PRTL_SRWLOCK;

typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

typedef VOID(WINAPI *PFIBER_START_ROUTINE)(LPVOID lpFiberParameter);
typedef PFIBER_START_ROUTINE LPFIBER_START_ROUTINE;

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

#ifdef UNICODE
#define CreateSemaphore  CreateSemaphoreW
#define CreateEvent  CreateEventW
#endif

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_THREAD_API_H_
