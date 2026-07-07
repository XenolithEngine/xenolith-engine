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

/*
 * Simple native WebAssembly allocator (wasm-port-draft.adoc §7, "own dlmalloc-
 * class allocator over memory.grow" — replaces mimalloc, whose os.c primitive
 * layer assumes mmap/VirtualAlloc).
 *
 * Design (intentionally minimal for the first milestone):
 *   - The heap begins at the linker-provided __heap_base and grows only upward,
 *     one or more 64 KiB pages at a time via memory.grow (wasm memory never
 *     shrinks — munmap-style returns just go on a free list).
 *   - Each allocation carries a 16-byte header holding the payload size; the
 *     returned pointer is always header + 16 and is at least 16-byte aligned
 *     (max_align_t), so free() recovers the header by simple subtraction.
 *   - Reuse is a single first-fit free list (no coalescing yet — freed blocks
 *     are reused whole or bump-allocated fresh). Good enough for correctness;
 *     fragmentation reduction is a later refinement.
 *   - A tiny atomic spinlock guards the global state so it is safe once the
 *     wasi-threads milestone lands; it is uncontended in the single-thread build.
 *
 * Exposes the same public C entry points the mimalloc SCU did, so nothing else
 * in the runtime needs to change.
 */

typedef __SIZE_TYPE__ size_t;
typedef __UINTPTR_TYPE__ uintptr_t;

#ifndef __SPRT_NOEXCEPT
#define __SPRT_NOEXCEPT
#endif

// Payload alignment and header size. Header must be a multiple of ALIGN so that
// (header + HEADER_SIZE) keeps the payload aligned.
#define WASM_MALLOC_ALIGN ((size_t)16)
#define WASM_MALLOC_HEADER ((size_t)16)
#define WASM_PAGE_BYTES ((uintptr_t)65536)

// Linker-provided start of the heap region (after static data + shadow stack).
extern unsigned char __heap_base;

// A free block reuses the first bytes of its header region: `size` overlays the
// allocation header's size field, `next` follows it (both fit in HEADER bytes).
struct wasm_free_block {
	size_t size;
	struct wasm_free_block *next;
};

static uintptr_t s_heap_top = 0; // next byte available for bump allocation
static struct wasm_free_block *s_free_list = 0;
static volatile int s_lock = 0;

static void wasm_lock(void) {
	while (__atomic_exchange_n(&s_lock, 1, __ATOMIC_ACQUIRE)) {
		// spin; uncontended in the single-thread build
	}
}

static void wasm_unlock(void) { __atomic_store_n(&s_lock, 0, __ATOMIC_RELEASE); }

static size_t wasm_align_up(size_t n, size_t a) { return (n + a - 1) & ~(a - 1); }

static uintptr_t wasm_heap_end(void) {
	return (uintptr_t)__builtin_wasm_memory_size(0) * WASM_PAGE_BYTES;
}

// Grow linear memory so that at least `needed_end` bytes are addressable.
// Returns 1 on success, 0 if memory.grow refused.
static int wasm_grow_to(uintptr_t needed_end) {
	uintptr_t cur = wasm_heap_end();
	if (needed_end <= cur) {
		return 1;
	}
	uintptr_t pages = (needed_end - cur + WASM_PAGE_BYTES - 1) / WASM_PAGE_BYTES;
	size_t prev = (size_t)__builtin_wasm_memory_grow(0, (uintptr_t)pages);
	return prev != (size_t)-1;
}

static void wasm_heap_init(void) {
	if (s_heap_top == 0) {
		s_heap_top = wasm_align_up((uintptr_t)&__heap_base, WASM_MALLOC_ALIGN);
	}
}

