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

#include "SPMemPoolInterface.h"
#include "SPMemPoolStruct.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler::memory {

pool_t *get_zero_pool() {
	struct ZeroPoolStruct {
		custom::Allocator _alloc;
		custom::Pool *_pool = nullptr;

		ZeroPoolStruct() { _pool = custom::Pool::create(&_alloc); }
		~ZeroPoolStruct() {
			custom::Pool::destroy(_pool);
			_pool = nullptr;
		}
	};

	static ZeroPoolStruct s_struct;
	return (pool_t *)s_struct._pool;
}

} // namespace stappler::memory

namespace STAPPLER_VERSIONIZED stappler::memory::pool {

SPUNUSED static void popPoolInfo(pool_t *pool);

}

namespace STAPPLER_VERSIONIZED stappler::memory::custom {

SPUNUSED static Allocator *s_global_allocator = nullptr;
SPUNUSED static Pool *s_global_pool = nullptr;
SPUNUSED static std::atomic<int> s_global_init = 0;

static std::atomic<size_t> s_nPools = 0;

static void Pool_performCleanup(Pool *pool) {
	perform_conditional([&] { Cleanup::run(&pool->pre_cleanups); },
			(stappler::memory::pool_t *)pool);

	pool->pre_cleanups = nullptr;

	// DO NOT push current pool when childs is destroyed
	while (pool->child) { pool->child->~Pool(); }

	/* Run cleanups */
	perform_conditional([&] { Cleanup::run(&pool->cleanups); }, (stappler::memory::pool_t *)pool);

	pool->cleanups = nullptr;
	pool->free_cleanups = nullptr;
	pool->user_data = nullptr;
}

void *Pool::alloc(size_t &sizeInBytes, uint32_t alignment) {
	if (sizeInBytes >= config::BlockThreshold) {
		return allocmngr.alloc(sizeInBytes, alignment,
				[](void *p, size_t s, uint32_t a) { return ((Pool *)p)->palloc(s, a); });
	}

	allocmngr.increment_alloc(sizeInBytes);
	return palloc(sizeInBytes, alignment);
}

void Pool::free(void *ptr, size_t sizeInBytes) {
	if (sizeInBytes >= config::BlockThreshold) {
		allocmngr.free(ptr, sizeInBytes, [](void *p, size_t s, uint32_t a) {
			if (a == config::DefaultAlignment) {
				return ((Pool *)p)->palloc_self(s);
			} else {
				return ((Pool *)p)->palloc(s, a);
			}
		});
	}
}

void *Pool::palloc(size_t in_size, uint32_t alignment) {
	MemNode *active;
	void *mem;
	size_t size, free_index;

	alignment = std::max(alignment, config::DefaultAlignment);

	if (alignment > 1'024) {
		log::source().error("memory", SP_FUNC, ": alignment value too large: ", alignment);
		return nullptr;
	}

	size = config::SPALIGN_DEFAULT(in_size);
	if (size < in_size) {
		return nullptr;
	}
	active = this->active;

	/* If the active node has enough bytes left, use it. */
	if (size <= active->free_space()) {
		mem = active->first_avail;

		if (alignment > config::DefaultAlignment) {
			size_t space = active->endp - active->first_avail;
			mem = std::align(alignment, in_size, mem, space);
			if (mem) {
				active->first_avail += size + ((active->endp - active->first_avail) - space);
				return mem;
			}
		} else {
			active->first_avail += size;
			return mem;
		}
	}

	auto node = active->next;
	if (size <= node->free_space()) {
		node->remove();
	} else {
		if ((node = allocator->alloc(size)) == NULL) {
			return nullptr;
		}
	}

	node->free_index = 0;

	mem = node->first_avail;

	if (alignment > config::DefaultAlignment) {
		size_t space = node->endp - node->first_avail;
		mem = std::align(alignment, in_size, mem, space);
		if (mem) {
			node->first_avail += size + ((node->endp - node->first_avail) - space);
		} else {
			log::source().error("memory", SP_FUNC,
					": fail to allocate aligned memory: ", alignment);
			return nullptr;
		}
	} else {
		node->first_avail += size;
	}

	node->insert(active);

	this->active = node;

	free_index = (config::SPALIGN(active->endp - active->first_avail + 1, config::BOUNDARY_SIZE)
						 - config::BOUNDARY_SIZE)
			>> config::BOUNDARY_INDEX;

	active->free_index = (uint32_t)free_index;
	node = active->next;
	if (free_index >= node->free_index) {
		return mem;
	}

	do { node = node->next; } while (free_index < node->free_index);

	active->remove();
	active->insert(node);

	return mem;
}

void *Pool::palloc_self(size_t in_size) {
	void *mem;
	auto size = config::SPALIGN_DEFAULT(in_size);
	if (size < in_size) {
		return nullptr;
	}

	/* If the active node has enough bytes left, use it. */
	if (size <= self->free_space()) {
		mem = self->first_avail;
		self->first_avail += size;
		return mem;
	}

	return palloc(in_size);
}

void *Pool::calloc(size_t count, size_t eltsize) {
	size_t s = count * eltsize;
	auto ptr = alloc(s);
	memset(ptr, 0, s);
	return ptr;
}

void *Pool::pmemdup(const void *m, size_t n) {
	if (m == nullptr) {
		return nullptr;
	}
	void *res = palloc(n);
	memcpy(res, m, n);
	return res;
}

char *Pool::pstrdup(const char *s) {
	if (s == nullptr) {
		return nullptr;
	}
	size_t len = strlen(s) + 1;
	char *res = (char *)pmemdup(s, len);
	return res;
}

void Pool::clear() {
	Pool_performCleanup(this);

	/* Find the node attached to the pool structure, reset it, make
	 * it the active node and free the rest of the nodes.
	 */
	MemNode *active = this->active = this->self;
	active->first_avail = this->self_first_avail;

	if (active->next == active) {
		this->allocmngr.reset(this);
		return;
	}

	*active->ref = nullptr;
	if (active->next) {
		this->allocator->free(active->next);
	}
	active->next = active;
	active->ref = &active->next;
	this->allocmngr.reset(this);
}

Pool *Pool::create(Allocator *alloc) {
	Allocator *allocator = alloc;
	if (allocator == nullptr) {
		allocator = new Allocator();
	}

	auto node = allocator->alloc(config::MIN_ALLOC - SIZEOF_MEMNODE);
	node->next = node;
	node->ref = &node->next;

	Pool *pool = new (node->first_avail) Pool(allocator, node);
	node->first_avail = pool->self_first_avail = (uint8_t *)pool + SIZEOF_POOL;

	if (!alloc) {
		allocator->owner = pool;
	}

	return pool;
}

void Pool::destroy(Pool *pool) {
	// SP_POOL_LOG("destroy %p %s", pool, pool->tag);
	pool->~Pool();
}

size_t Pool::getPoolsCount() { return s_nPools.load(); }

Pool::Pool() : allocmngr{this} { ++s_nPools; }

Pool::Pool(Allocator *alloc, MemNode *node)
: allocator(alloc), active(node), self(node), allocmngr{this} {
	++s_nPools;
}

Pool::Pool(Pool *p, Allocator *alloc, MemNode *node)
: allocator(alloc), active(node), self(node), allocmngr{this} {
	if ((parent = p) != nullptr) {
		std::unique_lock<Allocator> lock(*allocator);
		sibling = parent->child;
		if (sibling != nullptr) {
			sibling->ref = &sibling;
		}

		parent->child = this;
		ref = &parent->child;
	}
	++s_nPools;
}

Pool::~Pool() {
	Pool_performCleanup(this);

	memory::pool::popPoolInfo((memory::pool_t *)this);

	/* Remove the pool from the parents child list */
	if (this->parent) {
		std::unique_lock<Allocator> lock(*allocator);
		auto sib = this->sibling;
		*this->ref = this->sibling;
		if (sib != nullptr) {
			sib->ref = this->ref;
		}
	}

	Allocator *allocator = this->allocator;
	MemNode *active = this->self;
	*active->ref = NULL;

	allocator->free(active);
	if (allocator->owner == this) {
		delete allocator;
	}

	--s_nPools;
}


Pool *Pool::make_child() { return make_child(allocator); }

Pool *Pool::make_child(Allocator *allocator) {
	Pool *parent = this;
	if (allocator == nullptr) {
		allocator = parent->allocator;
	}

	MemNode *node;
	if ((node = allocator->alloc(config::MIN_ALLOC - SIZEOF_MEMNODE)) == nullptr) {
		return nullptr;
	}

	node->next = node;
	node->ref = &node->next;

	Pool *pool = new (node->first_avail) Pool(parent, allocator, node);
	node->first_avail = pool->self_first_avail = (uint8_t *)pool + SIZEOF_POOL;
	return pool;
}

void Pool::cleanup_register(const void *data, Cleanup::Callback cb) {
	Cleanup *c;

	if (free_cleanups) {
		/* reuse a cleanup structure */
		c = free_cleanups;
		free_cleanups = c->next;
	} else {
		c = (Cleanup *)palloc(sizeof(Cleanup));
	}

	c->data = data;
	c->fn = cb;
	c->next = cleanups;
	cleanups = c;
}

void Pool::pre_cleanup_register(const void *data, Cleanup::Callback cb) {
	Cleanup *c;

	if (free_cleanups) {
		/* reuse a cleanup structure */
		c = free_cleanups;
		free_cleanups = c->next;
	} else {
		c = (Cleanup *)palloc(sizeof(Cleanup));
	}
	c->data = data;
	c->fn = cb;
	c->next = pre_cleanups;
	pre_cleanups = c;
}

void Pool::cleanup_kill(void *data, Cleanup::Callback cb) {
	Cleanup *c, **lastp;

	c = cleanups;
	lastp = &cleanups;
	while (c) {
		if (c->data == data && c->fn == cb) {
			*lastp = c->next;
			/* move to freelist */
			c->next = free_cleanups;
			free_cleanups = c;
			break;
		}

		lastp = &c->next;
		c = c->next;
	}

	/* Remove any pre-cleanup as well */
	c = pre_cleanups;
	lastp = &pre_cleanups;
	while (c) {
		if (c->data == data && c->fn == cb) {
			*lastp = c->next;
			/* move to freelist */
			c->next = free_cleanups;
			free_cleanups = c;
			break;
		}

		lastp = &c->next;
		c = c->next;
	}
}

void Pool::cleanup_run(void *data, Cleanup::Callback cb) {
	cleanup_kill(data, cb);
	(*cb)(data);
}

Status Pool::userdata_set(const void *data, const char *key, Cleanup::Callback cleanup) {
	if (user_data == nullptr) {
		user_data = HashTable::make(this);
	}

	if (user_data->get(key, -1) == NULL) {
		char *new_key = pstrdup(key);
		user_data->set(new_key, -1, data);
	} else {
		user_data->set(key, -1, data);
	}

	if (cleanup) {
		cleanup_register(data, cleanup);
	}
	return Status::Ok;
}

Status Pool::userdata_setn(const void *data, const char *key, Cleanup::Callback cleanup) {
	if (user_data == nullptr) {
		user_data = HashTable::make(this);
	}

	user_data->set(key, -1, data);

	if (cleanup) {
		cleanup_register(data, cleanup);
	}
	return Status::Ok;
}

Status Pool::userdata_get(void **data, const char *key) {
	if (user_data == nullptr) {
		*data = nullptr;
	} else {
		*data = user_data->get(key, -1);
	}
	return Status::Ok;
}

Status Pool::userdata_get(void **data, const char *key, size_t klen) {
	if (user_data == nullptr) {
		*data = nullptr;
	} else {
		*data = user_data->get(key, klen);
	}
	return Status::Ok;
}

void initialize() {
	// We do not know, what thread calls this first!
	if (s_global_init.fetch_add(1) == 0) {
		if (!s_global_allocator) {
			s_global_allocator = new Allocator();
		}
		s_global_pool = Pool::create(s_global_allocator);
		s_global_pool->allocmngr.name = "Global";
	}
}

void terminate() {
	if (s_global_init.fetch_sub(1) == 1) {
		if (s_global_pool) {
			Pool::destroy(s_global_pool);
			s_global_pool = nullptr;
		}
		if (s_global_allocator) {
			delete s_global_allocator;
			s_global_allocator = nullptr;
		}
	}
}

Pool *create(Pool *p) {
	if (p) {
		return p->make_child();
	} else {
#if DEBUG
		assert(s_global_pool);
#endif
		return s_global_pool->make_child();
	}
}

void destroy(Pool *p) { Pool::destroy(p); }

} // namespace stappler::memory::custom
