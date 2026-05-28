#define _UNICODE 1
#define UNICODE 1

#include "src/static.c"

void *malloc(size_t s) __SPRT_NOEXCEPT { return mi_malloc(s); }

void *calloc(size_t count, size_t size) __SPRT_NOEXCEPT { return mi_calloc(count, size); }

void *realloc(void *ptr, size_t value) { return mi_realloc(ptr, value); }

void free_sized(void *ptr, size_t size) __SPRT_NOEXCEPT { mi_free(ptr); }

void free_aligned_sized(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT { mi_free(ptr); }

void free(void *ptr) __SPRT_NOEXCEPT { mi_free(ptr); }

int posix_memalign(void **ptr, size_t size, size_t align) {
	return mi_posix_memalign(ptr, size, align);
}

void *aligned_alloc(size_t align, size_t size) { return mi_aligned_alloc(align, size); }

size_t malloc_usable_size(void *p) { return mi_usable_size(p); }
