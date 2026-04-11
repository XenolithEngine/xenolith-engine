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

#include "pthread_win.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static CondAttrFlags __getCondFlagsFromClockId(__sprt_clockid_t clock) {
	switch (clock) {
	case __SPRT_CLOCK_REALTIME: return CondAttrFlags::None; break;
	case __SPRT_CLOCK_REALTIME_COARSE: return CondAttrFlags::ClockRealtimeCoarse; break;
	case __SPRT_CLOCK_MONOTONIC: return CondAttrFlags::ClockMonotomic; break;
	case __SPRT_CLOCK_MONOTONIC_COARSE: return CondAttrFlags::ClockMonotomicCoarse; break;
	case __SPRT_CLOCK_PROCESS_CPUTIME_ID: return CondAttrFlags::ClockProcessCpu; break;
	case __SPRT_CLOCK_THREAD_CPUTIME_ID: return CondAttrFlags::ClockThreadCpu; break;
	default: break;
	}
	return CondAttrFlags::None;
}

static __sprt_clockid_t __getClockIdFromCondFlags(CondAttrFlags flags) {
	auto f = flags & CondAttrFlags::ClockMask;
	switch (f) {
	case CondAttrFlags::ClockRealtimeCoarse: return __SPRT_CLOCK_REALTIME_COARSE; break;
	case CondAttrFlags::ClockMonotomic: return __SPRT_CLOCK_MONOTONIC; break;
	case CondAttrFlags::ClockMonotomicCoarse: return __SPRT_CLOCK_MONOTONIC_COARSE; break;
	case CondAttrFlags::ClockProcessCpu: return __SPRT_CLOCK_PROCESS_CPUTIME_ID; break;
	case CondAttrFlags::ClockThreadCpu: return __SPRT_CLOCK_THREAD_CPUTIME_ID; break;
	default: break;
	}
	return __SPRT_CLOCK_REALTIME;
}

static int pthread_cond_init(pthread_cond_t *__SPRT_RESTRICT cond,
		const pthread_condattr_t *__SPRT_RESTRICT attr) {
	if (!cond) {
		return EINVAL;
	}

	new (cond, nothrow) pthread_cond_t();
	if (attr) {
		cond->flags = attr->flags;
	}
	return 0;
}

static int pthread_cond_destroy(pthread_cond_t *cond) {
	if (!cond) {
		return EINVAL;
	}

	cond->~pthread_cond_t();
	return 0;
}

static int pthread_cond_wait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex) {
	if (!cond || !mutex || !mutex->is_owned_by_this_thread()) {
		return EINVAL;
	}

	return cond->wait(mutex, INFINITE);
}

static int pthread_cond_timedwait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex, const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!cond || !mutex || !mutex->is_owned_by_this_thread() || !tv || tv->tv_nsec < 0
			|| tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__getClockIdFromCondFlags(cond->flags), &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	DWORD millis = diffTv.tv_sec * 1'000 + diffTv.tv_nsec / 1'000'000;
	return cond->wait(mutex, millis ? millis : 1);
}

static int pthread_cond_broadcast(pthread_cond_t *cond) {
	if (!cond) {
		return EINVAL;
	}

	return cond->broadcast();
}
static int pthread_cond_signal(pthread_cond_t *cond) {
	if (!cond) {
		return EINVAL;
	}

	return cond->signal();
}

} // namespace sprt
