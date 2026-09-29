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

// thread_local under threads that come and go. Written for Embox EL1, where a
// thread_local is emulated (-femulated-tls) and the table behind it lives in
// core/embox/emutls.cc; it runs everywhere.
//
//   fresh      a thread that set its thread_locals and ended leaves nothing to
//              the next one: Embox hands `struct thread *` back out of a pool,
//              and a table keyed by the pointer alone gave the next thread the
//              dead one's values
//   crowd      48 threads with thread_locals alive at once, each seeing only
//              its own -- more than the 32 rows the table used to have
//   churn      300 short threads one after another: rows come back
//   main       the command's own thread starts fresh too; run the test twice
//              in one boot for this to mean anything (a task's main thread is
//              not an sprt thread and ends without giving its row back)

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include "../tests.h"

namespace sprt {

namespace {

constexpr int CrowdThreads = 48;
constexpr int ChurnThreads = 300;

thread_local int tl_value;
thread_local int tl_seenAtStart = -1;
thread_local char tl_block[256];

thread_local int tl_mainRuns;

void *setAndEnd(void *) {
	tl_value = 42;
	tl_block[0] = 1;
	return nullptr;
}

void *freshCheck(void *arg) {
	auto fresh = static_cast<int *>(arg);
	*fresh = (tl_value == 0 && tl_block[0] == 0 && tl_seenAtStart == -1) ? 1 : 0;
	tl_value = 42;
	tl_block[0] = 1;
	tl_seenAtStart = 7;
	return nullptr;
}

pthread_mutex_t s_crowdLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_crowdCond = PTHREAD_COND_INITIALIZER;
int s_crowdAlive = 0;
bool s_crowdRelease = false;

void *crowdMember(void *arg) {
	const int id = static_cast<int>(reinterpret_cast<intptr_t>(arg));
	auto ok = true;
	if (tl_value != 0) {
		ok = false;
	}
	tl_value = 1000 + id;
	tl_block[id % sizeof(tl_block)] = static_cast<char>(id);

	pthread_mutex_lock(&s_crowdLock);
	++s_crowdAlive;
	pthread_cond_broadcast(&s_crowdCond);
	while (!s_crowdRelease) {
		pthread_cond_wait(&s_crowdCond, &s_crowdLock);
	}
	pthread_mutex_unlock(&s_crowdLock);

	// Every other thread has written its own values in the meantime.
	if (tl_value != 1000 + id || tl_block[id % sizeof(tl_block)] != static_cast<char>(id)) {
		ok = false;
	}
	return reinterpret_cast<void *>(intptr_t(ok ? 1 : 0));
}

} // namespace

void performEmutlsThreadsTest() {
	int failures = 0;
	auto check = [&](bool cond, const char *msg) {
		printf("  %s: %s\n", cond ? "PASS" : sprt::test::failed("FAIL"), msg);
		if (!cond) {
			++failures;
		}
	};

	// main
	const int mainRuns = tl_mainRuns++;
	printf("emutls: this command's thread %p has run the test %d time(s) before\n",
			reinterpret_cast<void *>(pthread_self()), mainRuns);
	check(mainRuns == 0, "the command's own thread starts with fresh thread_locals");

	// fresh
	pthread_t t;
	bool created = pthread_create(&t, nullptr, setAndEnd, nullptr) == 0;
	if (created) {
		pthread_join(t, nullptr);
	}
	int stale = 0;
	for (int i = 0; i < 100 && created; ++i) {
		int fresh = 0;
		if (pthread_create(&t, nullptr, freshCheck, &fresh) != 0) {
			created = false;
			break;
		}
		pthread_join(t, nullptr);
		stale += fresh ? 0 : 1;
	}
	printf("emutls: 100 threads after one that set its thread_locals, %d saw stale values\n",
			stale);
	check(created, "the threads started");
	check(stale == 0, "a new thread never sees an ended thread's thread_locals");

	// crowd
	pthread_t crowd[CrowdThreads];
	int started = 0;
	s_crowdAlive = 0;
	s_crowdRelease = false;
	for (int i = 0; i < CrowdThreads; ++i) {
		if (pthread_create(&crowd[i], nullptr, crowdMember, reinterpret_cast<void *>(intptr_t(i)))
				!= 0) {
			break;
		}
		++started;
	}
	pthread_mutex_lock(&s_crowdLock);
	while (s_crowdAlive < started) {
		pthread_cond_wait(&s_crowdCond, &s_crowdLock);
	}
	s_crowdRelease = true;
	pthread_cond_broadcast(&s_crowdCond);
	pthread_mutex_unlock(&s_crowdLock);
	int own = 0;
	for (int i = 0; i < started; ++i) {
		void *ret = nullptr;
		pthread_join(crowd[i], &ret);
		own += ret ? 1 : 0;
	}
	printf("emutls: %d of %d threads alive at once kept their own thread_locals\n", own,
			CrowdThreads);
	check(started == CrowdThreads, "48 threads with thread_locals alive at once");
	check(own == started, "each of them saw only its own values");

	// churn
	int churned = 0;
	stale = 0;
	for (int i = 0; i < ChurnThreads; ++i) {
		int fresh = 0;
		if (pthread_create(&t, nullptr, freshCheck, &fresh) != 0) {
			break;
		}
		pthread_join(t, nullptr);
		++churned;
		stale += fresh ? 0 : 1;
	}
	check(churned == ChurnThreads && stale == 0,
			"300 short threads one after another, each starting fresh");

	printf("emutls_threads: %s (%d failure(s))\n", failures ? "FAILED" : "PASS", failures);
}

} // namespace sprt
