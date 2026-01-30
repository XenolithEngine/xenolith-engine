/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_SRC_PRIVATE_SPRTMEMSTRUCT_H_
#define RUNTIME_SRC_PRIVATE_SPRTMEMSTRUCT_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/new.h>
#include <sprt/runtime/atomic.h>
#include <sprt/runtime/mutex.h>

#include "private/SPRTDso.h"

using apr_status_t = int;
using apr_size_t = sprt::size_t;
using apr_abortfunc_t = int (*)(int retcode);

typedef struct apr_allocator_t apr_allocator_t;
typedef struct apr_pool_t apr_pool_t;
typedef struct apr_thread_mutex_t apr_thread_mutex_t;

namespace sprt::memory::apr {

struct AprInterface {
	decltype(&_sprt_null_fn) apr_first_fn = &_sprt_null_fn;
	apr_status_t (*apr_allocator_create)(apr_allocator_t **allocator)
			__attribute__((nonnull(1))) = nullptr;
	void (*apr_allocator_destroy)(apr_allocator_t *allocator) __attribute__((nonnull(1))) = nullptr;
	void (*apr_allocator_mutex_set)(apr_allocator_t *allocator, apr_thread_mutex_t *mutex)
			__attribute__((nonnull(1))) = nullptr;
	void (*apr_allocator_owner_set)(apr_allocator_t *allocator, apr_pool_t *pool)
			__attribute__((nonnull(1))) = nullptr;
	apr_pool_t *(*apr_allocator_owner_get)(apr_allocator_t *allocator)
			__attribute__((nonnull(1))) = nullptr;
	void (*apr_allocator_max_free_set)(apr_allocator_t *allocator, apr_size_t size)
			__attribute__((nonnull(1))) = nullptr;

	void (*apr_pool_initialize)() = nullptr;
	void (*apr_pool_terminate)() = nullptr;

	apr_status_t (*apr_pool_create_unmanaged_ex)(apr_pool_t **newpool, apr_abortfunc_t abort_fn,
			apr_allocator_t *allocator) __attribute__((nonnull(1))) = nullptr;

	apr_status_t (*apr_pool_create_ex)(apr_pool_t **newpool, apr_pool_t *parent,
			apr_abortfunc_t abort_fn, apr_allocator_t *allocator)
			__attribute__((nonnull(1))) = nullptr;

	void (*apr_pool_tag)(apr_pool_t *pool, const char *tag) __attribute__((nonnull(1))) = nullptr;
	void (*apr_pool_destroy)(apr_pool_t *p) __attribute__((nonnull(1))) = nullptr;
	void (*apr_pool_clear)(apr_pool_t *p) __attribute__((nonnull(1))) = nullptr;
	void *(*apr_palloc)(apr_pool_t *p, apr_size_t size) __attribute__((alloc_size(2)))
	__attribute__((nonnull(1))) = nullptr;

	void (*apr_pool_cleanup_kill)(apr_pool_t *p, const void *data, apr_status_t (*cleanup)(void *))
			__attribute__((nonnull(3))) = nullptr;

	apr_status_t (*apr_pool_cleanup_null)(void *data) = nullptr;

	void (*apr_pool_cleanup_register)(apr_pool_t *p, const void *data,
			apr_status_t (*plain_cleanup)(void *), apr_status_t (*child_cleanup)(void *))
			__attribute__((nonnull(3, 4))) = nullptr;

	void (*apr_pool_pre_cleanup_register)(apr_pool_t *p, const void *data,
			apr_status_t (*plain_cleanup)(void *)) __attribute__((nonnull(3))) = nullptr;

	apr_status_t (*apr_pool_userdata_set)(const void *data, const char *key,
			apr_status_t (*cleanup)(void *), apr_pool_t *pool)
			__attribute__((nonnull(2, 4))) = nullptr;

