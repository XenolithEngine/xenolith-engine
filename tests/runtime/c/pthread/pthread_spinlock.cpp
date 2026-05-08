#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace sprt {

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

} // namespace sprt
