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

#include "pthread_native.h"

namespace sprt::_thread {

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_init)(__SPRT_ID(pthread_spinlock_t) * spin, int pshared) {
	(void)pshared; // Ignored, no cross-process support
	*spin = 0;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_destroy)(__SPRT_ID(pthread_spinlock_t) * spin) {
	(void)spin;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_trylock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	// InterlockedCompareExchange returns previous value.
	uint32_t expected = 0;
	if (_atomic::compareSwap(spin, &expected, uint32_t(1))) {
		return 0;
	}
	return EBUSY;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_lock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	// Fast path
	if (__sprt_pthread_spin_trylock(spin) == 0) {
		return 0;
	}

	// Contended: spin with backoff
	int spins = 0;
	for (;;) {
		// Short spin
		for (int i = 0; i < 64; ++i) {
			__sprt_sched_yield();

			if (__sprt_pthread_spin_trylock(spin) == 0) {
				return 0;
			}
		}

		// After some spinning, yield to scheduler
		if (++spins < 20) {
			__sprt_usleep(0); // switch to another ready thread
		} else {
			__sprt_usleep(1'000); // back off harder under heavy contention
		}
	}
}

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_unlock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	// Simple store is enough on x86; use barrier if you need strict ordering
	_atomic::storeSeq(spin, uint32_t(0));
	return 0;
}

} // namespace sprt::_thread
