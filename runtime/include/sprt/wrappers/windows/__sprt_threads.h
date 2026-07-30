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

#ifndef SPRT_WRAPPERS_WINDOWS___SPRT_THREADS_H_
#define SPRT_WRAPPERS_WINDOWS___SPRT_THREADS_H_

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/c/bits/__sprt_uintptr_t.h>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_errno.h>

typedef struct _RTL_CRITICAL_SECTION {
	__SPRT_ID(pthread_mutex_t) __sprt_mutex;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

typedef __SPRT_ID(pthread_cond_t) CONDITION_VARIABLE, *PCONDITION_VARIABLE, *LPCONDITION_VARIABLE;

typedef __SPRT_ID(sprt_rlock_t) INIT_ONCE, *PINIT_ONCE, *LPINIT_ONCE;

#define INIT_ONCE_STATIC_INIT {0}

typedef BOOL(WINAPI *PINIT_ONCE_FN)(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context);

// Low bits of the INIT_ONCE word hold the state; the rest holds the context pointer, so a
// stored context must be aligned to (STATE_MASK + 1) == 8 bytes (its low 3 bits free). This
// mirrors Win32's INIT_ONCE_CTX_RESERVED_BITS scheme (2 bits there; 3 here).
#define __SPRT_INIT_ONCE_VALUE_BIT 1
#define __SPRT_INIT_ONCE_WAIT_BIT 2
#define __SPRT_INIT_ONCE_COMPLETE_BIT 4
#define __SPRT_INIT_ONCE_STATE_MASK 7

__SPRT_BEGIN_DECL

/*
	RECURSIVE, not the default mutex kind.

	A Win32 critical section is re-entrant by definition: "a thread that owns it can
	enter it repeatedly without blocking".
*/
SPRT_FORCEINLINE VOID __sprt_init_critical_section(LPCRITICAL_SECTION lpCriticalSection) {
	__SPRT_ID(pthread_mutexattr_t) attr;
	__sprt_pthread_mutexattr_init(&attr);
	__sprt_pthread_mutexattr_settype(&attr, __SPRT_PTHREAD_MUTEX_RECURSIVE);
	__sprt_pthread_mutex_init(&lpCriticalSection->__sprt_mutex, &attr);
	__sprt_pthread_mutexattr_destroy(&attr);
}

SPRT_FORCEINLINE VOID InitializeCriticalSection(
		LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_init_critical_section(lpCriticalSection);
}

SPRT_FORCEINLINE VOID InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection,
		DWORD dwSpinCount, DWORD Flags) __SPRT_NOEXCEPT {
	__sprt_init_critical_section(lpCriticalSection);
}

SPRT_FORCEINLINE BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection,
		DWORD v) __SPRT_NOEXCEPT {
	__sprt_init_critical_section(lpCriticalSection);
	return TRUE;
}

SPRT_FORCEINLINE VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_lock(&lpCriticalSection->__sprt_mutex);
}

SPRT_FORCEINLINE VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_unlock(&lpCriticalSection->__sprt_mutex);
}

SPRT_FORCEINLINE BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
	return __sprt_pthread_mutex_trylock(&lpCriticalSection->__sprt_mutex) == 0;
}

SPRT_FORCEINLINE VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_destroy(&lpCriticalSection->__sprt_mutex);
}

SPRT_FORCEINLINE VOID InitializeConditionVariable(
		PCONDITION_VARIABLE ConditionVariable) __SPRT_NOEXCEPT {
	__sprt_pthread_cond_init(ConditionVariable, __SPRT_NULL);
}

SPRT_FORCEINLINE VOID WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable) __SPRT_NOEXCEPT {
	__sprt_pthread_cond_signal(ConditionVariable);
}

SPRT_FORCEINLINE VOID WakeAllConditionVariable(
		PCONDITION_VARIABLE ConditionVariable) __SPRT_NOEXCEPT {
	__sprt_pthread_cond_broadcast(ConditionVariable);
}

SPRT_FORCEINLINE BOOL SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable,
		PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds) __SPRT_NOEXCEPT {
	int ret = 0;
	if (dwMilliseconds == INFINITE) {
		ret = __sprt_pthread_cond_wait(ConditionVariable, &CriticalSection->__sprt_mutex);
	} else {
		// clang-format off
		struct __SPRT_TIMESPEC_NAME ts;
		ts.tv_sec = dwMilliseconds / 1000;
		ts.tv_nsec = dwMilliseconds % 1000 * 1000000;
		// clang-format on
		ret = __sprt_pthread_cond_timedwait_relative_np(ConditionVariable,
				&CriticalSection->__sprt_mutex, &ts);
	}

	if (ret == 0) {
		return TRUE;
	}
	if (ret == ETIMEDOUT) {
		SetLastError(ERROR_TIMEOUT);
	}
	return FALSE;
}


