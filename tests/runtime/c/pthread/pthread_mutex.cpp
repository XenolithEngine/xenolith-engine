#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace sprt {

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

void performPthreadMutexTest() {
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

} // namespace sprt
