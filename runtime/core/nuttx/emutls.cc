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

// Per-tid emutls for NuttX.
//
// compiler-rt's __emutls_get_address stores the slot table in a NuttX pthread
// key. NSH tasks are not pthreads; pthread_getspecific has returned non-mapped
// pointers (0x47ffffffe, 0xe9). A process-global table is also wrong: AppThread
// is a real pthread and would share AllocStack with the NSH task, which trips
// "Unbalansed pool::push". Index assignment is still global (object identity);
// the slot array is keyed by gettid().

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

typedef unsigned int gcc_word __attribute__((mode(word)));

struct __emutls_control {
	gcc_word size;
	gcc_word align;
	union {
		uintptr_t index;
		void *address;
	} object;
	void *value;
};

struct EmutlsTable {
	pid_t tid;
	void **slots;
	uintptr_t nslots;
};

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t s_next = 0;
static EmutlsTable s_tables[8];
static unsigned s_ntables = 0;

static void *allocateObject(__emutls_control *control) {
	size_t size = control->size;
	size_t align = control->align;
	if (align < sizeof(void *)) {
		align = sizeof(void *);
	}

	const size_t extra = align - 1 + sizeof(void *);
	char *object = static_cast<char *>(malloc(extra + size));
	if (!object) {
		abort();
	}
	void *base = reinterpret_cast<void *>(
			(reinterpret_cast<uintptr_t>(object + extra)) & ~(uintptr_t(align) - 1));
	reinterpret_cast<void **>(base)[-1] = object;

	if (control->value) {
		memcpy(base, control->value, size);
	} else {
		memset(base, 0, size);
	}
	return base;
}

static EmutlsTable *tableForSelf() {
	// Kernel tid, not __sprt_gettid(): that wrapper used to consult pthread
	// TLS and recurse into this function.
	const pid_t tid = ::gettid();
	for (unsigned i = 0; i < s_ntables; ++i) {
		if (s_tables[i].tid == tid) {
			return &s_tables[i];
		}
	}
	if (s_ntables >= (sizeof(s_tables) / sizeof(s_tables[0]))) {
		abort();
	}
	EmutlsTable *t = &s_tables[s_ntables++];
	t->tid = tid;
	t->slots = nullptr;
	t->nslots = 0;
	return t;
}

extern "C" __attribute__((visibility("default"))) void *__emutls_get_address(
		__emutls_control *control) {
	pthread_mutex_lock(&s_mutex);
	uintptr_t index = control->object.index;
	if (!index) {
		index = ++s_next;
		control->object.index = index;
	}
	EmutlsTable *table = tableForSelf();
	if (index > table->nslots) {
		uintptr_t n = (index + 15u) & ~uintptr_t(15);
		void **grown = static_cast<void **>(realloc(table->slots, n * sizeof(void *)));
		if (!grown) {
			abort();
		}
		memset(grown + table->nslots, 0, (n - table->nslots) * sizeof(void *));
		table->slots = grown;
		table->nslots = n;
	}
	if (!table->slots[index - 1]) {
		table->slots[index - 1] = allocateObject(control);
	}
	void *ret = table->slots[index - 1];
	pthread_mutex_unlock(&s_mutex);
	return ret;
}