// Synchronous one-time initialization ([synchapi.h] InitOnceExecuteOnce). Per the Win32
// contract the callback's BOOL result is authoritative: TRUE marks the block permanently
// initialized and returns TRUE to every caller; FALSE leaves the block UNinitialized, so a
// later (or currently waiting) caller re-runs the callback, and reports FALSE to this
// caller. GetLastError from a failed callback is preserved (nothing is set on the failure
// path). The context the callback stores in *Context is persisted in the high bits of the
// INIT_ONCE word and handed back to EVERY caller (initializer, waiters and late arrivals);
// it must be aligned to 8 bytes (see __SPRT_INIT_ONCE_STATE_MASK). The word is waited on
// with sprt_rlock_wait (a 64-bit WaitOnAddress); completion is broadcast with
// WakeByAddressAll rather than sprt_rlock_wake, because the latter resets the word to 0 and
// wakes only one waiter, which would drop the published context and starve other waiters.
// Asynchronous mode (INIT_ONCE_ASYNC / InitOnceBeginInitialize) is not modelled.
SPRT_FORCEINLINE BOOL InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn,
		PVOID Parameter, LPVOID *Context) {
	for (;;) {
		__SPRT_ID(uint64_t)
		val = __atomic_fetch_or(&InitOnce->u64, __SPRT_INIT_ONCE_VALUE_BIT, __ATOMIC_SEQ_CST);
		if (val == 0) {
			// The First One: run the initializer against a local context cell so we can
			// persist whatever it stores.
			PVOID ctx = __SPRT_NULL;
			if (InitFn(InitOnce, Parameter, &ctx)) {
				// Success: publish the context pointer + COMPLETE and broadcast to waiters.
				__SPRT_ID(uint64_t)
				done = (__SPRT_ID(uint64_t))(__SPRT_ID(uintptr_t))ctx
						| __SPRT_INIT_ONCE_COMPLETE_BIT;
				val = __atomic_exchange_n(&InitOnce->u64, done, __ATOMIC_SEQ_CST);
				if (val & __SPRT_INIT_ONCE_WAIT_BIT) {
					WakeByAddressAll(&InitOnce->u64);
				}
				if (Context) {
					*Context = ctx;
				}
				return TRUE;
			}
			// Failure: reset to the uninitialized state so another caller can retry, wake
			// any waiters so they re-attempt, and report the failure to this caller.
			val = __atomic_exchange_n(&InitOnce->u64, (__SPRT_ID(uint64_t))0, __ATOMIC_SEQ_CST);
			if (val & __SPRT_INIT_ONCE_WAIT_BIT) {
				WakeByAddressAll(&InitOnce->u64);
			}
			return FALSE;
		} else if (val & __SPRT_INIT_ONCE_COMPLETE_BIT) {
			// Already initialized: hand back the stored context.
			if (Context) {
				*Context = (PVOID)(__SPRT_ID(uintptr_t))(
						val & ~(__SPRT_ID(uint64_t))__SPRT_INIT_ONCE_STATE_MASK);
			}
			return TRUE;
		} else {
			// Initialization is in progress: register as a waiter and block on the full
			// 64-bit word until it either completes (COMPLETE set) or fails (state reset ->
			// VALUE bit cleared).
			val = __atomic_fetch_or(&InitOnce->u64, __SPRT_INIT_ONCE_WAIT_BIT, __ATOMIC_SEQ_CST)
					| __SPRT_INIT_ONCE_WAIT_BIT;
			while ((val & __SPRT_INIT_ONCE_COMPLETE_BIT) == 0
					&& (val & __SPRT_INIT_ONCE_VALUE_BIT) != 0) {
				INIT_ONCE expected;
				expected.u64 = val;
				__sprt_sprt_rlock_wait(InitOnce, &expected, __SPRT_SPRT_TIMEOUT_INFINITE,
						__SPRT_SPRT_LOCK_FLAG_NONE);
				val = __atomic_load_n(&InitOnce->u64, __ATOMIC_SEQ_CST);
			}
			if (val & __SPRT_INIT_ONCE_COMPLETE_BIT) {
				if (Context) {
					*Context = (PVOID)(__SPRT_ID(uintptr_t))(
							val & ~(__SPRT_ID(uint64_t))__SPRT_INIT_ONCE_STATE_MASK);
				}
				return TRUE;
			}
			// The in-progress initialization failed and reset the state; loop to retry.
		}
	}
}

typedef unsigned(WINAPI *_beginthreadex_proc_type)(void *);

WINAPI __SPRT_ID(uintptr_t)
		_beginthreadex(void *_Security, unsigned _StackSize, _beginthreadex_proc_type _StartAddress,
				void *_ArgList, unsigned _InitFlag, unsigned *_ThrdAddr) __SPRT_NOEXCEPT;

WINAPI void _endthreadex(unsigned _ReturnCode) __SPRT_NOEXCEPT;

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_THREADS_H_
