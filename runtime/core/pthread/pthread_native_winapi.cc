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

#define __SPRT_BUILD 1

/* SPRT pthread implementation over winapi */

#include "pthread_impl.h"

#if SPRT_WINDOWS

#include "pthread_impl.h"
#include "../windows/sched_win.h"

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/windows.h>

namespace sprt::platform {

int lastErrorToErrno(unsigned long winerr);

}

typedef void(__cdecl *__funcptr)(void);

__SPRT_C_FUNC int __tlregdtor(__funcptr fn) __SPRT_NOEXCEPT;

namespace sprt::_thread::native {

static uint64_t __getNativeThreadId() { return GetCurrentThreadId(); }

static void __registerForDestruction(void (*fn)(void)) { __tlregdtor(fn); }

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr,
		__thread_pool *pool) {
	DWORD id = 0;
	// Thread-local variables must be able to locate this thread's
	// data before the main thread function gains control.
	// We create thread as suspended, then register it's global data, and then resume it
	unique_lock globalLock(pool->mutex);

	auto handle = (HANDLE)CreateThread(nullptr, attr->stackSize, &__runthead, thread,
			((thread->attr.stackSize > 0) ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0)
					| CREATE_SUSPENDED,
			&id);
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		return platform::lastErrorToErrno(GetLastError());
	} else {
		// attach thread's data as globally available by it's id
		thread->threadId = id;
		__attachNativeThread(thread, reinterpret_cast<void *>(handle), id, globalLock);
		globalLock.unlock();

		// Resume thread
		// this will run thread-local initializers, then thread main func
		if (!hasFlag(attr->attr, ThreadAttrFlags::CreateSuspended)) {
			ResumeThread(handle);
		}

		return 0;
	}
}

static void __closeNativeHandle(void *handle) {
	CloseHandle((HANDLE)handle); //
}

static void __initNativeHandle(thread_t *thread) {
	if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
				&thread->handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		__deallocateThread(thread);
		return;
	}

	GetCurrentThreadStackLimits(&thread->lowStack, &thread->highStack);

	thread->attr.stackSize = (thread->highStack - thread->lowStack);
	thread->attr.stack = (void *)thread->lowStack;

	// SetThreadPriority if it's requested, or get current priority for unmanaged and if
	// priority was not explicitly set
	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)
			&& hasFlagAll(thread->attr.attr, ThreadAttrFlags::ExplicitSched)) {
		SetThreadPriority(thread->handle, __mapPriority(thread->attr.prio));
	} else {
		thread->attr.prio = __unmapPriority(GetThreadPriority(thread->handle));
	}
}

static bool __isNativeHandleValid(thread_t *thread) {
	DWORD dwInfo = 0;
	if (!thread->handle || !GetHandleInformation(thread->handle, &dwInfo)) {
		return false;
	}
	return true;
}

static void __exitNativeThread(void *ret) { ExitThread((ptrdiff_t)ret); }

static void __pthread_cancel_callback(ULONG_PTR Parameter) { thread_t::exit((void *)Parameter); }

static int __cancelThreadAsync(thread_t *thread) {
	// Break user's context to call __pthread_cancel_callback on target thread
	// QueueUserAPC2 will unwind the stack for current user context
	// Then, pthread_exit should unwind the remaining stack with longjmp, if possible.

	QueueUserAPC2(__pthread_cancel_callback, thread->handle, (ULONG_PTR)__SPRT_PTHREAD_CANCELED,
			QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC);
	return EINVAL;
}

static int __applyThreadPrio(thread_t *thread, int32_t dprio) {
	if (hasFlag(thread->attr.attr, ThreadAttrFlags::PrioMask)) {
		if (!SetThreadPriority(thread->handle, THREAD_PRIORITY_NORMAL)) {
			return EINVAL;
		}
	} else {
		if (!SetThreadPriority(thread->handle, __mapPriority(thread->attr.prio))) {
			return EINVAL;
		}
	}
	return 0;
}

SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) { return false; }

SPRT_UNUSED static bool validate_attr_setstacksize(size_t size) { return true; }

SPRT_UNUSED static bool validate_attr_setstack(void *, size_t) { return false; }

SPRT_UNUSED static bool validate_attr_setschedpolicy(int) { return true; }

SPRT_UNUSED static bool validate_attr_setschedparam(int) { return true; }

SPRT_UNUSED static bool validate_attr_setinheritsched(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprioceiling(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprotocol(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_condattr_setclock(int clock) {
	switch (clock) {
	case __SPRT_CLOCK_REALTIME:
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME);
		break;
	case __SPRT_CLOCK_MONOTONIC: return true; break;
	}
	return true;
}

SPRT_UNUSED static bool validate_condattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_rwlockattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_barrierattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}


} // namespace sprt::_thread::native

namespace sprt::_thread {

static bool __fillCpuSet(SpanView<ULONG> ids, size_t cpusetsize, __sprt_cpu_set_t *set) {
	ULONG bufferSize = 0;
	if (!GetSystemCpuSetInformation(nullptr, 0, &bufferSize, GetCurrentProcess(), 0)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return false;
		}
	}

	if (bufferSize == 0) {
		return false;
	}

