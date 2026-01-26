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

int pthread_barrier_t::wait() {
	uint32_t expected = nthreads & ValueMask;
	uint32_t gen = atomicLoadSeq(&generations);
	auto ticket = atomicFetchAdd(&nIn, uint32_t(1));

	if ((ticket % (expected)) == expected - 1) {
		// golden ticket - we can set PassFlag and signal others
		// but only if PassFlag is NOT set

		// decrement In value to exclude overflow
		atomicFetchSub(&nIn, expected);

		// we expect that flag is set for our WaitOnAddress
		expected |= PassFlag;

		auto v = atomicFetchOr(&nthreads, PassFlag);
		while ((v & PassFlag) != 0) {
			// flag was set - waiting
			// this can happen, if some other thread with golden ticket set flag before us
			// in this case, we wait until the last thread in group will wake us
			WaitOnAddress(&nthreads, &expected, sizeof(uint32_t), INFINITE);

			// try to set flag again
			v = atomicFetchOr(&nthreads, PassFlag);
		}

		// clear Out counter
		atomicStoreSeq(&nOut, uint32_t(0));
		atomicFetchAdd(&generations, uint32_t(1));

		WakeByAddressAll(&nthreads);
		return __SPRT_PTHREAD_BARRIER_SERIAL_THREAD;
	} else {
		do {
			// wait until PassFlag is set
			do {
				WaitOnAddress(&nthreads, &expected, sizeof(uint32_t), INFINITE);
			} while (atomicLoadSeq(&nthreads) == expected);

			uint32_t newGen;
			// spinlock for generation increment
			// if generation was incremented - Out was cleared
			while ((newGen = atomicLoadSeq(&generations)) == gen) { YieldProcessor(); }
			gen = newGen;

			// pass flag was set
			// last exiting thread (expected - 2 thread) should remove flag; others ( > expected - 2) should wait again

			auto exitTicket = atomicFetchAdd(&nOut, uint32_t(1));
			if (exitTicket < expected - 2) {
				return 0;
			} else if (exitTicket == expected - 2) {
				// black ticket - remove PassFlag
				atomicFetchAnd(&nthreads, ValueMask);

				// we can wake a quick ones from this loop, so, use WakeByAddressAll instead of WakeByAddressSingle
				WakeByAddressAll(&nthreads);

				return 0;
			} else {
				// we fail to exit
			}
		} while (1);
	}

	__builtin_unreachable();
	return 0;

	// acquire our ticket and generation atomically
	/*genmutex.lock();
	auto ticket = narrivals++;
	auto gen = generations;

	if (ticket + 1 == nthreads) {
		narrivals -= nthreads;
		++generations;
	}
	genmutex.unlock();

	// Unlock whole generation or wait until it

	if (ticket + 1 == nthreads) {
		// Last thread: signal completion
		atomicFetchAdd(&value, uint32_t(1));
		WakeByAddressAll(&value);
		return __SPRT_PTHREAD_BARRIER_SERIAL_THREAD;
	} else {
		// Wait for barrier pass
		do {
			WaitOnAddress(&value, &gen, sizeof(uint32_t), INFINITE);
		} while (atomicLoadSeq(&value) == gen);
		return 0; // PTHREAD_BARRIER_SERIAL_THREAD (non-last)
	}*/
}

static int pthread_barrierattr_init(pthread_barrierattr_t *attr) {
	new (attr, nothrow) pthread_barrierattr_t();
	return 0;
}

static int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	attr->~pthread_barrierattr_t();
	return 0;
}

static int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	return 0;
}

static int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_barrier_init(pthread_barrier_t *__SPRT_RESTRICT bar,
		const pthread_barrierattr_t *__SPRT_RESTRICT attr, unsigned v) {
	if (!attr || v == 0) {
		return EINVAL;
	}

	new (bar, nothrow) pthread_barrier_t{v};
	return 0;
}
static int pthread_barrier_destroy(pthread_barrier_t *bar) {
	if (!bar) {
		return EINVAL;
	}

	if (atomicLoadSeq(&bar->nOut) != 0 || atomicLoadSeq(&bar->nIn) != 0
			|| (atomicLoadSeq(&bar->nthreads) & pthread_barrier_t::PassFlag)) {
		return EBUSY;
	}

	bar->~pthread_barrier_t();
	return 0;
}

static int pthread_barrier_wait(pthread_barrier_t *bar) {
	if (!bar) {
		return EINVAL;
	}

	return bar->wait();
}


} // namespace sprt
