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
#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_errno.h>

typedef __SPRT_ID(pthread_mutex_t) CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

typedef __SPRT_ID(pthread_cond_t) CONDITION_VARIABLE, *PCONDITION_VARIABLE, *LPCONDITION_VARIABLE;

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE VOID InitializeCriticalSection(
		LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_init(lpCriticalSection, __SPRT_NULL);
}

SPRT_FORCEINLINE BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection,
		DWORD v) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_init(lpCriticalSection, __SPRT_NULL);
	return TRUE;
}

SPRT_FORCEINLINE VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_lock(lpCriticalSection);
}

SPRT_FORCEINLINE VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_unlock(lpCriticalSection);
}

SPRT_FORCEINLINE BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
	return __sprt_pthread_mutex_trylock(lpCriticalSection) == 0;
}

SPRT_FORCEINLINE VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection) __SPRT_NOEXCEPT {
	__sprt_pthread_mutex_destroy(lpCriticalSection);
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
		ret = __sprt_pthread_cond_wait(ConditionVariable, CriticalSection);
	} else {
		// clang-format off
		struct __SPRT_TIMESPEC_NAME ts;
		ts.tv_sec = dwMilliseconds / 1000;
		ts.tv_nsec = dwMilliseconds % 1000 * 1000000;
		// clang-format on
		ret = __sprt_pthread_cond_timedwait_relative_np(ConditionVariable, CriticalSection, &ts);
	}

	if (ret == 0) {
		return TRUE;
	}
	if (ret == ETIMEDOUT) {
		SetLastError(ERROR_TIMEOUT);
	}
	return FALSE;
}

typedef unsigned(WINAPI *_beginthreadex_proc_type)(void *);

WINAPI __SPRT_ID(uintptr_t)
		_beginthreadex(void *_Security, unsigned _StackSize, _beginthreadex_proc_type _StartAddress,
				void *_ArgList, unsigned _InitFlag, unsigned *_ThrdAddr) __SPRT_NOEXCEPT;

WINAPI void _endthreadex(unsigned _ReturnCode) __SPRT_NOEXCEPT;

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_THREADS_H_