	auto buf = (SYSTEM_CPU_SET_INFORMATION *)__sprt_malloc(bufferSize);
	if (!GetSystemCpuSetInformation(buf, bufferSize, &bufferSize, GetCurrentProcess(), 0)) {
		__sprt_freea(buf);
		return false;
	}

	uint32_t idx = 0;
	DWORD offset = 0;
	auto target = (SYSTEM_CPU_SET_INFORMATION *)buf;

	while (offset < bufferSize) {
		if (target->Type == 0) {
			for (auto &it : ids) {
				if (it == target->CpuSet.Id) {
					__SPRT_CPU_SET_S(idx, cpusetsize, set);
					break;
				}
			}
		}

		offset += buf->Size;
		target = (SYSTEM_CPU_SET_INFORMATION *)(((char *)target) + target->Size);
		++idx;
	}

	__sprt_freea(buf);
	return true;
}

static uint32_t __fillCpuIds(ULONG *cpuids, size_t cpusetsize, const __sprt_cpu_set_t *set) {
	ULONG bufferSize = 0;
	if (!GetSystemCpuSetInformation(nullptr, 0, &bufferSize, GetCurrentProcess(), 0)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return 0;
		}
	}

	if (bufferSize == 0) {
		return 0;
	}

	auto buf = (SYSTEM_CPU_SET_INFORMATION *)__sprt_malloc(bufferSize);
	if (!GetSystemCpuSetInformation(buf, bufferSize, &bufferSize, GetCurrentProcess(), 0)) {
		__sprt_freea(buf);
		return false;
	}

	uint32_t ncpus = 0;
	uint32_t idx = 0;
	DWORD offset = 0;
	auto target = (SYSTEM_CPU_SET_INFORMATION *)buf;

	while (offset < bufferSize) {
		if (target->Type == 0) {
			if (__SPRT_CPU_ISSET_S(idx, cpusetsize, set)) {
				*cpuids++ = target->CpuSet.Id;
				++ncpus;
			}
		}

		offset += buf->Size;
		target = (SYSTEM_CPU_SET_INFORMATION *)(((char *)target) + target->Size);
		++idx;
	}

	__sprt_freea(buf);
	return ncpus;
}

int thread_t::getcpuclockid(__sprt_clockid_t *clock) const {
	if (threadId & uint32_t(1U << 31)) {
		return ESRCH;
	}

	*clock = (static_cast<__sprt_clockid_t>(threadId) & 0x7FFF'FFFF) | 0x8000'0000;
	return 0;
}

int thread_t::getaffinity(__SPRT_ID(size_t) cpusetsize, __SPRT_ID(cpu_set_t) * set) {
	SYSTEM_INFO sysInfo;
	GetNativeSystemInfo(&sysInfo);

	DWORD kernelSetSize =
			sysInfo.dwNumberOfProcessors / 8 + ((sysInfo.dwNumberOfProcessors % 8) ? 1 : 0);

	if (cpusetsize < kernelSetSize) {
		return EINVAL;
	}

	ULONG nCpus = 0;
	if (!GetThreadSelectedCpuSets((HANDLE)handle, nullptr, 0, &nCpus)) {
		return EINVAL;
	}

	__SPRT_CPU_ZERO_S(cpusetsize, set);
	if (nCpus == 0) {
		return 0;
	}

	auto cpus = __sprt_typed_malloca(ULONG, nCpus);

	if (!GetThreadSelectedCpuSets((HANDLE)handle, cpus, nCpus, &nCpus)) {
		__sprt_freea(cpus);
		return EINVAL;
	}

	if (__fillCpuSet(SpanView<ULONG>(cpus, nCpus), cpusetsize, set)) {
		__sprt_freea(cpus);
		return 0;
	}

	__sprt_freea(cpus);
	return EINVAL;
}

int thread_t::setaffinity(__SPRT_ID(size_t) cpusetsize, const __SPRT_ID(cpu_set_t) * set) {
	auto nCpus = __SPRT_CPU_COUNT_S(cpusetsize, set);
	if (nCpus == 0) {
		return EINVAL;
	}

	auto cpus = __sprt_typed_malloca(ULONG, nCpus);

	auto nset = __fillCpuIds(cpus, cpusetsize, set);
	if (nset == 0 || nset != nCpus) {
		__sprt_freea(cpus);
		return EINVAL;
	}

	if (SetThreadSelectedCpuSets((HANDLE)handle, cpus, nCpus)) {
		__sprt_freea(cpus);
		return 0;
	}

	__sprt_freea(cpus);
	return EINVAL;
}

int thread_t::setname_native(const char *name) {
	int ret = 0;
	unicode::toUtf16([&](WideStringView wname) {
		auto hr = SetThreadDescription((HANDLE)handle, (wchar_t *)wname.data());
		if (FAILED(hr)) {
			ret = EINVAL;
		}
	}, StringView(name));
	return ret;
}

} // namespace sprt::_thread

__SPRT_C_FUNC int sigprocmask(int how, const __SPRT_ID(sigset_t) * set,
		__SPRT_ID(sigset_t) * oldset) __SPRT_NOEXCEPT;

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(
		pthread_sigmask)(int how, const __SPRT_ID(sigset_t) * set, __SPRT_ID(sigset_t) * oldset) {
	return sigprocmask(how, set, oldset);
}

} // namespace sprt

#endif // SPRT_WINDOWS
