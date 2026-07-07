#include <stdio.h>
#include <pthread.h>
static volatile int g = 0;
static void *worker(void *a) { g = 42; return (void *)(long)7; }
int main(int, char **) {
	printf("main: creating thread\n");
	pthread_t t;
	int rc = pthread_create(&t, nullptr, worker, nullptr);
	printf("create rc=%d\n", rc);
	if (rc != 0) return 1;
	void *ret = nullptr;
	int jrc = pthread_join(t, &ret);
	printf("join rc=%d g=%d ret=%ld\n", jrc, g, (long)ret);
	return 0;
}
