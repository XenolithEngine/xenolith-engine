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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLSTRUCT_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLSTRUCT_H_

#include "SPMemPoolConfig.h"
#include "SPStatus.h"

namespace STAPPLER_VERSIONIZED stappler::memory::custom {

struct SP_LOCAL MemAddr {
	uint32_t size = 0;
	MemAddr *next = nullptr;
	void *address = nullptr;
};

struct SP_LOCAL AllocManager {
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

struct SP_LOCAL MemNode {
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

struct SP_LOCAL Cleanup {
	using Callback = Status (*)(void *data);

	Cleanup *next;
	const void *data;
	Callback fn;

	static void run(Cleanup **cref);
};

struct SP_LOCAL Allocator {
	using AllocMutex = std::recursive_mutex;

	// used to detect stappler allocators vs. APR allocators
	uintptr_t magic = static_cast<uintptr_t>(config::POOL_MAGIC);
	uint32_t last = 0; // largest used index into free
	uint32_t max = config::ALLOCATOR_MAX_FREE_UNLIMITED; // Total size (in BOUNDARY_SIZE multiples)
	uint32_t current = 0; // current allocated size in BOUNDARY_SIZE
	Pool *owner = nullptr;

	AllocMutex mutex;
	std::array<MemNode *, config::MAX_INDEX> buf;
	std::atomic<size_t> allocated;

	static size_t getAllocatorsCount();

	Allocator();
	~Allocator();

	void set_max(size_t);

	MemNode *alloc(size_t);
	void free(MemNode *);

	void lock();
	void unlock();
};

struct SP_LOCAL Pool {
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

struct SP_LOCAL HashEntry {
	HashEntry *next;
	uint32_t hash;
	const void *key;
	size_t klen;
	const void *val;
};

struct SP_LOCAL HashIndex {
	HashTable *ht;
	HashEntry *_self, *_next;
	uint32_t index;

	HashIndex *next();

	void self(const void **key, size_t *klen, void **val);
};

struct SP_LOCAL HashTable {
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

SP_LOCAL void initialize();
SP_LOCAL void terminate();

// creates managed pool (managed by root, if parent in mullptr)
SP_LOCAL Pool *create(Pool *);

SP_LOCAL void destroy(Pool *);
SP_LOCAL void clear(Pool *);

constexpr size_t SIZEOF_MEMNODE(config::SPALIGN_DEFAULT(sizeof(MemNode)));
constexpr size_t SIZEOF_POOL(config::SPALIGN_DEFAULT(sizeof(Pool)));

} // namespace stappler::memory::custom

#endif /* STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLSTRUCT_H_ */
