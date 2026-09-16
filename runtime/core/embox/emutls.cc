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

// No mutex on this path, and that is the point. Embox's pthread_mutex is the
// kernel mutex: it takes sched_lock(), which is the big kernel lock. Measured on
// the kiosk (xenolith-os docs/EMBOX-NEXT.md): about 15 000 calls a second here,
// roughly 900 per frame, each of them a lock and an unlock -- more than half of
// every BKL acquisition the kiosk makes. Everything below is either owned by the
// calling thread or claimed with a compare-exchange.
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

// `created` reports whether this thread needs the exit hook armed - that has to
// happen after the lookup, because arming it allocates (see below).
//
// A row belongs to one thread: once claimed, only that thread reads or writes
// its slots, so the lookup is a scan of at most 32 words and the claim is a
// compare-exchange on `used`. `self` is published after the claim and cleared
// before the row is released, so a row seen as used with a matching `self` is
// this thread's and nobody else's.
static EmutlsTable *tableForSelf(bool *created) {
	// pthread_self(), not __sprt_gettid(): that wrapper used to consult pthread
	// TLS and recurse into this function. Embox has no gettid(2) anyway.
	const uintptr_t self = emboxSelf();

	for (auto &t : s_tables) {
		if (__atomic_load_n(&t.used, __ATOMIC_ACQUIRE)
				&& __atomic_load_n(&t.self, __ATOMIC_RELAXED) == self) {
			*created = false;
			return &t;
		}
	}

	for (auto &t : s_tables) {
		bool expected = false;
		if (!__atomic_load_n(&t.used, __ATOMIC_RELAXED)
				&& __atomic_compare_exchange_n(&t.used, &expected, true, false, __ATOMIC_ACQ_REL,
						__ATOMIC_RELAXED)) {
			t.slots = nullptr;
			t.nslots = 0;
			__atomic_store_n(&t.self, self, __ATOMIC_RELEASE);
			*created = true;
			return &t;
		}
	}

	// More live threads than slots. Embox's own thread pool is smaller than
	// this table, so reaching here means the pool was resized without
	// resizing this.
	abort();
}

// Runs on the exiting thread, so the row it frees is its own and nothing else
// may touch it: `used` is cleared last, and only then may another thread claim
// the row.
static void releaseTable(void *) {
	const uintptr_t self = emboxSelf();
	for (auto &t : s_tables) {
		if (__atomic_load_n(&t.used, __ATOMIC_ACQUIRE)
				&& __atomic_load_n(&t.self, __ATOMIC_RELAXED) == self) {
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
			__atomic_store_n(&t.self, uintptr_t(0), __ATOMIC_RELAXED);
			__atomic_store_n(&t.used, false, __ATOMIC_RELEASE);
			break;
		}
	}
}

static void makeExitKey() { pthread_key_create(&s_exitKey, releaseTable); }

extern "C" __attribute__((visibility("default"))) void *__emutls_get_address(
		__emutls_control *control) {
	// The index identifies the object, not the thread, so it is the one piece of
	// shared state here. Claimed once with a compare-exchange; a thread that
	// loses the race takes the winner's number.
	uintptr_t index = __atomic_load_n(&control->object.index, __ATOMIC_ACQUIRE);
	if (!index) {
		uintptr_t claimed = __atomic_add_fetch(&s_next, 1, __ATOMIC_RELAXED);
		uintptr_t expected = 0;
		if (__atomic_compare_exchange_n(&control->object.index, &expected, claimed, false,
					__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
			index = claimed;
		} else {
			index = expected;
		}
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

	// After the table is usable, not before: pthread_once/pthread_setspecific
	// allocate on first use, and an allocator that touches a thread_local
	// re-enters this function. Only the thread that just claimed a row gets here.
	if (created) {
		pthread_once(&s_exitKeyOnce, makeExitKey);
		pthread_setspecific(s_exitKey, table);
	}
	return ret;
}
