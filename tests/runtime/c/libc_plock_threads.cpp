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

// Several threads hold address locks while other locks enter and leave the shared table.

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sprt/c/sys/__sprt_sprt.h>
#include "../tests.h"

namespace sprt {

namespace {

constexpr int PlockKeys = 6;
constexpr int PlockThreads = 8;
constexpr int PlockIterations = 20'000;

struct PlockShared {
	int keys[PlockKeys] = {};
	int inside[PlockKeys] = {};
	int overlaps = 0;
	int errors = 0;
};

struct PlockArg {
	int id;
	PlockShared *shared;
};

void *plockThreadFunc(void *arg) {
	auto a = static_cast<PlockArg *>(arg);
	auto s = a->shared;
	for (int i = 0; i < PlockIterations; ++i) {
		int k = (a->id + i * (a->id + 1)) % PlockKeys;
		__SPRT_ID(pid_t) owner = 0;
		bool tried = (i % 7) == 0;
		int st = tried ? __sprt_sprt_plock_trylock(&s->keys[k], 0, &owner)
					   : __sprt_sprt_plock_lock(&s->keys[k], 0, &owner);
		if (st != 0) {
			if (!tried) {
				if (__atomic_fetch_add(&s->errors, 1, __ATOMIC_RELAXED) < 3) {
					printf("plock: lock of %d failed, errno %d\n", k, errno);
				}
			}
			continue;
		}
		if (__atomic_fetch_add(&s->inside[k], 1, __ATOMIC_SEQ_CST) != 0) {
			__atomic_fetch_add(&s->overlaps, 1, __ATOMIC_RELAXED);
		}
		__atomic_fetch_sub(&s->inside[k], 1, __ATOMIC_SEQ_CST);
		if (__sprt_sprt_plock_unlock(&s->keys[k], 0, &owner) != 0) {
			if (__atomic_fetch_add(&s->errors, 1, __ATOMIC_RELAXED) < 3) {
				printf("plock: unlock of %d failed, errno %d\n", k, errno);
			}
		}
	}
	return nullptr;
}

} // namespace

void performPlockThreadsTest() {
	printf("--- sprt_plock under threads ---\n");

	PlockShared shared;
	pthread_t threads[PlockThreads];
	PlockArg args[PlockThreads];
	int started = 0;
	for (int i = 0; i < PlockThreads; ++i) {
		args[i] = {i, &shared};
		if (pthread_create(&threads[started], nullptr, plockThreadFunc, &args[i]) != 0) {
			break;
		}
		++started;
	}
	for (int i = 0; i < started; ++i) { pthread_join(threads[i], nullptr); }

	bool ok = started == PlockThreads && shared.overlaps == 0 && shared.errors == 0;
	printf("plock: %d threads, %d overlaps, %d errors: %s\n", started, shared.overlaps,
			shared.errors, ok ? "PASS" : sprt::test::failed("FAILED"));
}

} // namespace sprt