	apr_status_t (*apr_pool_userdata_setn)(const void *data, const char *key,
			apr_status_t (*cleanup)(void *), apr_pool_t *pool)
			__attribute__((nonnull(2, 4))) = nullptr;

	apr_status_t (*apr_pool_userdata_get)(void **data, const char *key, apr_pool_t *pool)
			__attribute__((nonnull(1, 2, 3))) = nullptr;

	apr_allocator_t *(*apr_pool_allocator_get)(apr_pool_t *pool)
			__attribute__((nonnull(1))) = nullptr;

	void *(*apr_pmemdup)(apr_pool_t *p, const void *m, apr_size_t n)
			__attribute__((alloc_size(3))) = nullptr;
	char *(*apr_pstrdup)(apr_pool_t *p, const char *s) = nullptr;
	decltype(&_sprt_null_fn) apr_last_fn = &_sprt_null_fn;

	Dso _handle;

	bool load(Dso &&);

	explicit operator bool() const;
};

} // namespace sprt::memory::apr

namespace sprt::memory::impl {

struct SPRT_LOCAL MemAddr {
	uint32_t size = 0;
	MemAddr *next = nullptr;
	void *address = nullptr;
};

struct SPRT_LOCAL AllocManager : public AllocPlacement {
	using AllocFn = void *(*)(void *, size_t, uint32_t);
	void *pool = nullptr;
	MemAddr *buffered = nullptr;
	MemAddr *free_buffered = nullptr;

	const char *name = nullptr;
	uint32_t tag = 0;
	const void *ptr = 0;

	size_t alloc_buffer = 0;
	size_t allocated = 0;
	size_t returned = 0;

	void reset(void *);

	void *alloc(size_t &sizeInBytes, uint32_t alignment, AllocFn);
	void free(void *ptr, size_t sizeInBytes, AllocFn);

	void increment_alloc(size_t s) {
		allocated += s;
		alloc_buffer += s;
	}
	void increment_return(size_t s) { returned += s; }

	size_t get_alloc() { return allocated; }
	size_t get_return() { return returned; }
};

struct Pool;
struct HashTable;

struct SPRT_LOCAL MemNode : public AllocPlacement {
	MemNode *next; // next memnode
	MemNode **ref; // reference to self
	uint32_t mapped : 1;
	uint32_t index	: 31; // size
	uint32_t free_index; // how much free
	uint8_t *first_avail; // pointer to first free memory
	uint8_t *endp; // pointer to end of free memory

	void insert(MemNode *point);
	void remove();

	size_t free_space() const;
};

struct SPRT_LOCAL Cleanup {
	using Callback = Status (*)(void *data);

	Cleanup *next;
	const void *data;
	Callback fn;

	static void run(Cleanup **cref);
};

struct SPRT_LOCAL Allocator {
	using AllocMutex = sprt::rmutex;

	// used to detect stappler allocators vs. APR allocators
	uintptr_t magic = static_cast<uintptr_t>(config::POOL_MAGIC);
	uint32_t last = 0; // largest used index into free
	uint32_t max = config::ALLOCATOR_MAX_FREE_UNLIMITED; // Total size (in BOUNDARY_SIZE multiples)
	uint32_t current = 0; // current allocated size in BOUNDARY_SIZE
	Pool *owner = nullptr;

	AllocMutex mutex;
	array<MemNode *, config::MAX_INDEX> buf;
	atomic<size_t> allocated;

	static size_t getAllocatorsCount();

	Allocator();
	~Allocator();

	void set_max(size_t);

	MemNode *alloc(size_t);
	void free(MemNode *);

