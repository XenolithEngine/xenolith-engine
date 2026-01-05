/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_POOL_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_POOL_H_

#include <sprt/runtime/status.h>
#include <sprt/runtime/math.h>

/*
	The basic functions for working with memory pools are defined here.
	A memory pool is used to quickly allocate memory with relaxed control over its return.
	Memory allocated from the pool does not need to be returned to the system.
	Instead, all memory will be freed when the pool is destroyed.

	It is the user's responsibility to ensure that memory is not used after the pool is destroyed.

	Typically, a memory pool is tied to some context (processing a network request, drawing a frame),
	within which all actions allocate memory from it.
	
	It is not recommended to use memory from memory pools between threads, except for immutable blocks.

	The memory pool subsystem, when compiled with the stappler_apr module, is partially compatible with the Apache Portable Runtime
	That is, any pools and allocators from APR work in the stappler environment, but stappler pools and allocators do not work in APR

	Among other things, using the stappler functions, you can create child APR pools from the base APR pool,
	and these will also be APR pools capable of running in an APR environment.

	You need to keep in mind the possibility of returning a sufficiently large block of memory
	(more than BlockThreshold) to the memory pool. Extensive use of this optimization can reduce
	system performance, however, limited use can significantly reduce memory consumption for
	temporary operations (for example, creating a string using a stream)

	Many features of memory pool-based containers are optimized to take advantage of returning
	large blocks of memory. For example, reserve_block_optimal allows you to reserve a block of
	memory, which can then be reused with a guarantee for a temporary container.
*/


namespace sprt::memory::config {

// Compatibility with Apache Portable Runtime
static constexpr int AprCompatible = 1;

// minimal size of block, that can be reallocated
static constexpr uint32_t BlockThreshold = 256;

// standard alignment when allocating memory. Must be at least 8 (16 recommended)
static constexpr uint32_t DefaultAlignment = 16;

// Default alignment, 16-bytes is compatible with SSE or other 128-bit SIMD
static constexpr size_t align_default(size_t size) {
	return math::align<size_t>(size, DefaultAlignment);
}

// Adapted for 16k pages
static constexpr uint32_t BOUNDARY_INDEX(13);
static constexpr uint32_t BOUNDARY_SIZE(1 << BOUNDARY_INDEX);

static constexpr uint32_t MIN_ALLOC(2 * BOUNDARY_SIZE);
static constexpr uint32_t MAX_INDEX(20);
static constexpr uint32_t ALLOCATOR_MAX_FREE_UNLIMITED(0);

// address space (not actual mem) reservation for mmap allocator
// you can not allocate more then this with mmap
static constexpr size_t ALLOCATOR_MMAP_RESERVED = size_t(64_GiB);

// Can be 64-bit or stripped to 32-bit
static constexpr uint64_t POOL_MAGIC = 0xDEAD'7fff'DEAD'7fff;

} // namespace sprt::memory::config


namespace sprt::memory {

// Hide pool implementation details from users
// Note that OpaquePool and OpaqueAllocator are never defined
class OpaquePool;
class OpaqueAllocator;

using pool_t = OpaquePool;
using allocator_t = OpaqueAllocator;

using cleanup_fn = Status (*)(void *);

// Use when you need to create pool from application root pool
constexpr pool_t *app_root_pool = nullptr;

// Allocator to use if you want to create an self-contained memory pool
constexpr allocator_t *self_contained_allocator = nullptr;

// Use this for a static init with memory::* types
SPRT_API pool_t *get_zero_pool();

// This pool can be used to store thread-associated data
// It will be destroyed when the thread finishes executing
// For the main thread, it will be zero_pool (see above)
//
// This pool is lazy-init and self-contained
// You SHOULD NOT clear this pool, it can store critical execution data.
// You SHOULD NOT call this function during static initialization, because
// ordering of thread_local and static initializatiors is not defined.
SPRT_API pool_t *get_thread_support_pool();

// Check if Apache portale runtime is available at runtime
SPRT_API bool is_apr_available();

} // namespace sprt::memory


namespace sprt::memory::allocator {

/*
	Creates an allocator for memory pools
	An allocator can be tied to a specific pool or exist separately.
	The user must ensure that the pool does not outlive its allocator.
	All allocators in stappler are thread-safe (but allocating memory from pools is not).
	That is, you can freely transfer allocators between threads.
*/
SPRT_API allocator_t *create();

SPRT_API allocator_t *create_apr(void *mutex = nullptr);

SPRT_API void owner_set(allocator_t *alloc, pool_t *pool);
SPRT_API pool_t *owner_get(allocator_t *alloc);

SPRT_API void max_free_set(allocator_t *alloc, size_t size);

SPRT_API void destroy(allocator_t *);

} // namespace sprt::memory::allocator


