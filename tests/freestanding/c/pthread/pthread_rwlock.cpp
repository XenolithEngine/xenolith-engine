#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace sprt {

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
	for (int i = 0; i < 10; ++i) {
		pthread_rwlock_rdlock(a->rwlock);
		int v = *a->value;
		(*a->read_count)++;
		printf("pthread_rwlock: reader %d (tid=%p) read value=%d\n", a->id, (void *)pthread_self(),
				v);
		pthread_rwlock_unlock(a->rwlock);
		struct timespec ts = {0, 10 * 1'000 * 1'000}; // 10ms
		nanosleep(&ts, nullptr);
	}
	return reinterpret_cast<void *>(static_cast<intptr_t>(a->id));
}

static void *rwlockWriterFunc(void *arg) {
	auto *a = static_cast<RwlockWriterArg *>(arg);
	struct timespec ts = {0, 10 * 1'000 * 1'000}; // 10ms
	nanosleep(&ts, nullptr);
	for (int i = 0; i < a->write_rounds; ++i) {
		pthread_rwlock_wrlock(a->rwlock);
		(*a->value)++;
		printf("pthread_rwlock: writer %d (tid=%p) wrote value=%d\n", a->id, (void *)pthread_self(),
				*a->value);
		pthread_rwlock_unlock(a->rwlock);
		ts = {0, 10 * 1'000 * 1'000}; // 10ms
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

} // namespace sprt