	void lock();
	void unlock();
};

struct SPRT_LOCAL Pool : public AllocPlacement {
	Pool *parent = nullptr;
	Pool *child = nullptr;
	Pool *sibling = nullptr;
	Pool **ref = nullptr;
	Cleanup *cleanups = nullptr;
	Cleanup *free_cleanups = nullptr;
	Allocator *allocator = nullptr;
	uintptr_t magic = static_cast<uintptr_t>(
			config::POOL_MAGIC); // used to detect stappler pools vs. APR pools
	MemNode *active = nullptr;
	MemNode *self = nullptr; /* The node containing the pool itself */
	uint8_t *self_first_avail = nullptr;
	Cleanup *pre_cleanups = nullptr;
	HashTable *user_data = nullptr;

	AllocManager allocmngr;

	static Pool *create(Allocator *alloc = nullptr);
	static void destroy(Pool *);
	static size_t getPoolsCount();

	Pool();
	Pool(Allocator *alloc, MemNode *node);
	Pool(Pool *parent, Allocator *alloc, MemNode *node);
	~Pool();

	void *alloc(size_t &sizeInBytes, uint32_t = config::DefaultAlignment);
	void free(void *ptr, size_t sizeInBytes);

	void *palloc(size_t, uint32_t = config::DefaultAlignment);
	void *palloc_self(size_t);
	void *calloc(size_t count, size_t eltsize);

	void *pmemdup(const void *m, size_t n);
	char *pstrdup(const char *s);

	void clear();

	Pool *make_child();
	Pool *make_child(Allocator *);

	void cleanup_register(const void *, Cleanup::Callback cb);
	void pre_cleanup_register(const void *, Cleanup::Callback cb);

	void cleanup_kill(void *, Cleanup::Callback cb);
	void cleanup_run(void *, Cleanup::Callback cb);

	Status userdata_set(const void *data, const char *key, Cleanup::Callback cb);
	Status userdata_setn(const void *data, const char *key, Cleanup::Callback cb);
	Status userdata_get(void **data, const char *key);
	Status userdata_get(void **data, const char *key, size_t);
};

using HashFunc = uint32_t (*)(const char *key, size_t *klen);

struct SPRT_LOCAL HashEntry {
	HashEntry *next;
	uint32_t hash;
	const void *key;
	size_t klen;
	const void *val;
};

struct SPRT_LOCAL HashIndex {
	HashTable *ht;
	HashEntry *_self, *_next;
	uint32_t index;

	HashIndex *next();

	void self(const void **key, size_t *klen, void **val);
};

struct SPRT_LOCAL HashTable {
	using merge_fn = void *(*)(Pool * p, const void *key, size_t klen, const void *h1_val,
			const void *h2_val, const void *data);
	using foreach_fn = bool (*)(void *rec, const void *key, size_t klen, const void *value);

	Pool *pool;
	HashEntry **array;
	HashIndex iterator; /* For apr_hash_first(NULL, ...) */
	uint32_t count, max, seed;
	HashFunc hash_func;
	HashEntry *free; /* List of recycled entries */

	static void init(HashTable *ht, Pool *pool);

	static HashTable *make(Pool *pool);
	static HashTable *make(Pool *pool, HashFunc);

	HashIndex *first(Pool *p = nullptr);

	HashTable *copy(Pool *pool) const;

	void *get(const void *key, size_t klen);
	void set(const void *key, size_t klen, const void *val);

	size_t size() const;

	void clear();

	HashTable *merge(Pool *, const HashTable *ov) const;
	HashTable *merge(Pool *, const HashTable *ov, merge_fn, const void *data) const;

	bool foreach (foreach_fn, void *rec) const;
};

SPRT_LOCAL void initialize();
SPRT_LOCAL void terminate();

// creates managed pool (managed by root, if parent in mullptr)
SPRT_LOCAL Pool *create(Pool *);

SPRT_LOCAL void destroy(Pool *);
SPRT_LOCAL void clear(Pool *);

constexpr size_t SIZEOF_MEMNODE(config::align_default(sizeof(MemNode)));
constexpr size_t SIZEOF_POOL(config::align_default(sizeof(Pool)));

} // namespace sprt::memory::impl

#endif /* STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLSTRUCT_H_ */
