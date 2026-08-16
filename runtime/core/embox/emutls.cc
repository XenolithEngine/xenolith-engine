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

// Per-tid emutls for Embox.
//
// compiler-rt's __emutls_get_address stores the slot table in a Embox pthread
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
	// The pthread_self() POINTER, not a folded pid_t: Embox's pthread_t is
	// `struct thread *`, and any narrowing (the (p >> 4) ^ (p >> 32) that
	// __sprt_gettid() has to do to produce a pid_t) can alias two live threads
	// onto one table. Here the full pointer is available, so use it.
	uintptr_t self;
	bool used;
	void **slots;
	uintptr_t nslots;
};

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t s_next = 0;
static EmutlsTable s_tables[32];

// Embox hands `struct thread *` back out of a fixed pool (thread_pool_size,
// 16 by default), so a thread that exits leaves its address free for the next
// one. Without a release step the new thread finds the dead thread's table and
// inherits its thread_locals instead of zero-initialised ones. Slots are
// therefore freed from a pthread_key destructor, which Embox runs at thread
// exit (embox.compat.posix.pthread_key is in the board template).
static pthread_key_t s_exitKey;
static pthread_once_t s_exitKeyOnce = PTHREAD_ONCE_INIT;

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

static uintptr_t emboxSelf() { return reinterpret_cast<uintptr_t>(pthread_self()); }

// Called with s_mutex held. `created` reports whether this thread needs the
// exit hook armed - that has to happen outside the lock (see below).
static EmutlsTable *tableForSelf(bool *created) {
	// pthread_self(), not __sprt_gettid(): that wrapper used to consult pthread
	// TLS and recurse into this function. Embox has no gettid(2) anyway.
	const uintptr_t self = emboxSelf();
	EmutlsTable *free = nullptr;
	for (auto &t : s_tables) {
		if (t.used) {
			if (t.self == self) {
				*created = false;
				return &t;
			}
		} else if (!free) {
			free = &t;
		}
	}
	if (!free) {
		// More live threads than slots. Embox's own thread pool is smaller than
		// this table, so reaching here means the pool was resized without
		// resizing this.
		abort();
	}
	free->used = true;
	free->self = self;
	free->slots = nullptr;
	free->nslots = 0;
	*created = true;
	return free;
}

static void releaseTable(void *) {
	const uintptr_t self = emboxSelf();
	pthread_mutex_lock(&s_mutex);
	for (auto &t : s_tables) {
		if (t.used && t.self == self) {
			for (uintptr_t i = 0; i < t.nslots; ++i) {
				if (t.slots[i]) {
					// allocateObject() over-allocates and stores the malloc base
					// in the word below the aligned object.
					free(reinterpret_cast<void **>(t.slots[i])[-1]);
				}
			}
			free(t.slots);
			t.slots = nullptr;
			t.nslots = 0;
			t.used = false;
			break;
		}
	}
	pthread_mutex_unlock(&s_mutex);
}

static void makeExitKey() { pthread_key_create(&s_exitKey, releaseTable); }

extern "C" __attribute__((visibility("default"))) void *__emutls_get_address(
		__emutls_control *control) {
	pthread_mutex_lock(&s_mutex);
	uintptr_t index = control->object.index;
	if (!index) {
		index = ++s_next;
		control->object.index = index;
	}
	bool created = false;
	EmutlsTable *table = tableForSelf(&created);
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

	// Outside the lock on purpose. pthread_once/pthread_setspecific allocate on
	// first use, and an allocator that touches a thread_local would re-enter
	// __emutls_get_address - on a non-recursive mutex that is a deadlock, not a
	// slow path. Only the thread that just claimed a table gets here.
	if (created) {
		pthread_once(&s_exitKeyOnce, makeExitKey);
		pthread_setspecific(s_exitKey, table);
	}
	return ret;
}
