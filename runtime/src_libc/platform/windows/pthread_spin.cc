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

// @AI-generated
static inline int pthread_spin_init(pthread_spinlock_t *s, int pshared) {
	(void)pshared; // Ignored, no cross-process support
	s = 0;
	return 0;
}

// @AI-generated
static inline int pthread_spin_destroy(pthread_spinlock_t *s) {
	(void)s;
	return 0;
}

// Try to acquire: return 0 on success, EBUSY if already locked
static inline int pthread_spin_trylock(pthread_spinlock_t *s) {
	// InterlockedCompareExchange returns previous value.
	uint32_t value = 0;
	if (atomicCompareSwap(s, &value, uint32_t(1))) {
		return 0;
	}
	return EBUSY;
}

// @AI-generated
// Blocking lock with polite backoff
static inline int pthread_spin_lock(pthread_spinlock_t *s) {
	// Fast path
	if (pthread_spin_trylock(s) == 0) {
		return 0;
	}

	// Contended: spin with backoff
	int spins = 0;
	for (;;) {
		// Short spin
		for (int i = 0; i < 64; ++i) {
			YieldProcessor(); // pause on x86/x64

			if (pthread_spin_trylock(s) == 0) {
				return 0;
			}
		}

		// After some spinning, yield to scheduler
		if (++spins < 20) {
			Sleep(0); // switch to another ready thread
		} else {
			Sleep(1); // back off harder under heavy contention
		}
	}
}

// Unlock
static inline int pthread_spin_unlock(pthread_spinlock_t *s) {
	// Simple store is enough on x86; use barrier if you need strict ordering
	atomicStoreSeq(s, uint32_t(0));
	return 0;
}

} // namespace sprt