void *malloc(size_t size) __SPRT_NOEXCEPT {
	if (size == 0) {
		size = 1;
	}
	size = wasm_align_up(size, WASM_MALLOC_ALIGN);

	wasm_lock();

	// First-fit reuse from the free list.
	struct wasm_free_block **pp = &s_free_list;
	for (; *pp; pp = &(*pp)->next) {
		if ((*pp)->size >= size) {
			struct wasm_free_block *b = *pp;
			*pp = b->next;
			wasm_unlock();
			return (void *)((uintptr_t)b + WASM_MALLOC_HEADER);
		}
	}

	// Otherwise bump-allocate, growing linear memory as needed.
	wasm_heap_init();
	uintptr_t hdr = s_heap_top;
	uintptr_t payload = hdr + WASM_MALLOC_HEADER;
	uintptr_t new_top = payload + size;
	if (!wasm_grow_to(new_top)) {
		wasm_unlock();
		return 0;
	}
	*(size_t *)hdr = size;
	s_heap_top = new_top;

	wasm_unlock();
	return (void *)payload;
}

void free(void *ptr) __SPRT_NOEXCEPT {
	if (!ptr) {
		return;
	}
	struct wasm_free_block *b = (struct wasm_free_block *)((uintptr_t)ptr - WASM_MALLOC_HEADER);
	wasm_lock();
	// b->size (header offset 0) is preserved from allocation.
	b->next = s_free_list;
	s_free_list = b;
	wasm_unlock();
}

size_t malloc_usable_size(void *p) {
	if (!p) {
		return 0;
	}
	return *(size_t *)((uintptr_t)p - WASM_MALLOC_HEADER);
}

void *calloc(size_t count, size_t size) __SPRT_NOEXCEPT {
	size_t total = count * size;
	if (count != 0 && total / count != size) {
		return 0; // multiplication overflow
	}
	void *p = malloc(total);
	if (p) {
		__builtin_memset(p, 0, total);
	}
	return p;
}

void *realloc(void *ptr, size_t size) {
	if (!ptr) {
		return malloc(size);
	}
	if (size == 0) {
		free(ptr);
		return 0;
	}
	size_t old = malloc_usable_size(ptr);
	if (wasm_align_up(size, WASM_MALLOC_ALIGN) <= old) {
		return ptr; // fits in place
	}
	void *n = malloc(size);
	if (!n) {
		return 0;
	}
	__builtin_memcpy(n, ptr, old);
	free(ptr);
	return n;
}

// Aligned allocation always bump-allocates (never reuses the free list) so the
// header sits exactly WASM_MALLOC_HEADER bytes before the aligned payload and
// free()/malloc_usable_size() keep working unchanged.
void *aligned_alloc(size_t align, size_t size) {
	if (align < WASM_MALLOC_ALIGN) {
		align = WASM_MALLOC_ALIGN;
	}
	size = wasm_align_up(size, WASM_MALLOC_ALIGN);

	wasm_lock();
	wasm_heap_init();
	uintptr_t payload = wasm_align_up(s_heap_top + WASM_MALLOC_HEADER, align);
	uintptr_t hdr = payload - WASM_MALLOC_HEADER;
	uintptr_t new_top = payload + size;
	if (!wasm_grow_to(new_top)) {
		wasm_unlock();
		return 0;
	}
	*(size_t *)hdr = size;
	s_heap_top = new_top;
	wasm_unlock();
	return (void *)payload;
}

int posix_memalign(void **out, size_t align, size_t size) {
	if (!out) {
		return 22; // EINVAL
	}
	// align must be a power of two and a multiple of sizeof(void*).
	if (align < sizeof(void *) || (align & (align - 1)) != 0) {
		return 22; // EINVAL
	}
	void *p = aligned_alloc(align, size);
	if (!p) {
		return 12; // ENOMEM
	}
	*out = p;
	return 0;
}

void aligned_free(void *memblock) { free(memblock); }

void free_sized(void *ptr, size_t size) __SPRT_NOEXCEPT {
	(void)size;
	free(ptr);
}

void free_aligned_sized(void *ptr, size_t alignment, size_t size) __SPRT_NOEXCEPT {
	(void)alignment;
	(void)size;
	free(ptr);
}
