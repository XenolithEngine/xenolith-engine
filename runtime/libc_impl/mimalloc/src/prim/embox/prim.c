/* ----------------------------------------------------------------------------
Copyright (c) 2026 Xenolith Team
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// sprt: Embox EL1 -- the application linked into the kernel image.
//
// This file is included in `src/prim/prim.c`.
//
// MEMORY comes from Embox's physical page allocator, phymem: identity-mapped,
// Normal, cacheable, usable at once. There is no virtual memory to reserve and
// commit separately, so everything mimalloc asks for is committed when it is
// allocated, and commit/decommit/reset are nothing. The kernel's malloc is not
// involved: it is mspace under sched_lock(), the big kernel lock, which is what
// this allocator exists to keep the engine's hot path out of.
//
// phymem takes its own lock (mem/pagealloc/bitmask.c), searches first-fit and
// aligns to 4 KiB only. So an aligned request is over-allocated once and its
// head and tail given back here, rather than mimalloc's generic path, which
// would allocate, find it unaligned, free it and try again larger.
//
// THREADS: see mi_embox_tcb_t in mimalloc/prim.h. The blocks live in a static
// pool below. A thread's heap is handed back through a pthread key of the
// runtime's own (sprt's, whose destructors run at thread exit; Embox's never
// do), and its block with it.

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim.h"

#include <errno.h>
#include <stdio.h> // fputs
#include <stdlib.h> // getenv, abort
#include <string.h> // memset
#include <time.h>
#include <pthread.h> // sprt's

//---------------------------------------------
// The kernel's side, resolved when the image is linked
//---------------------------------------------

extern void *phymem_alloc(size_t page_number);
extern void phymem_free(void *page, size_t page_number);
extern void *phy_allocator(void);
extern void *thread_self(void);

// The head of Embox's struct page_allocator (src/include/mem/page.h), for the
// free count. A request larger than what is free is not refused quickly by
// phymem: the first-fit search walks every free run first.
struct mi_embox_page_allocator {
	void *pages_start;
	unsigned int pages_n;
	size_t page_size;
	size_t free;
};

// Set by the runtime's static initialiser (core/linux/libc.cc): the thread the
// image was started on, as Embox's pthread_self() -- which is thread_self().
extern unsigned long long __libc_main_thread;

#define MI_EMBOX_PAGE 4096

static size_t mi_embox_free_bytes(void) {
	const struct mi_embox_page_allocator *pa =
			(const struct mi_embox_page_allocator *)phy_allocator();
	return (pa != NULL ? pa->free : 0);
}

//---------------------------------------------
// Initialize
//---------------------------------------------

void _mi_prim_mem_init(mi_os_mem_config_t *config) {
	const struct mi_embox_page_allocator *pa =
			(const struct mi_embox_page_allocator *)phy_allocator();
	config->page_size = MI_EMBOX_PAGE;
	config->large_page_size = 0;
	config->alloc_granularity = MI_EMBOX_PAGE;
	if (pa != NULL) {
		config->physical_memory_in_kib = ((size_t)pa->pages_n * pa->page_size) / MI_KiB;
	}
	config->has_overcommit = false; // physical memory, handed over on the spot
	config->has_partial_free = true; // phymem frees any sub-range of a run
	config->has_virtual_reserve = false; // nothing to reserve without committing
}

//---------------------------------------------
// Free
//---------------------------------------------

int _mi_prim_free(void *addr, size_t size) {
	if (addr == NULL || size == 0) {
		return 0;
	}
	phymem_free(addr, _mi_divide_up(size, MI_EMBOX_PAGE));
	return 0;
}

//---------------------------------------------
// Allocation
//---------------------------------------------

int _mi_prim_alloc(void *hint_addr, size_t size, size_t try_alignment, bool commit,
		bool allow_large, bool *is_large, bool *is_zero, void **addr) {
	MI_UNUSED(hint_addr);
	MI_UNUSED(commit);
	MI_UNUSED(allow_large);
	*is_large = false;
	*is_zero = false;
	*addr = NULL;

	const size_t pages = _mi_divide_up(size, MI_EMBOX_PAGE);
	const size_t align_pages = (try_alignment > MI_EMBOX_PAGE ? try_alignment / MI_EMBOX_PAGE : 1);
	const size_t take = pages + align_pages - 1;

	if (take * MI_EMBOX_PAGE > mi_embox_free_bytes()) {
		return ENOMEM;
	}

	uint8_t *p = (uint8_t *)phymem_alloc(take);
	if (p == NULL) {
		return ENOMEM;
	}

	// One search for an aligned run: take it with room to spare, keep the
	// aligned part, give the edges back.
	uint8_t *aligned = (uint8_t *)_mi_align_up((uintptr_t)p, align_pages * MI_EMBOX_PAGE);
	const size_t head = (size_t)(aligned - p) / MI_EMBOX_PAGE;
	const size_t tail = take - head - pages;
	if (head > 0) {
		phymem_free(p, head);
	}
	if (tail > 0) {
		phymem_free(aligned + pages * MI_EMBOX_PAGE, tail);
	}

	*addr = aligned;
	return 0;
}

//---------------------------------------------
// Commit/Reset/Protect: the memory is physical and always committed
//---------------------------------------------

int _mi_prim_commit(void *addr, size_t size, bool *is_zero) {
	MI_UNUSED(addr);
	MI_UNUSED(size);
	*is_zero = false;
	return 0;
}

int _mi_prim_decommit(void *addr, size_t size, bool *needs_recommit) {
	MI_UNUSED(addr);
	MI_UNUSED(size);
	*needs_recommit = false;
	return 0;
}

int _mi_prim_reset(void *addr, size_t size) {
	MI_UNUSED(addr);
	MI_UNUSED(size);
	return 0;
}

int _mi_prim_reuse(void *addr, size_t size) {
	MI_UNUSED(addr);
	MI_UNUSED(size);
	return 0;
}

int _mi_prim_protect(void *addr, size_t size, bool protect) {
	MI_UNUSED(addr);
	MI_UNUSED(size);
	MI_UNUSED(protect);
	return 0;
}

//---------------------------------------------
// Huge pages and NUMA
//---------------------------------------------

int _mi_prim_alloc_huge_os_pages(void *hint_addr, size_t size, int numa_node, bool *is_zero,
		void **addr) {
	MI_UNUSED(hint_addr);
	MI_UNUSED(size);
	MI_UNUSED(numa_node);
	*is_zero = false;
	*addr = NULL;
	return ENOMEM;
}

size_t _mi_prim_numa_node(void) { return 0; }

size_t _mi_prim_numa_node_count(void) { return 1; }

//----------------------------------------------------------------
// Clock, process info, output, environment, random
//----------------------------------------------------------------

mi_msecs_t _mi_prim_clock_now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return ((mi_msecs_t)t.tv_sec * 1000) + ((mi_msecs_t)t.tv_nsec / 1000000);
}

void _mi_prim_process_info(mi_process_info_t *pinfo) {
	MI_UNUSED(pinfo); // defaults
}

void _mi_prim_out_stderr(const char *msg) { fputs(msg, __sprt_stderr_impl()); }

bool _mi_prim_getenv(const char *name, char *result, size_t result_size) {
	if (_mi_preloading()) {
		return false;
	}
	const char *s = getenv(name);
	if (s == NULL) {
		char buf[64 + 1];
		size_t len = _mi_strnlen(name, sizeof(buf) - 1);
		for (size_t i = 0; i < len; i++) { buf[i] = _mi_toupper(name[i]); }
		buf[len] = 0;
		s = getenv(buf);
	}
	if (s == NULL || _mi_strnlen(s, result_size) >= result_size) {
		return false;
	}
	_mi_strlcpy(result, s, result_size);
	return true;
}

bool _mi_prim_random_buf(void *buf, size_t buf_len) {
	MI_UNUSED(buf);
	MI_UNUSED(buf_len);
	return false; // mimalloc falls back to its own weak random, seeded by the clock
}

//----------------------------------------------------------------
// The thread's block (mi_embox_tcb_t, mimalloc/prim.h)
//----------------------------------------------------------------

// More than the threads an image has alive at once: a block is held only by a
// thread that has allocated, and goes back when the thread is done.
#define MI_EMBOX_TCB_COUNT 256

static mi_embox_tcb_t mi_embox_tcb_pool[MI_EMBOX_TCB_COUNT] __attribute__((aligned(64)));
static mi_embox_tcb_t *mi_embox_tcb_free; // blocks given back
static size_t mi_embox_tcb_next; // blocks never handed out start here
static _Atomic(uintptr_t)
		mi_embox_tcb_lock; // both of the above; taken at thread start and exit only

static void mi_embox_tcb_lock_take(void) {
	while (mi_atomic_exchange_acq_rel(&mi_embox_tcb_lock, (uintptr_t)1) != 0) { mi_atomic_yield(); }
}

static void mi_embox_tcb_lock_give(void) {
	mi_atomic_store_release(&mi_embox_tcb_lock, (uintptr_t)0);
}

static inline void mi_embox_tpidr_set(mi_embox_tcb_t *t) {
	__asm__ volatile("msr tpidr_el0, %0" : : "r"((uintptr_t)t) : "memory");
}

uintptr_t _mi_embox_thread_self(void) mi_attr_noexcept { return (uintptr_t)thread_self(); }

mi_embox_tcb_t *_mi_embox_tcb_attach(void) mi_attr_noexcept {
	mi_embox_tcb_t *t = mi_embox_tcb_peek();
	if (t != NULL) {
		return t;
	}

	mi_embox_tcb_lock_take();
	t = mi_embox_tcb_free;
	if (t != NULL) {
		mi_embox_tcb_free = t->next_free;
	} else if (mi_embox_tcb_next < MI_EMBOX_TCB_COUNT) {
		t = &mi_embox_tcb_pool[mi_embox_tcb_next++];
	}
	mi_embox_tcb_lock_give();

	if (t == NULL) {
		// No safe answer: two threads on one block would share a heap unlocked.
		fputs("mimalloc: every thread block is taken (MI_EMBOX_TCB_COUNT); aborting\n",
				__sprt_stderr_impl());
		abort();
	}

	memset(t, 0, sizeof(*t));
	mi_embox_tpidr_set(t);
	return t;
}

// The calling thread is done with its block: back to the pool, and the
// register back to 0, so anything the thread still frees on its way out takes
// the cross-thread path and a later allocation attaches afresh.
static void mi_embox_tcb_release(void) {
	mi_embox_tcb_t *t = mi_embox_tcb_peek();
	if (t == NULL) {
		return;
	}
	mi_embox_tpidr_set(NULL);
	memset(t, 0, sizeof(*t));
	mi_embox_tcb_lock_take();
	t->next_free = mi_embox_tcb_free;
	mi_embox_tcb_free = t;
	mi_embox_tcb_lock_give();
}

//----------------------------------------------------------------
// Thread init/done, through a pthread key of the runtime's own
//----------------------------------------------------------------

static pthread_key_t mi_embox_heap_done_key = (pthread_key_t)(-1);

static void mi_embox_pthread_done(void *value) {
	if (value != NULL) {
		_mi_thread_done((mi_heap_t *)value);
	}
	mi_embox_tcb_release();
}

// The key is made on first use, not here. This runs inside mimalloc's
// process-init once-lock, and the runtime's pthread_key_create() allocates
// (its key table), which on the thread's first allocation comes back into
// process init and waits on that same lock for ever. By the time a second
// thread associates its heap, that heap is already the thread's default, so
// the allocation inside pthread_key_create() is served from it.
static pthread_once_t mi_embox_heap_done_once = PTHREAD_ONCE_INIT;

static void mi_embox_heap_done_key_create(void) {
	pthread_key_create(&mi_embox_heap_done_key, &mi_embox_pthread_done);
}

void _mi_prim_thread_init_auto_done(void) {
	// nothing: see mi_embox_heap_done_once
}

void _mi_prim_thread_done_auto_done(void) {
	if (mi_embox_heap_done_key != (pthread_key_t)(-1)) {
		pthread_key_delete(mi_embox_heap_done_key);
	}
}

void _mi_prim_thread_associate_default_heap(mi_heap_t *heap) {
	// The thread the image started on never ends, so it needs no exit hook --
	// and it allocates in static constructors, before the runtime's thread pool
	// is built: a pthread_setspecific() then would attach it to a pool that is
	// about to be constructed over it (the fiasco wasm fixes up after the fact
	// with __sprt_wasm_reinit_main_thread). 0 means the runtime's static
	// initialiser has not run yet, and that is the same thread.
	const unsigned long long self = (unsigned long long)(uintptr_t)thread_self();
	if (__libc_main_thread == 0 || __libc_main_thread == self) {
		return;
	}
	pthread_once(&mi_embox_heap_done_once, &mi_embox_heap_done_key_create);
	if (mi_embox_heap_done_key == (pthread_key_t)(-1)) {
		return;
	}
	pthread_setspecific(mi_embox_heap_done_key, heap);
}
