#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

namespace sprt {

struct ThreadArg {
	int id;
};

static void *threadFunc(void *arg) {
	auto *ta = static_cast<ThreadArg *>(arg);
	pthread_t self = pthread_self();

	//usleep(100);

	printf("pthread: thread %d (tid=%p) started\n", ta->id, (void *)self);
	return reinterpret_cast<void *>(static_cast<intptr_t>(ta->id * 10));
}

void performPthreadCreateTest() {
	printf("--- pthread create/join test ---\n");

	const int numThreads = 4;
	pthread_t threads[numThreads];
	ThreadArg args[numThreads];

	// Create threads
	for (int i = 0; i < numThreads; ++i) {
		args[i].id = i;
		int ret = pthread_create(&threads[i], nullptr, threadFunc, &args[i]);
		if (ret != 0) {
			printf("pthread: pthread_create failed for thread %d (ret=%d)\n", i, ret);
			continue;
		}
		printf("pthread: created thread %d (tid=%p)\n", i, (void *)threads[i]);
	}

	//usleep(1'000);

	// Join threads and collect return values
	int sum = 0;
	for (int i = 0; i < numThreads; ++i) {
		void *retval = nullptr;
		int ret = pthread_join(threads[i], &retval);
		if (ret != 0) {
			printf("pthread: pthread_join failed for thread %d (ret=%d)\n", i, ret);
		} else {
			int value = static_cast<int>(reinterpret_cast<intptr_t>(retval));
			sum += value;
			printf("pthread: thread %d returned %d\n", i, value);
		}
	}

	printf("pthread: main thread (tid=%p)\n", (void *)pthread_self());
	printf("pthread: sum of return values=%d\n", sum);
	printf("--- pthread create/join test done ---\n");
}

} // namespace sprt
