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

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct ThreadArg {
	int id;
	int *counter;
	pthread_mutex_t *mutex;
};

static void *threadFunc(void *arg) {
	auto *ta = static_cast<ThreadArg *>(arg);
	pthread_t self = pthread_self();

	pthread_mutex_lock(ta->mutex);
	int val = ++(*ta->counter);
	pthread_mutex_unlock(ta->mutex);

	printf("pthread: thread %d (tid=%p) incremented counter to %d\n", ta->id, (void *)self, val);
	return reinterpret_cast<void *>(static_cast<intptr_t>(val));
}

void performPthreadTest() {
	printf("--- pthread test ---\n");

	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, nullptr);

	int counter = 0;
	const int numThreads = 4;
	pthread_t threads[numThreads];
	ThreadArg args[numThreads];

	for (int i = 0; i < numThreads; ++i) {
		args[i] = {i, &counter, &mutex};
		int ret = pthread_create(&threads[i], nullptr, threadFunc, &args[i]);
		if (ret != 0) {
			printf("pthread: pthread_create failed for thread %d (ret=%d)\n", i, ret);
			continue;
		}
		printf("pthread: created thread %d (tid=%p)\n", i, (void *)threads[i]);
	}

	int sum = 0;
	for (int i = 0; i < numThreads; ++i) {
		void *retval = nullptr;
		int ret = pthread_join(threads[i], &retval);
		if (ret != 0) {
			printf("pthread: pthread_join failed for thread %d (ret=%d)\n", i, ret);
		} else {
			sum += static_cast<int>(reinterpret_cast<intptr_t>(retval));
		}
	}

	pthread_mutex_destroy(&mutex);

	printf("pthread: main thread (tid=%p)\n", (void *)pthread_self());
	printf("pthread: final counter=%d, sum of return values=%d\n", counter, sum);
	printf("--- pthread test done ---\n");
}

// --- pthread_cond_t test ---

struct CondTestArg {
	int id;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	int *ready;
	int *wake_count;
};

static void *condWaiterFunc(void *arg) {
	auto *a = static_cast<CondTestArg *>(arg);
	pthread_mutex_lock(a->mutex);
	printf("pthread_cond: waiter %d (tid=%p) waiting\n", a->id, (void *)pthread_self());
	while (*a->ready == 0) { pthread_cond_wait(a->cond, a->mutex); }
	(*a->wake_count)++;
	printf("pthread_cond: waiter %d (tid=%p) woken, wake_count=%d\n", a->id, (void *)pthread_self(),
			*a->wake_count);
	pthread_mutex_unlock(a->mutex);
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

void performPthreadCondTest() {
	printf("--- pthread_cond_t test ---\n");

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int ready = 0;
	int wake_count = 0;

	pthread_mutex_init(&mutex, nullptr);
	pthread_cond_init(&cond, nullptr);

	const int numWaiters = 3;
	pthread_t waiters[numWaiters];
	CondTestArg args[numWaiters];
	for (int i = 0; i < numWaiters; ++i) {
		args[i] = {
			i,
			&mutex,
			&cond,
			&ready,
			&wake_count,
		};
		int ret = pthread_create(&waiters[i], nullptr, condWaiterFunc, &args[i]);
		if (ret != 0) {
			printf("pthread_cond: pthread_create failed for waiter %d (ret=%d)\n", i, ret);
		}
	}

	// Give waiters time to block on cond
	struct timespec ts = {0, 100 * 1'000 * 1'000}; // 100ms
	nanosleep(&ts, nullptr);

	// Wake all with broadcast
	pthread_mutex_lock(&mutex);
	ready = 1;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);

	for (int i = 0; i < numWaiters; ++i) {
		void *retval = nullptr;
		pthread_join(waiters[i], &retval);
	}

	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);

	printf("pthread_cond: all %d waiters woken (wake_count=%d)\n", numWaiters, wake_count);
	printf("--- pthread_cond_t test done ---\n");
}

// --- pthread_rwlock_t test ---

struct RwlockReaderArg {
	int id;
	pthread_rwlock_t *rwlock;
	const int *value;
	int *read_count;
};

struct RwlockWriterArg {
	int id;
	pthread_rwlock_t *rwlock;
	int *value;
	int write_rounds;
};

