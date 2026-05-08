#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace sprt {

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

} // namespace sprt
