#include "mimalloc.h"
#define _UNICODE 1
#define UNICODE 1

#define MI_DEBUG 0

#include "src/static.c"

void *malloc(size_t s) __SPRT_NOEXCEPT { return mi_malloc(s); }

void *calloc(size_t count, size_t size) __SPRT_NOEXCEPT { return mi_calloc(count, size); }

void *realloc(void *ptr, size_t value) { return mi_realloc(ptr, value); }

void aligned_free(void *memblock) { mi_free(memblock); }

void free_sized(void *ptr, size_t size) __SPRT_NOEXCEPT { mi_free_size(ptr, size); }

void free_aligned_sized(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT {
	mi_free_size_aligned(ptr, size, alignment);
}

void free(void *ptr) __SPRT_NOEXCEPT { mi_free(ptr); }

int posix_memalign(void **ptr, size_t size, size_t align) {
	return mi_posix_memalign(ptr, size, align);
}

void *aligned_alloc(size_t align, size_t size) { return mi_aligned_alloc(align, size); }

size_t malloc_usable_size(void *p) { return mi_usable_size(p); }

// Total bytes currently allocated from the default heap, summed over mimalloc's
// areas (used-block count * block size). Backs <malloc.h>'s _heapwalk / llvm's
// Process::GetMallocUsage. Area-level (visit_blocks == false) so the cost is
// O(areas), not O(individual blocks).
static bool __sprt_malloc_usage_visit(const mi_heap_t *heap, const mi_heap_area_t *area,
		void *block, size_t block_size, void *arg) {
	(void) heap;
	(void) block;
	(void) block_size;
	*(size_t *) arg += area->used * area->block_size;
	return true; // keep visiting
}

size_t __sprt_malloc_usage(void) __SPRT_NOEXCEPT {
	size_t total = 0;
	mi_heap_visit_blocks(mi_heap_get_default(), false, &__sprt_malloc_usage_visit, &total);
	return total;
}
