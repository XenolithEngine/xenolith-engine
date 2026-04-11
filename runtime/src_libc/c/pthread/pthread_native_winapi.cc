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

#include "pthread_native.h"

#if SPRT_WINDOWS

namespace sprt::_thread {

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr) {
	auto handle = (HANDLE)_beginthreadex(nullptr, attr->stackSize, &__runthead, thread,
			((thread->attr.stackSize > 0) ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0),
			&thread->threadId);
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		return platform::lastErrorToErrno(GetLastError());
	} else {
		__attachNativeThread(thread, reinterpret_cast<void *>(pthread));
		return 0;
	}
}

static void __closeNativeHandle(void *handle) {
	CloseHandle(thread->handle); //
}

static void __initNativeHandle(thread_t *thread) {
	if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
				&thread->handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		__deallocateThread(thread);
		return;
	}

	GetCurrentThreadStackLimits(&thread->lowStack, &thread->highStack);

	thread->attr.stackSize = (highStack - lowStack);

	// SetThreadPriority if it's requested, or get current priority for unmanaged and if
	// priority was not explicitly set
	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)
			&& hasFlagAll(thread->attr.attr,
					ThreadAttrFlags::ApplyPrio | ThreadAttrFlags::ExplicitSched)) {
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

static void __exitNativeThread(void *ret) { _endthreadex((unsigned int)(ptrdiff_t)ret); }

static int __joinThread(thread_t *thread, timeout_t ns_timeout) {
	// Timeout -> milliseconds
	auto waitret = WaitForSingleObject(thread->handle, (ns_timeout == Infinite) : INFINITE ? timeout / 1'000'000);
	if (waitret == WAIT_TIMEOUT) {
		lock.lock();
		thread->attr.attr &= ThreadAttrFlags::JoinRequested;
		return ETIMEDOUT;
	} else if (waitret == WAIT_FAILED) {
		return platform::lastErrorToErrno(GetLastError());
	}
	return 0;
}

static void __pthread_cancel_callback(ULONG_PTR Parameter) { thread_t::exit((void *)Parameter); }

static int __cancelThread(thread_t *thread) {
	// Break user's context to call __pthread_cancel_callback on target thread
	// QueueUserAPC2 will unwind the stack for current user context
	// Then, pthread_exit should unwind the remaining stack with longjmp, if possible.

	QueueUserAPC2(__pthread_cancel_callback, thread->handle, (ULONG_PTR)__SPRT_PTHREAD_CANCELED,
			QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC);
}

static int __applyThreadPrio(thread_t *thread, int32_t dprio) {
	if (hasFlag(thread->attr.attr, ThreadAttrFlags::RRPrio)) {
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

/*
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

static int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, __sprt_cpu_set_t *set) {
	if (!thread) {
		return ESRCH;
	}
	if (!set) {
		return EINVAL;
	}

	SYSTEM_INFO sysInfo;
	GetNativeSystemInfo(&sysInfo);

	DWORD kernelSetSize =
			sysInfo.dwNumberOfProcessors / 8 + ((sysInfo.dwNumberOfProcessors % 8) ? 1 : 0);

	if (cpusetsize < kernelSetSize) {
		return EINVAL;
	}

	ULONG nCpus = 0;
	if (!GetThreadSelectedCpuSets(thread->handle, nullptr, 0, &nCpus)) {
		return EINVAL;
	}

	__SPRT_CPU_ZERO_S(cpusetsize, set);
	if (nCpus == 0) {
		return 0;
	}

	auto cpus = __sprt_typed_malloca(ULONG, nCpus);

	if (!GetThreadSelectedCpuSets(thread->handle, cpus, nCpus, &nCpus)) {
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

static int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize,
		const __sprt_cpu_set_t *set) {
	if (!thread) {
		return ESRCH;
	}
	if (!set) {
		return EINVAL;
	}

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

	if (SetThreadSelectedCpuSets(thread->handle, cpus, nCpus)) {
		__sprt_freea(cpus);
		return 0;
	}

	__sprt_freea(cpus);
	return EINVAL;
}

static int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
}
static int pthread_setname_np(pthread_t thread, const char *name) {
	if (!thread || !name) {
		return EINVAL;
	}

	int ret = 0;

	unicode::toUtf16([&](WideStringView wname) {
		auto hr = SetThreadDescription(thread->handle, (wchar_t *)wname.data());
		if (FAILED(hr)) {
			ret = EINVAL;
		}
	}, StringView(name));

	return ret;
}

static int pthread_getname_np(pthread_t thread, char *buf, size_t len) {
	if (!thread || !buf) {
		return EINVAL;
	}

	wchar_t *wbuf = nullptr;
	auto hr = GetThreadDescription(thread->handle, &wbuf);
	if (FAILED(hr)) {
		return EINVAL;
	}

	int ret = 0;
	unicode::toUtf8([&](StringView name) {
		if (name.size() <= len) {
			__sprt_memcpy(buf, name.data(), name.size());
		} else {
			__sprt_memcpy(buf, name.data(), len);
			ret = ERANGE;
		}
	}, WideStringView((char16_t *)wbuf));

	return ret;
}

static int pthread_getattr_default_np(pthread_attr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	unique_lock lock(s_handlePool.mutex);
	*attr = s_handlePool.defaultAttr;
	return 0;
}

static int pthread_setattr_default_np(const pthread_attr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	unique_lock lock(s_handlePool.mutex);
	s_handlePool.defaultAttr = *attr;

	return 0;
}

static int pthread_tryjoin_np(pthread_t thread, void **ret) {
	return __pthread_join(thread, ret, INFINITE, true);
}

static int pthread_timedjoin_np(pthread_t thread, void **ret, const __SPRT_TIMESPEC_NAME *tv) {
	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	DWORD millis = diffTv.tv_sec * 1'000 + diffTv.tv_nsec / 1'000'000;
	return __pthread_join(thread, ret, millis, false);
}*/

} // namespace sprt::_thread

#endif // SPRT_WINDOWS
