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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_THREAD_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_THREAD_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

/*
 * Windows threading and timer types
 */

// clang-format off
#define __SPRT_THREAD_TERMINATE                 (0x0001)
#define __SPRT_THREAD_SUSPEND_RESUME            (0x0002)
#define __SPRT_THREAD_GET_CONTEXT               (0x0008)
#define __SPRT_THREAD_SET_CONTEXT               (0x0010)
#define __SPRT_THREAD_QUERY_INFORMATION         (0x0040)
#define __SPRT_THREAD_SET_INFORMATION           (0x0020)
#define __SPRT_THREAD_SET_THREAD_TOKEN          (0x0080)
#define __SPRT_THREAD_IMPERSONATE               (0x0100)
#define __SPRT_THREAD_DIRECT_IMPERSONATION      (0x0200)
#define __SPRT_THREAD_SET_LIMITED_INFORMATION   (0x0400)  // winnt
#define __SPRT_THREAD_QUERY_LIMITED_INFORMATION (0x0800)  // winnt
#define __SPRT_THREAD_RESUME                    (0x1000)  // winnt

#define __SPRT_THREAD_BASE_PRIORITY_LOWRT  15  // value that gets a thread to LowRealtime-1
#define __SPRT_THREAD_BASE_PRIORITY_MAX    2   // maximum thread base priority boost
#define __SPRT_THREAD_BASE_PRIORITY_MIN    (-2)  // minimum thread base priority boost
#define __SPRT_THREAD_BASE_PRIORITY_IDLE   (-15) // value that gets a thread to idle

#define __SPRT_THREAD_PRIORITY_LOWEST          __SPRT_THREAD_BASE_PRIORITY_MIN
#define __SPRT_THREAD_PRIORITY_BELOW_NORMAL    (__SPRT_THREAD_PRIORITY_LOWEST+1)
#define __SPRT_THREAD_PRIORITY_NORMAL          0
#define __SPRT_THREAD_PRIORITY_HIGHEST         __SPRT_THREAD_BASE_PRIORITY_MAX
#define __SPRT_THREAD_PRIORITY_ABOVE_NORMAL    (__SPRT_THREAD_PRIORITY_HIGHEST-1)

#define __SPRT_THREAD_PRIORITY_TIME_CRITICAL   __SPRT_THREAD_BASE_PRIORITY_LOWRT
#define __SPRT_THREAD_PRIORITY_IDLE            __SPRT_THREAD_BASE_PRIORITY_IDLE

#define __SPRT_TH32CS_SNAPHEAPLIST 0x00000001
#define __SPRT_TH32CS_SNAPPROCESS  0x00000002
#define __SPRT_TH32CS_SNAPTHREAD   0x00000004
#define __SPRT_TH32CS_SNAPMODULE   0x00000008
#define __SPRT_TH32CS_SNAPMODULE32 0x00000010
#define __SPRT_TH32CS_SNAPALL      (__SPRT_TH32CS_SNAPHEAPLIST | __SPRT_TH32CS_SNAPPROCESS | __SPRT_TH32CS_SNAPTHREAD | __SPRT_TH32CS_SNAPMODULE)
#define __SPRT_TH32CS_INHERIT      0x80000000

#define __SPRT_WT_EXECUTEDEFAULT       0x00000000                           
#define __SPRT_WT_EXECUTEINIOTHREAD    0x00000001                           
#define __SPRT_WT_EXECUTEINUITHREAD    0x00000002                           
#define __SPRT_WT_EXECUTEINWAITTHREAD  0x00000004                           
#define __SPRT_WT_EXECUTEONLYONCE      0x00000008                           
#define __SPRT_WT_EXECUTEINTIMERTHREAD 0x00000020                           
#define __SPRT_WT_EXECUTELONGFUNCTION  0x00000010                           
#define __SPRT_WT_EXECUTEINPERSISTENTIOTHREAD  0x00000040                   
#define __SPRT_WT_EXECUTEINPERSISTENTTHREAD 0x00000080                      
#define __SPRT_WT_TRANSFER_IMPERSONATION 0x00000100            

#define __SPRT_RTL_SRWLOCK_INIT {0}
#define __SPRT_SRWLOCK_INIT __SPRT_RTL_SRWLOCK_INIT

#define __SPRT_FIBER_FLAG_FLOAT_SWITCH 0x1     // context switch floating point

#define __SPRT_TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

// clang-format on

/* Timer APC routine callback type */
typedef void (*PTIMERAPCROUTINE)(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue,
		DWORD dwTimerHighValue);

/* Thread start routine - function pointer for thread entry points */
typedef DWORD(__SPRT_WINAPI *LPTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);

typedef void(__SPRT_WINAPI *PAPCFUNC)(DWORD_PTR dwParam);

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

typedef VOID(__SPRT_WINAPI *PFIBER_START_ROUTINE)(LPVOID lpFiberParameter);
typedef PFIBER_START_ROUTINE LPFIBER_START_ROUTINE;

typedef VOID(__SPRT_NTAPI *WAITORTIMERCALLBACKFUNC)(PVOID, BOOLEAN);
typedef VOID(__SPRT_NTAPI *WORKERCALLBACKFUNC)(PVOID);
typedef VOID(__SPRT_NTAPI *APC_CALLBACK_FUNCTION)(DWORD, PVOID, PVOID);
typedef WAITORTIMERCALLBACKFUNC WAITORTIMERCALLBACK;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_THREAD_API_H_