static void *rwlockReaderFunc(void *arg) {
	auto *a = static_cast<RwlockReaderArg *>(arg);
	struct timespec ts = {0, 10 * 1'000 * 1'000}; // 10ms
	nanosleep(&ts, nullptr);
	for (int i = 0; i < 10; ++i) {
		pthread_rwlock_rdlock(a->rwlock);
		int v = *a->value;
		(*a->read_count)++;
		printf("pthread_rwlock: reader %d (tid=%p) read value=%d\n", a->id, (void *)pthread_self(),
				v);
		pthread_rwlock_unlock(a->rwlock);
		nanosleep(&ts, nullptr);
	}
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

static void *rwlockWriterFunc(void *arg) {
	auto *a = static_cast<RwlockWriterArg *>(arg);
	for (int i = 0; i < a->write_rounds; ++i) {
		pthread_rwlock_wrlock(a->rwlock);
		(*a->value)++;
		printf("pthread_rwlock: writer %d (tid=%p) wrote value=%d\n", a->id, (void *)pthread_self(),
				*a->value);
		pthread_rwlock_unlock(a->rwlock);
		struct timespec ts = {0, 10 * 1'000 * 1'000}; // 10ms
		nanosleep(&ts, nullptr);
	}
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

void performPthreadRwlockTest() {
	printf("--- pthread_rwlock_t test ---\n");

	pthread_rwlock_t rwlock;
	int shared_value = 0;
	int read_count = 0;

	pthread_rwlock_init(&rwlock, nullptr);

	const int numReaders = 3;
	const int numWriters = 1;
	const int write_rounds = 4;
	pthread_t readers[numReaders];
	pthread_t writers[numWriters];
	RwlockReaderArg rargs[numReaders];
	RwlockWriterArg wargs[numWriters];

	for (int i = 0; i < numReaders; ++i) {
		rargs[i] = {i, &rwlock, &shared_value, &read_count};
		int ret = pthread_create(&readers[i], nullptr, rwlockReaderFunc, &rargs[i]);
		if (ret != 0) {
			printf("pthread_rwlock: pthread_create failed for reader %d (ret=%d)\n", i, ret);
		}
	}
	for (int i = 0; i < numWriters; ++i) {
		wargs[i] = {i, &rwlock, &shared_value, write_rounds};
		int ret = pthread_create(&writers[i], nullptr, rwlockWriterFunc, &wargs[i]);
		if (ret != 0) {
			printf("pthread_rwlock: pthread_create failed for writer %d (ret=%d)\n", i, ret);
		}
	}

	for (int i = 0; i < numReaders; ++i) {
		void *retval = nullptr;
		pthread_join(readers[i], &retval);
	}
	for (int i = 0; i < numWriters; ++i) {
		void *retval = nullptr;
		pthread_join(writers[i], &retval);
	}

	pthread_rwlock_destroy(&rwlock);

	printf("pthread_rwlock: final value=%d, total reads=%d\n", shared_value, read_count);
	printf("--- pthread_rwlock_t test done ---\n");
}

// --- pthread_barrier_t test ---

struct BarrierArg {
	int id;
	pthread_barrier_t *barrier;
	int *phase_count;
};

static void *barrierWaiterFunc(void *arg) {
	auto *a = static_cast<BarrierArg *>(arg);
	printf("pthread_barrier: thread %d (tid=%p) waiting at phase 1\n", a->id, (void *)pthread_self());
	int ret = pthread_barrier_wait(a->barrier);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		(*a->phase_count)++;
		printf("pthread_barrier: thread %d was serial thread for phase 1\n", a->id);
	}
	printf("pthread_barrier: thread %d (tid=%p) released from phase 1\n", a->id,
			(void *)pthread_self());

	printf("pthread_barrier: thread %d (tid=%p) waiting at phase 2\n", a->id, (void *)pthread_self());
	ret = pthread_barrier_wait(a->barrier);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		(*a->phase_count)++;
		printf("pthread_barrier: thread %d was serial thread for phase 2\n", a->id);
	}
	printf("pthread_barrier: thread %d (tid=%p) released from phase 2, done\n", a->id,
			(void *)pthread_self());
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

void performPthreadBarrierTest() {
	printf("--- pthread_barrier_t test ---\n");

	const int numThreads = 4;
	pthread_barrier_t barrier;
	int phase_count = 0;

	pthread_barrier_init(&barrier, nullptr, numThreads);

	pthread_t threads[numThreads];
	BarrierArg args[numThreads];

	for (int i = 0; i < numThreads; ++i) {
		args[i] = {i, &barrier, &phase_count};
		int ret = pthread_create(&threads[i], nullptr, barrierWaiterFunc, &args[i]);
		if (ret != 0) {
			printf("pthread_barrier: pthread_create failed for thread %d (ret=%d)\n", i, ret);
		}
	}

	for (int i = 0; i < numThreads; ++i) {
		void *retval = nullptr;
		pthread_join(threads[i], &retval);
	}

	pthread_barrier_destroy(&barrier);

	printf("pthread_barrier: all %d threads passed 2 phases (serial_count=%d)\n", numThreads,
			phase_count);
	printf("--- pthread_barrier_t test done ---\n");
}

// --- pthread_spinlock_t test ---

struct SpinlockArg {
	int id;
	pthread_spinlock_t *spinlock;
	int *counter;
	int iterations;
};

static void *spinlockThreadFunc(void *arg) {
	auto *a = static_cast<SpinlockArg *>(arg);
	for (int i = 0; i < a->iterations; ++i) {
		pthread_spin_lock(a->spinlock);
		(*a->counter)++;
		pthread_spin_unlock(a->spinlock);
	}
	int trylock_acquired = 0;
	for (int i = 0; i < 100; ++i) {
		if (pthread_spin_trylock(a->spinlock) == 0) {
			(*a->counter)++;
			trylock_acquired++;
			pthread_spin_unlock(a->spinlock);
		}
	}
	printf("pthread_spinlock: thread %d (tid=%p) done, trylock_acquired=%d\n", a->id,
			(void *)pthread_self(), trylock_acquired);
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

void performPthreadSpinlockTest() {
	printf("--- pthread_spinlock_t test ---\n");

	pthread_spinlock_t spinlock;
	int counter = 0;

	pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);

	const int numThreads = 4;
	const int iterations = 100;
	pthread_t threads[numThreads];
	SpinlockArg args[numThreads];

	for (int i = 0; i < numThreads; ++i) {
		args[i] = {i, &spinlock, &counter, iterations};
		int ret = pthread_create(&threads[i], nullptr, spinlockThreadFunc, &args[i]);
		if (ret != 0) {
			printf("pthread_spinlock: pthread_create failed for thread %d (ret=%d)\n", i, ret);
		}
	}

	for (int i = 0; i < numThreads; ++i) {
		void *retval = nullptr;
		pthread_join(threads[i], &retval);
	}

	pthread_spin_destroy(&spinlock);

	printf("pthread_spinlock: final counter=%d (expected ~%d from lock + trylock)\n", counter,
			numThreads * (iterations + 100));
	printf("--- pthread_spinlock_t test done ---\n");
}
