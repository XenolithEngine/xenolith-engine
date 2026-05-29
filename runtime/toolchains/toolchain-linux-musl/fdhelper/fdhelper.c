#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>

#define STR_AND_SIZE(s) s, sizeof(s) - 1

//
// Here we need to initialize threads, thread-local storage, then send dso functions to host
//

void *funcs[4];

static unsigned long hexstrtoul(const char *s)
{
	unsigned long res = 0;
	char c;
	while ((c = *s++)) {
		if (c >= '0' && c <= '9') {
		c -= '0';
		} else {
			c &= ~0x20;
			c -= 'A' - 0xa;
		}
		res = res << 4 | c;
	}
	return res;
}

pthread_key_t thread_data_key;

void destroy_thread_data(void *data) { }

void *thread_function(void *arg) {
	pthread_setspecific(thread_data_key, "test");
	pthread_exit(NULL); // Terminate the thread
}

int main(int argc, char *argv[]) {
	if (pthread_key_create(&thread_data_key, destroy_thread_data) != 0) {
		write(1, STR_AND_SIZE("pthread_key_create failed\n"));
		return 1;
	}

	pthread_t thread_id;

	if (pthread_create(&thread_id, NULL, thread_function, NULL) != 0) {
		write(1, STR_AND_SIZE("pthread_create failed\n"));
		return 1;
 	}

	pthread_join(thread_id, NULL);

	pthread_key_delete(thread_data_key);

	if (argc == 2) {
		void (*p)(void*) = (void*)hexstrtoul(argv[1]);
		funcs[0] = dlopen;
		funcs[1] = dlsym;
		funcs[2] = dlclose;
		funcs[3] = dlerror;
		p(funcs);
	} else {
		write(1, STR_AND_SIZE("Success\n"));
	}
}
