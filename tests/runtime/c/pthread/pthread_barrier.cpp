#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace sprt {

// --- pthread_barrier_t test ---

struct BarrierArg {
	int id;
	pthread_barrier_t *barrier;
	int *phase_count;
};

static void *barrierWaiterFunc(void *arg) {
	auto *a = static_cast<BarrierArg *>(arg);
	printf("pthread_barrier: thread %d (tid=%p) waiting at phase 1\n", a->id,
			(void *)pthread_self());
	int ret = pthread_barrier_wait(a->barrier);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		(*a->phase_count)++;
		printf("pthread_barrier: thread %d was serial thread for phase 1\n", a->id);
	}
	printf("pthread_barrier: thread %d (tid=%p) released from phase 1\n", a->id,
			(void *)pthread_self());

	printf("pthread_barrier: thread %d (tid=%p) waiting at phase 2\n", a->id,
			(void *)pthread_self());
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

} // namespace sprt
