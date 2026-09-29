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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

// syscall(2) is embox.compat.linux.syscall_stub, which every Xenolith board
// template carries (libc++abi's static-init guards ask it too). Weak, so a
// kernel without it still links; then the pointer is the only identity, as
// before.
extern "C" long syscall(long number, ...) __attribute__((weak));

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
	// onto one table.
	uintptr_t self;
	// The owner's thread id: never reused, so it tells the owner of `self`
	// from an earlier thread that had the same pointer.
	long tid;
	bool used;
	void **slots;
	uintptr_t nslots;
};

// One row per `struct thread *` that has touched a thread_local: the thread
// pool (thread_pool_size, 64 on the Xenolith boards), tasks' main threads,
// the boot and idle threads.
static constexpr unsigned EmutlsRows = 128;

// No mutex on this path, and that is the point. Embox's pthread_mutex is the
// kernel mutex: it takes sched_lock(), which is the big kernel lock. Measured on
// the kiosk (xenolith-os docs/EMBOX-NEXT.md): about 15 000 calls a second here,
// roughly 900 per frame, each of them a lock and an unlock -- more than half of
// every BKL acquisition the kiosk makes. Everything below is either owned by the
// calling thread or claimed with a compare-exchange.
static uintptr_t s_next = 0;
static EmutlsTable s_tables[EmutlsRows];

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

static long emboxThreadId() { return syscall ? syscall(SYS_gettid) : 0; }

// Free a row's objects and its slot array. Only the row's owner, or the thread
// that has just found the row's owner dead, calls this: nobody else reads
// `slots`.
static void emptyTable(EmutlsTable &t) {
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
}

static void tableLimitReached() {
	static const char msg[] = "emutls: every row of the thread_local table is taken"
			" (core/embox/emutls.cc, EmutlsRows)\n";
	write(2, msg, sizeof(msg) - 1);
	abort();
}

// A row belongs to one thread: once claimed, only the thread whose pointer it
// carries reads or writes its slots, so the lookup is a scan of pointers and
// the claim is a compare-exchange on `used`. `self` is published after the
// claim and cleared before the row is released, so a row seen as used with a
// matching `self` belongs to the thread now at that pointer, or to a dead one
// that had it -- and that is what `tid` decides.
static EmutlsTable *tableForSelf() {
	const uintptr_t self = emboxSelf();
	const long tid = emboxThreadId();

	for (auto &t : s_tables) {
		if (__atomic_load_n(&t.used, __ATOMIC_ACQUIRE)
				&& __atomic_load_n(&t.self, __ATOMIC_RELAXED) == self) {
			if (t.tid != tid) {
				// The pointer was a thread's that ended without giving the row
				// back. That thread runs no more, and no other live thread has
				// this pointer, so the row is ours to empty and keep.
				emptyTable(t);
				t.tid = tid;
			}
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
			t.tid = tid;
			__atomic_store_n(&t.self, self, __ATOMIC_RELEASE);
			return &t;
		}
	}

	tableLimitReached();
	return nullptr;
}

namespace sprt {

// The calling thread gives its row back: every thread_local object it had is
// freed. Called by __sprt_libc_thread_exit() as the last thing before the
// thread ends, after sprt's own key destructors, so nothing reads a
// thread_local of this thread afterwards.
void __emutls_release_self() {
	const uintptr_t self = emboxSelf();
	const long tid = emboxThreadId();
	for (auto &t : s_tables) {
		if (__atomic_load_n(&t.used, __ATOMIC_ACQUIRE)
				&& __atomic_load_n(&t.self, __ATOMIC_RELAXED) == self) {
			if (t.tid == tid) {
				emptyTable(t);
				__atomic_store_n(&t.self, uintptr_t(0), __ATOMIC_RELAXED);
				__atomic_store_n(&t.used, false, __ATOMIC_RELEASE);
			}
			break;
		}
	}
}

} // namespace sprt

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
	return table->slots[index - 1];
}
