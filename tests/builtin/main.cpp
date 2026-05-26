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

extern "C" {
struct FILE;

void *memcpy(void *, const void *, size_t);
int write(int fd, void *, size_t);
size_t strlen(const char *);

int __sprt_pthread_create(void **thread, void *, void *(*threadFunc)(void *), void *);
int __sprt_pthread_join(void *thread, void **result);
int __sprt_usleep(size_t);

int printf(const char *__restrict fmt, ...);

char *tmpnam(char *);
int system(const char *);

FILE *popen(const char *, const char *);
char *fgets(char *, size_t, FILE *);
int pclose(FILE *);
}

struct StaticInitTest {
	StaticInitTest() {
		auto str = "Static costructor\n";
		write(1, (void *)str, strlen(str));
	};

	~StaticInitTest() {
		auto str = "Static destructor\n";
		write(1, (void *)str, strlen(str));
	}
};

struct ThreadLocalInitTest {
	ThreadLocalInitTest() {
		auto str = "Thread local costructor\n";
		write(1, (void *)str, strlen(str));
	};

	~ThreadLocalInitTest() {
		auto str = "Thread local destructor\n";
		write(1, (void *)str, strlen(str));
	}

	void call() {
		auto str = "Thread local call\n";
		write(1, (void *)str, strlen(str));
	}
};

static StaticInitTest s_test;
thread_local ThreadLocalInitTest tl_test;

int showFrameAddress() {
	/*printf("fa 0 %llx \n", __sprt_cfa_lookup(0));
	printf("fa 1 %llx \n", __sprt_cfa_lookup(1));
	printf("fa 2 %llx \n", __sprt_cfa_lookup(2));*/
	return 0;
}

static void *threadFunc(void *arg) {
	write(1, (void *)"Thread started!\n", strlen("Thread started!\n"));

	__sprt_usleep(100);

	//showFrameAddress();

	write(1, (void *)"Thread ended!\n", strlen("Thread ended!\n"));
	return nullptr;
}

int main(int argc, const char *argv[]) {
	auto str =
			"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	char buf[256];

	memcpy(buf, str, strlen(str) + 1);

	write(1, (void *)buf, strlen(str));

	printf("tmpnam: %s\n", tmpnam(nullptr));

	printf("%d\n", system("echo %COMSPEC%"));

	auto fp = popen("echo TEST %COMSPEC%", "r");
	char buffer[1'024];
	while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
		printf("Output: %s\n", buffer); //
	}
	pclose(fp);

	void *thread = nullptr;

	__sprt_pthread_create(&thread, nullptr, threadFunc, nullptr);
	__sprt_pthread_join(thread, nullptr);

	//showFrameAddress();

	printf("%s \n", "Complete!");

	/*printf(" 0 %llx \n", __sprt_cfa_lookup(0));
	printf(" 1 %llx \n", __sprt_cfa_lookup(1));
	printf(" 2 %llx \n", __sprt_cfa_lookup(2));*/

	return 0;
}
