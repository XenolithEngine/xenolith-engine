#include "mimalloc.h"
#define _UNICODE 1
#define UNICODE 1

#define MI_DEBUG 0

#include "src/static.c"

#if defined(__EMBOX__) && !defined(__EMBOX_USER__)

void __sprt_embox_kernel_free(void *);
void *__sprt_embox_kernel_realloc(void *, size_t);

static inline bool __sprt_mi_owns(const void *p) { return p == NULL || mi_is_in_heap_region(p); }

void *__sprt_malloc_impl(size_t s) __SPRT_NOEXCEPT { return mi_malloc(s); }

void *__sprt_calloc_impl(size_t count, size_t size) __SPRT_NOEXCEPT {
	return mi_calloc(count, size);
}

void *__sprt_realloc_impl(void *ptr, size_t value) __SPRT_NOEXCEPT {
	if (!__sprt_mi_owns(ptr)) {
		return __sprt_embox_kernel_realloc(ptr, value);
	}
	return mi_realloc(ptr, value);
}

void __sprt_free_impl(void *ptr) __SPRT_NOEXCEPT {
	if (!__sprt_mi_owns(ptr)) {
		__sprt_embox_kernel_free(ptr);
		return;
	}
	mi_free(ptr);
}

void __sprt_free_sized(void *ptr, size_t size) __SPRT_NOEXCEPT {
	if (!__sprt_mi_owns(ptr)) {
		__sprt_embox_kernel_free(ptr);
		return;
	}
	mi_free_size(ptr, size);
}

void __sprt_free_aligned_sized(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT {
	if (!__sprt_mi_owns(ptr)) {
		__sprt_embox_kernel_free(ptr);
		return;
	}
	mi_free_size_aligned(ptr, size, alignment);
}

void *__sprt_aligned_alloc(size_t align, size_t size) __SPRT_NOEXCEPT {
	return mi_aligned_alloc(align, size);
}

void __sprt_aligned_free(void *ptr) { __sprt_free_impl(ptr); }

// The runtime's order is (ptr, size, align) -- include/sprt/wrappers/libc/stdlib.h.
int __sprt_posix_memalign(void **ptr, size_t size, size_t align) {
	return mi_posix_memalign(ptr, align, size);
}

void *__sprt_local_alloc(size_t size) __SPRT_NOEXCEPT { return mi_malloc(size); }

void __sprt_local_free(void *ptr, size_t size) __SPRT_NOEXCEPT {
	(void)size;
	__sprt_free_impl(ptr);
}

#else

void *malloc(size_t s) __SPRT_NOEXCEPT { return mi_malloc(s); }

void *calloc(size_t count, size_t size) __SPRT_NOEXCEPT { return mi_calloc(count, size); }

void *realloc(void *ptr, size_t value) { return mi_realloc(ptr, value); }

void aligned_free(void *memblock) { mi_free(memblock); }

void free_sized(void *ptr, size_t size) __SPRT_NOEXCEPT { mi_free_size(ptr, size); }

void free_aligned_sized(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT {
	mi_free_size_aligned(ptr, size, alignment);
}

void free(void *ptr) __SPRT_NOEXCEPT { mi_free(ptr); }

int posix_memalign(void **ptr, size_t align, size_t size) {
	return mi_posix_memalign(ptr, align, size);
}

void *aligned_alloc(size_t align, size_t size) { return mi_aligned_alloc(align, size); }

size_t malloc_usable_size(void *p) { return mi_usable_size(p); }

#endif // Embox EL1

void __sprt_malloc_thread_attach(void) __SPRT_NOEXCEPT {
	mi_heap_t *heap = mi_prim_get_default_heap();

	/* The main thread is the one whose heap is the main heap. Not
	 * _mi_is_main_thread(): it also answers yes while the main heap's thread id
	 * is 0, and on Embox EL0 process init runs before the thread pointer is set,
	 * so there it said yes for every thread. */
	if (heap == &_mi_heap_main) {
		return;
	}
	if (!mi_heap_is_initialized(heap)) {
		mi_free(mi_malloc(1));
		return;
	}
	_mi_prim_thread_associate_default_heap(heap);
}

// Total bytes currently allocated from the default heap, summed over mimalloc's
// areas (used-block count * block size). Backs <malloc.h>'s _heapwalk / llvm's
// Process::GetMallocUsage. Area-level (visit_blocks == false) so the cost is
// O(areas), not O(individual blocks).
static bool __sprt_malloc_usage_visit(const mi_heap_t *heap, const mi_heap_area_t *area,
		void *block, size_t block_size, void *arg) {
	(void)heap;
	(void)block;
	(void)block_size;
	*(size_t *)arg += area->used * area->block_size;
	return true; // keep visiting
}

size_t __sprt_malloc_usage(void) __SPRT_NOEXCEPT {
	size_t total = 0;
	mi_heap_visit_blocks(mi_heap_get_default(), false, &__sprt_malloc_usage_visit, &total);
	return total;
}
