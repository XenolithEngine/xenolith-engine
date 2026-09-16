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

#include <threads.h>
#include <stdio.h>
#include <stdint.h>

namespace sprt::test {

// C11 <threads.h>. On both targets these resolve to SPRT's own C11 layer
// (runtime/core/pthread/c11threads.cc) over the pthread implementation, so the
// behaviour is identical and every printed value is deterministic: thread
// outcomes are aggregated (a mutex-protected counter, a sum of join results,
// a once-counter) rather than printed per-thread, and the condvar / trylock
// cases are arranged so the result never depends on scheduling.

namespace {

constexpr int kThreads = 4;
constexpr int kIncrements = 1000;

mtx_t gCounterMtx;
int gCounter;

int counterWorker(void *arg) {
	for (int i = 0; i < kIncrements; ++i) {
		mtx_lock(&gCounterMtx);
		++gCounter;
		mtx_unlock(&gCounterMtx);
	}
	return static_cast<int>(reinterpret_cast<intptr_t>(arg)); // echo the argument back
}

mtx_t gBusyMtx;
int gBusyResult;

int trylockWorker(void *) {
	// main holds gBusyMtx for the whole lifetime of this thread, so the result is
	// deterministically thrd_busy.
	gBusyResult = mtx_trylock(&gBusyMtx);
	return 0;
}

mtx_t gCndMtx;
cnd_t gCnd;
int gReady;
int gCndResult;

int cndWorker(void *) {
	mtx_lock(&gCndMtx);
	while (!gReady) {
		cnd_wait(&gCnd, &gCndMtx);
	}
	gCndResult = 42;
	mtx_unlock(&gCndMtx);
	return 0;
}

once_flag gOnce = ONCE_FLAG_INIT;
int gOnceCount;

void onceFn(void) { ++gOnceCount; }

int onceWorker(void *) {
	call_once(&gOnce, onceFn);
	return 0;
}

tss_t gTssKey;
int gTssObserved;

int tssWorker(void *) {
	// each thread sees its own slot: set then read back the same value.
	tss_set(gTssKey, reinterpret_cast<void *>(static_cast<intptr_t>(0xABCD)));
	gTssObserved = static_cast<int>(reinterpret_cast<intptr_t>(tss_get(gTssKey)));
	return 0;
}

} // namespace

void performThreadsTest() {
	// --- thrd_create / thrd_join: mutex-protected shared counter + return values ---
	mtx_init(&gCounterMtx, mtx_plain);
	gCounter = 0;
	thrd_t threads[kThreads];
	int created = 0;
	for (int i = 0; i < kThreads; ++i) {
		if (thrd_create(&threads[i], counterWorker, reinterpret_cast<void *>(static_cast<intptr_t>(i + 1)))
				== thrd_success) {
			++created;
		}
	}
	int sumResults = 0;
	for (int i = 0; i < kThreads; ++i) {
		int res = 0;
		thrd_join(threads[i], &res);
		sumResults += res;
	}
	mtx_destroy(&gCounterMtx);
	printf("created=%d/%d\n", created, kThreads);
	printf("counter=%d expect=%d\n", gCounter, kThreads * kIncrements);
	printf("sum_results=%d expect=%d\n", sumResults, kThreads * (kThreads + 1) / 2);

	// --- thrd_current / thrd_equal ---
	thrd_t self = thrd_current();
	printf("equal_self=%d\n", thrd_equal(self, self) != 0 ? 1 : 0);

	// --- mtx_trylock returns thrd_busy while another thread holds the lock ---
	mtx_init(&gBusyMtx, mtx_plain);
	mtx_lock(&gBusyMtx);
	thrd_t bt;
	thrd_create(&bt, trylockWorker, nullptr);
	thrd_join(bt, nullptr);
	mtx_unlock(&gBusyMtx);
	mtx_destroy(&gBusyMtx);
	printf("trylock_busy=%d\n", gBusyResult == thrd_busy ? 1 : 0);

	// --- recursive mutex re-lock by the same thread ---
	mtx_t rec;
	int recInit = mtx_init(&rec, mtx_recursive);
	mtx_lock(&rec);
	int reLock = mtx_lock(&rec);
	mtx_unlock(&rec);
	mtx_unlock(&rec);
	mtx_destroy(&rec);
	printf("recursive_init=%d relock=%d\n", recInit == thrd_success ? 1 : 0,
			reLock == thrd_success ? 1 : 0);

	// --- condition variable handshake ---
	mtx_init(&gCndMtx, mtx_plain);
	cnd_init(&gCnd);
	gReady = 0;
	gCndResult = 0;
	thrd_t cw;
	thrd_create(&cw, cndWorker, nullptr);
	mtx_lock(&gCndMtx);
	gReady = 1;
	cnd_signal(&gCnd);
	mtx_unlock(&gCndMtx);
	thrd_join(cw, nullptr);
	cnd_destroy(&gCnd);
	mtx_destroy(&gCndMtx);
	printf("cnd_result=%d\n", gCndResult);

	// --- thread-specific storage ---
	tss_create(&gTssKey, nullptr);
	gTssObserved = 0;
	thrd_t tw;
	thrd_create(&tw, tssWorker, nullptr);
	thrd_join(tw, nullptr);
	printf("tss_observed=%x main_slot=%d\n", gTssObserved,
			tss_get(gTssKey) == nullptr ? 1 : 0); // main never set its slot
	tss_delete(gTssKey);

	// --- call_once across all threads runs exactly once ---
	gOnceCount = 0;
	thrd_t ot[kThreads];
	for (int i = 0; i < kThreads; ++i) {
		thrd_create(&ot[i], onceWorker, nullptr);
	}
	for (int i = 0; i < kThreads; ++i) {
		thrd_join(ot[i], nullptr);
	}
	printf("once_count=%d\n", gOnceCount);
}

} // namespace sprt::test