namespace sprt::memory::pool {

/*
	Initializes the memory pool subsystem
	The call is thread safe
	Every call to initialize must be balanced with terminate

	Before initialization, it is impossible to use memory pools other than independent ones and those with a dedicated allocator.
*/
SPRT_API void initialize();
SPRT_API void terminate();

/*
	Creates a memory pool associated with its own internal allocator.
	Such memory pools do not require prior stappler initialization
*/
SPRT_API pool_t *create();

/*
	Creates a memory pool associated with its own internal allocator.
	Such memory pools do not require prior stappler initialization
*/
SPRT_API pool_t *create_tagged(const char *);

/*
	Create a pool with a dedicated an allocator
	Such memory pools do not require prior stappler initialization
*/
SPRT_API pool_t *create(allocator_t *);

/*
	Creates a standard memory pool inherited from the specified one
	Such a memory pool cannot live longer than the specified one
	To create a pool that inherits from the root memory pool, pass app_root_pool
*/
SPRT_API pool_t *create(pool_t *);

/*
	Creates a standard memory pool inherited from the specified one
	Such a memory pool cannot live longer than the specified one
	To create a pool that inherits from the root memory pool, pass app_root_pool
*/
SPRT_API pool_t *create_tagged(pool_t *, const char *);

/*
	Creates an memory pool for Apache Portable Runtime, compatible with stappler functions.
	Technically, any APR pool is compatible with stappler values ​​if the stappler_apr module is built
*/
SPRT_API pool_t *create_apr(allocator_t * = nullptr);

/*
	Creates an memory pool for Apache Portable Runtime, compatible with stappler functions.
	Technically, any APR pool is compatible with stappler values ​​if the stappler_apr module is built
*/
SPRT_API pool_t *create_apr_tagged(const char *);

SPRT_API void destroy(pool_t *);
SPRT_API void clear(pool_t *);

/*
	Allocates memory from the memory pool
	size - input - the size of the memory block in bytes
	     - output - the actual allocated block size which may be larger than the required one
*/
SPRT_API void *alloc(pool_t *, size_t &size);

/*
	Allocates memory from the memory pool
	size - input - the size of the memory block in bytes
	     - output - the actual allocated block size which may be larger than the required one
	alignment - memory alignment requirement (if less then DefaultAlignment - ignored)

	NOTE: APR pools cannot return aligned memory, so they return nullptr
*/
SPRT_API void *alloc(pool_t *, size_t &size, uint32_t alignment);

/*
	Allocates memory from the memory pool
	size - the size of the memory block in bytes
*/
SPRT_API void *palloc(pool_t *, size_t);

/*
	Allocates memory from the memory pool
	size - the size of the memory block in bytes
	alignment - memory alignment requirement (if less then DefaultAlignment - ignored)

	NOTE: APR pools cannot return aligned memory, so they return nullptr
*/
SPRT_API void *palloc(pool_t *, size_t, uint32_t alignment);

SPRT_API void *calloc(pool_t *, size_t count, size_t eltsize);

/*
	Returns a block of memory to be re-allocated if possible
	The dimension must be greater than BlockThreshold
*/
SPRT_API void free(pool_t *, void *ptr, size_t size);

SPRT_API void cleanup_kill(pool_t *, void *, cleanup_fn);
SPRT_API void cleanup_register(pool_t *, void *, cleanup_fn);
SPRT_API void pre_cleanup_register(pool_t *, void *, cleanup_fn);

SPRT_API Status userdata_set(const void *data, const char *key, cleanup_fn, pool_t *);
SPRT_API Status userdata_setn(const void *data, const char *key, cleanup_fn, pool_t *);
SPRT_API Status userdata_get(void **data, const char *key, pool_t *);
SPRT_API Status userdata_get(void **data, const char *key, size_t, pool_t *);

SPRT_API allocator_t *get_allocator(pool_t *);

SPRT_API void *pmemdup(pool_t *a, const void *m, size_t n);
SPRT_API char *pstrdup(pool_t *a, const char *s);

SPRT_API const char *get_tag(pool_t *);

// debug counters
SPRT_API size_t get_allocated_bytes(pool_t *);
SPRT_API size_t get_return_bytes(pool_t *);
SPRT_API size_t get_active_count();

SPRT_API void set_pool_info(pool_t *pool, uint32_t tag, const void *ptr);

/*
// start recording additional pool info on creation
SPRT_API bool debug_begin(pool_t *pool = nullptr);

// stop recording and return info
SPRT_API std::map<pool_t *, const char **, less<void>> debug_end();

SPRT_API void debug_foreach(void *, void (*)(void *, pool_t *));
*/

} // namespace sprt::memory::pool

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_POOL_H_
