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

#include "private/SPRTMemStruct.h"
#include <sprt/runtime/mem/context.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/new.h>
#include <sprt/c/__sprt_assert.h>
#include <sprt/runtime/mem/detail/alloc.h>

namespace sprt::memory {

static apr::AprInterface s_aprInterface;

bool is_apr_available() { return s_aprInterface ? true : false; }

} // namespace sprt::memory


namespace sprt::memory::apr::allocator {

static apr_allocator_t *create() {
	if (s_aprInterface) {
		apr_allocator_t *ret = nullptr;
		s_aprInterface.apr_allocator_create(&ret);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static apr_allocator_t *create(void *mutex) {
	if (s_aprInterface) {
		if (!mutex) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
					"Invalid apr_mutex_t for APR allocator");
			return nullptr;
		}
		apr_allocator_t *ret = nullptr;
		s_aprInterface.apr_allocator_create(&ret);
		s_aprInterface.apr_allocator_mutex_set(ret, (apr_thread_mutex_t *)mutex);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static void destroy(apr_allocator_t *alloc) {
	if (s_aprInterface) {
		s_aprInterface.apr_allocator_destroy(alloc);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void owner_set(apr_allocator_t *alloc, apr_pool_t *pool) {
	if (s_aprInterface) {
		s_aprInterface.apr_allocator_owner_set(alloc, pool);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}
static apr_pool_t *owner_get(apr_allocator_t *alloc) {
	if (s_aprInterface) {
		return s_aprInterface.apr_allocator_owner_get(alloc);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return nullptr;
	}
}
static void max_free_set(apr_allocator_t *alloc, size_t size) {
	if (s_aprInterface) {
		s_aprInterface.apr_allocator_max_free_set(alloc, size);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

} // namespace sprt::memory::apr::allocator


namespace sprt::memory::apr::pool {

struct wrapper_pool_t {
	apr_pool_t *parent;
	apr_pool_t *child;
	apr_pool_t *sibling;
	apr_pool_t **ref;
	void *cleanups;
	void *free_cleanups;
	apr_allocator_t *allocator;
	struct process_chain *subprocesses;
	apr_abortfunc_t abort_fn;
	void *user_data;
	const char *tag;
};

static impl::AllocManager *allocmngr_get(apr_pool_t *pool);

static void initialize() {
	if (s_aprInterface) {
		s_aprInterface.apr_pool_initialize();
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void terminate() {
	if (s_aprInterface) {
		s_aprInterface.apr_pool_terminate();
	}
}

static apr_pool_t *create() {
	if (s_aprInterface) {
		apr_pool_t *ret = nullptr;
		s_aprInterface.apr_pool_create_unmanaged_ex(&ret, nullptr, nullptr);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static apr_pool_t *create(apr_allocator_t *alloc) {
	if (s_aprInterface) {
		apr_pool_t *ret = nullptr;
		s_aprInterface.apr_pool_create_unmanaged_ex(&ret, nullptr, alloc);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static apr_pool_t *create(apr_pool_t *p) {
	if (s_aprInterface) {
		apr_pool_t *ret = nullptr;
		if (!p) {
			s_aprInterface.apr_pool_create_ex(&ret, nullptr, nullptr, nullptr);
		} else {
			s_aprInterface.apr_pool_create_ex(&ret, p, nullptr, nullptr);
		}
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static apr_pool_t *createTagged(const char *tag) {
	if (s_aprInterface) {
		auto ret = create();
		s_aprInterface.apr_pool_tag(ret, tag);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static apr_pool_t *createTagged(apr_pool_t *p, const char *tag) {
	if (s_aprInterface) {
		auto ret = create(p);
		s_aprInterface.apr_pool_tag(ret, tag);
		return ret;
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static void destroy(apr_pool_t *p) {
	if (s_aprInterface) {
		s_aprInterface.apr_pool_destroy(p);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void clear(apr_pool_t *p) {
	if (s_aprInterface) {
		s_aprInterface.apr_pool_clear(p);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void *alloc(apr_pool_t *p, size_t &size) {
	if (s_aprInterface) {
		if (auto mngr = allocmngr_get(p)) {
			if (size >= config::BlockThreshold) {
				return mngr->alloc(size, config::DefaultAlignment,
						[](void *p, size_t s, uint32_t a) {
					return s_aprInterface.apr_palloc((apr_pool_t *)p, s);
				});
			} else {
				mngr->increment_alloc(size);
			}
		}
		return s_aprInterface.apr_palloc(p, size);
	}
	log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
			"APR interface is not available");
	return nullptr;
}

static void free(apr_pool_t *p, void *ptr, size_t size) {
	if (s_aprInterface) {
		if (size >= config::BlockThreshold) {
			if (auto m = allocmngr_get(p)) {
				return m->free(ptr, size, [](void *p, size_t s, uint32_t a) {
					return s_aprInterface.apr_palloc((apr_pool_t *)p, s);
				});
			}
		}
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void *palloc(apr_pool_t *p, size_t size) { return pool::alloc(p, size); }

static void *calloc(apr_pool_t *p, size_t count, size_t eltsize) {
	size_t s = count * eltsize;
	auto ptr = pool::alloc(p, s);
	if (ptr) {
		__builtin_memset(ptr, 0, s);
	}
	return ptr;
}

static void cleanup_kill(apr_pool_t *p, void *ptr, apr_status_t (*cb)(void *)) {
	if (s_aprInterface) {
		s_aprInterface.apr_pool_cleanup_kill(p, ptr, cb);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

struct __CleaupData {
	void *data;
	apr_pool_t *pool;
	apr_status_t (*callback)(void *);

	static apr_status_t doCleanup(void *data) {
		if (auto d = (__CleaupData *)data) {
			perform_conditional([&] { d->callback(d->data); }, (pool_t *)d->pool);
		}
		return 0;
	}
};

static void cleanup_register(apr_pool_t *p, void *ptr, apr_status_t (*cb)(void *)) {
	if (s_aprInterface) {
		auto data = (__CleaupData *)s_aprInterface.apr_palloc(p, sizeof(__CleaupData));
		data->data = ptr;
		data->pool = p;
		data->callback = cb;
		s_aprInterface.apr_pool_cleanup_register(p, data, &__CleaupData::doCleanup,
				s_aprInterface.apr_pool_cleanup_null);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static void pre_cleanup_register(apr_pool_t *p, void *ptr, apr_status_t (*cb)(void *)) {
	if (s_aprInterface) {
		auto data = (__CleaupData *)s_aprInterface.apr_palloc(p, sizeof(__CleaupData));
		data->data = ptr;
		data->pool = p;
		data->callback = cb;
		s_aprInterface.apr_pool_pre_cleanup_register(p, data, &__CleaupData::doCleanup);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
	}
}

static apr_status_t userdata_set(const void *data, const char *key, cleanup_fn cb,
		apr_pool_t *pool) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pool_userdata_set(data, key,
				reinterpret_cast<apr_status_t (*)(void *)>(cb), pool);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return -1;
	}
}

static apr_status_t userdata_setn(const void *data, const char *key, cleanup_fn cb,
		apr_pool_t *pool) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pool_userdata_setn(data, key,
				reinterpret_cast<apr_status_t (*)(void *)>(cb), pool);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return -1;
	}
}

static apr_status_t userdata_get(void **data, const char *key, apr_pool_t *pool) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pool_userdata_get(data, key, pool);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return -1;
	}
}

static size_t get_allocated_bytes(apr_pool_t *p) { return allocmngr_get(p)->get_alloc(); }
static size_t get_return_bytes(apr_pool_t *p) { return allocmngr_get(p)->get_return(); }

static apr_allocator_t *get_allocator(apr_pool_t *p) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pool_allocator_get(p);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return nullptr;
	}
}

static void *pmemdup(apr_pool_t *a, const void *m, size_t n) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pmemdup(a, m, n);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return nullptr;
	}
}
static char *pstrdup(apr_pool_t *a, const char *s) {
	if (s_aprInterface) {
		return s_aprInterface.apr_pstrdup(a, s);
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return nullptr;
	}
}

static void set_pool_info(apr_pool_t *p, uint32_t tag, const void *ptr) {
	if (auto mngr = allocmngr_get(p)) {
		if (tag > mngr->tag) {
			mngr->tag = tag;
		}
		mngr->ptr = ptr;
	}
}

static impl::AllocManager *allocmngr_get(apr_pool_t *pool) {
	if (!pool) {
		return nullptr;
	}

	if (s_aprInterface) {
		wrapper_pool_t *p = (wrapper_pool_t *)pool;
		if (p->tag) {
			auto m = (impl::AllocManager *)p->tag;
			if (m->pool == pool) {
				return m;
			}
		}

		auto mem = s_aprInterface.apr_palloc(pool, sizeof(impl::AllocManager));
		auto m = new (mem) impl::AllocManager;
		m->pool = pool;
		m->name = p->tag;

		pre_cleanup_register(pool, m, [](void *ptr) {
			auto m = (impl::AllocManager *)ptr;

			wrapper_pool_t *p = (wrapper_pool_t *)m->pool;
			p->tag = m->name;
			return 0;
		});

		p->tag = (const char *)m;
		return m;
	} else {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
				"APR interface is not available");
		return nullptr;
	}
}

static const char *get_tag(apr_pool_t *pool) {
	wrapper_pool_t *p = (wrapper_pool_t *)pool;
	if (p->tag) {
		auto m = (impl::AllocManager *)p->tag;
		if (m->pool == pool) {
			return m->name;
		}
		return p->tag;
	}
	return nullptr;
}

} // namespace sprt::memory::apr::pool


namespace sprt::memory {

SPRT_UNUSED static inline bool isStappler(allocator_t *alloc) {
	if constexpr (config::AprCompatible) {
		if (!alloc || *((uintptr_t *)alloc) == static_cast<uintptr_t>(config::POOL_MAGIC)) {
			return true;
		} else {
			return false;
		}
	}
	return true;
}

SPRT_UNUSED static inline bool isStappler(pool_t *p) {
	if constexpr (config::AprCompatible) {
		if (!p || ((impl::Pool *)p)->magic == static_cast<uintptr_t>(config::POOL_MAGIC)) {
			return true;
		} else {
			return false;
		}
	}
	return true;
}

} // namespace sprt::memory

namespace sprt::memory::allocator {

allocator_t *create() { return (allocator_t *)(new (sprt::nothrow) impl::Allocator()); }

allocator_t *create_apr(void *mutex) {
	if constexpr (config::AprCompatible) {
		if (mutex) {
			return (allocator_t *)apr::allocator::create(mutex);
		} else {
			return (allocator_t *)apr::allocator::create();
		}
	}
	return nullptr;
}

void destroy(allocator_t *alloc) {
	if constexpr (config::AprCompatible) {
		if (isStappler(alloc)) {
			delete (impl::Allocator *)alloc;
		} else {
			apr::allocator::destroy((apr_allocator_t *)alloc);
		}
	} else {
		delete (impl::Allocator *)alloc;
	}
}

void owner_set(allocator_t *alloc, pool_t *pool) {
	if constexpr (config::AprCompatible) {
		if (isStappler(alloc)) {
			if (isStappler(pool)) {
				((impl::Allocator *)alloc)->owner = (impl::Pool *)pool;
			} else {
				log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::memory",
						"Mixing of sprt and APR is forbidden");
			}
		} else {
			apr::allocator::owner_set((apr_allocator_t *)alloc, (apr_pool_t *)pool);
		}
	} else {
		((impl::Allocator *)alloc)->owner = (impl::Pool *)pool;
	}
}

pool_t *owner_get(allocator_t *alloc) {
	if constexpr (config::AprCompatible) {
		if (!isStappler(alloc)) {
			return (pool_t *)apr::allocator::owner_get((apr_allocator_t *)alloc);
		}
	}
	return (pool_t *)((impl::Allocator *)alloc)->owner;
}

void max_free_set(allocator_t *alloc, size_t size) {
	if constexpr (config::AprCompatible) {
		if (isStappler(alloc)) {
			((impl::Allocator *)alloc)->set_max(size);
		} else {
			apr::allocator::max_free_set((apr_allocator_t *)alloc, size);
		}
	} else {
		((impl::Allocator *)alloc)->set_max(size);
	}
}

} // namespace sprt::memory::allocator

namespace sprt::memory::pool {

static atomic<size_t> s_activePools = 0;
static atomic<bool> s_poolDebug = 0;
static qmutex s_poolDebugMutex;

struct debug_bt_info {
	pool_t *pool;
	const char **target;
	size_t index;
};

static pool_t *pushPoolInfo(pool_t *pool) {
	if (pool) {
		++s_activePools;
		if (s_poolDebug.load()) {
			/*if (auto ret = getPoolInfo(pool)) {
				s_poolDebugMutex.lock();
				s_poolDebugInfo.emplace(pool, ret);
				s_poolDebugMutex.unlock();
			}*/
		}
#if DEBUG_POOL_LIST
		if (isCustom(pool)) {
			s_poolDebugMutex.lock();
			s_poolList.emplace_back(pool);
			s_poolDebugMutex.unlock();
		}
#endif
	}
	return pool;
}

static void popPoolInfo(pool_t *pool) {
	if (pool) {
		if (s_poolDebug.load()) {
			s_poolDebugMutex.lock();
			//s_poolDebugInfo.erase(pool);
			s_poolDebugMutex.unlock();
		}
#if DEBUG_POOL_LIST
		if (isCustom(pool)) {
			s_poolDebugMutex.lock();
			auto it = std::find(s_poolList.begin(), s_poolList.end(), pool);
			if (it != s_poolList.end()) {
				s_poolList.erase(it);
			}
			s_poolDebugMutex.unlock();
		}
#endif
		--s_activePools;
	}
}

void initialize() {
	auto self = Dso(StringView(), DsoFlags::Self);

	s_aprInterface.load(sprt::move(self));

	if constexpr (config::AprCompatible) {
		apr::pool::initialize();
	}
	impl::initialize();
}

void terminate() {
	if constexpr (config::AprCompatible) {
		apr::pool::terminate();
	}
	impl::terminate();
}

pool_t *create() { return pushPoolInfo((pool_t *)impl::Pool::create(nullptr)); }

pool_t *create(allocator_t *alloc) {
	if constexpr (config::AprCompatible) {
		if (isStappler(alloc)) {
			return pushPoolInfo((pool_t *)impl::Pool::create((impl::Allocator *)alloc));
		} else {
			return pushPoolInfo((pool_t *)apr::pool::create((apr_allocator_t *)alloc));
		}
	}
	return pushPoolInfo((pool_t *)impl::Pool::create((impl::Allocator *)alloc));
}

// creates managed pool (managed by root, if parent in mullptr)
pool_t *create(pool_t *pool) {
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return pushPoolInfo((pool_t *)apr::pool::create((apr_pool_t *)pool));
		}
	}
	return pushPoolInfo((pool_t *)impl::create((impl::Pool *)pool));
}

// creates unmanaged pool
pool_t *create_tagged(const char *tag) {
	if (auto ret = impl::Pool::create(nullptr)) {
		ret->allocmngr.name = tag;
		return pushPoolInfo((pool_t *)ret);
	}
	return nullptr;
}

pool_t *create_tagged(pool_t *p, const char *tag) {
	if constexpr (config::AprCompatible) {
		if (!isStappler(p)) {
			return pushPoolInfo((pool_t *)apr::pool::createTagged((apr_pool_t *)p, tag));
		}
	}
	if (auto ret = impl::create((impl::Pool *)p)) {
		ret->allocmngr.name = tag;
		return pushPoolInfo((pool_t *)ret);
	}
	return nullptr;
}

pool_t *create_apr(allocator_t *alloc) {
	if (alloc) {
		return pushPoolInfo((pool_t *)apr::pool::create((apr_allocator_t *)alloc));
	} else {
		return pushPoolInfo((pool_t *)apr::pool::create());
	}
}

pool_t *create_apr_tagged(const char *tag) {
	return pushPoolInfo((pool_t *)apr::pool::createTagged(tag));
}

void destroy(pool_t *p) {
#if DEBUG
	// Clearing or destruction of a pool, that currently on stack, is an error,
	// That can not be tracked another way
	foreach_info(p, [](void *ptr, pool_t *p, const PoolCtxInfo &info) {
		if (ptr == p) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "memory",
					"pool::destroy was called on pool, that currently on stack/in use; pushed on: ",
					info.source);
			__sprt_abort();
		}
		return true;
	});
#endif

	popPoolInfo(p);
	if constexpr (config::AprCompatible) {
		if (!isStappler(p)) {
			apr::pool::destroy((apr_pool_t *)p);
		} else {
			impl::destroy((impl::Pool *)p);
		}
	} else {
		impl::destroy((impl::Pool *)p);
	}
}

void clear(pool_t *p) {
	sprt_passert(p, "Memory pool can not be NULL");
#if DEBUG
	// Clearing or destruction of a pool, that currently on stack, is an error,
	// That can not be tracked another way
	foreach_info(p, [](void *ptr, pool_t *p, const PoolCtxInfo &info) {
		if (ptr == p) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "memory",
					"pool::destroy was called on pool, that currently on stack/in use; pushed on: ",
					info.source);
			__sprt_abort();
		}
		return true;
	});
#endif

	if constexpr (config::AprCompatible) {
		if (!isStappler(p)) {
			apr::pool::clear((apr_pool_t *)p);
		} else {
			((impl::Pool *)p)->clear();
		}
	} else {
		((impl::Pool *)p)->clear();
	}
}

void *alloc(pool_t *pool, size_t &size) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::alloc((apr_pool_t *)pool, size);
		}
	}
	return ((impl::Pool *)pool)->alloc(size);
}

void *alloc(pool_t *pool, size_t &size, uint32_t alignment) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "memory",
					"APR pool can not allocate aligned mem");
			return nullptr;
		}
	}
	return ((impl::Pool *)pool)->alloc(size, alignment);
}

void *palloc(pool_t *pool, size_t size) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::palloc((apr_pool_t *)pool, size);
		}
	}
	return ((impl::Pool *)pool)->palloc(size);
}

void *palloc(pool_t *pool, size_t size, uint32_t alignment) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "memory",
					"APR pool can not allocate aligned mem");
			return nullptr;
		}
	}
	return ((impl::Pool *)pool)->palloc(size, alignment);
}

void *calloc(pool_t *pool, size_t count, size_t eltsize) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::calloc((apr_pool_t *)pool, count, eltsize);
		}
	}
	return ((impl::Pool *)pool)->calloc(count, eltsize);
}

void free(pool_t *pool, void *ptr, size_t size) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			apr::pool::free((apr_pool_t *)pool, ptr, size);
			return;
		}
	}
	((impl::Pool *)pool)->free(ptr, size);
}

void cleanup_kill(pool_t *pool, void *ptr, cleanup_fn cb) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			apr::pool::cleanup_kill((apr_pool_t *)pool, ptr, (apr_status_t (*)(void *))cb);
			return;
		}
	}
	((impl::Pool *)pool)->cleanup_kill(ptr, (impl::Cleanup::Callback)cb);
}

void cleanup_register(pool_t *pool, void *ptr, cleanup_fn cb) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			apr::pool::cleanup_register((apr_pool_t *)pool, ptr, (apr_status_t (*)(void *))cb);
			return;
		}
	}
	((impl::Pool *)pool)->cleanup_register(ptr, (impl::Cleanup::Callback)cb);
}

void pre_cleanup_register(pool_t *pool, void *ptr, cleanup_fn cb) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			apr::pool::pre_cleanup_register((apr_pool_t *)pool, ptr, (apr_status_t (*)(void *))cb);
			return;
		}
	}
	((impl::Pool *)pool)->pre_cleanup_register(ptr, (impl::Cleanup::Callback)cb);
}

Status userdata_set(const void *data, const char *key, cleanup_fn cb, pool_t *pool) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return Status(apr::pool::userdata_set(data, key, cb, (apr_pool_t *)pool));
		}
	}
	return ((impl::Pool *)pool)->userdata_set(data, key, (impl::Cleanup::Callback)cb);
}

Status userdata_setn(const void *data, const char *key, cleanup_fn cb, pool_t *pool) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return Status(apr::pool::userdata_setn(data, key, cb, (apr_pool_t *)pool));
		}
	}
	return ((impl::Pool *)pool)->userdata_setn(data, key, (impl::Cleanup::Callback)cb);
}

Status userdata_get(void **data, const char *key, pool_t *pool) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return Status(apr::pool::userdata_get(data, key, (apr_pool_t *)pool));
		}
	}
	return ((impl::Pool *)pool)->userdata_get(data, key);
}

Status userdata_get(void **data, const char *key, size_t klen, pool_t *pool) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			if (key[klen]) {
				return Status(apr::pool::userdata_get(data, key, (apr_pool_t *)pool));
			} else {
				char buf[klen + 1];
				__builtin_memcpy(buf, key, klen);
				buf[klen] = 0;
				return Status(apr::pool::userdata_get(data, key, (apr_pool_t *)pool));
			}
		}
	}
	return ((impl::Pool *)pool)->userdata_get(data, key, klen);
}

// debug counters
size_t get_allocated_bytes(pool_t *pool) {
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::get_allocated_bytes((apr_pool_t *)pool);
		}
	}
	return ((impl::Pool *)pool)->allocmngr.allocated;
}

size_t get_return_bytes(pool_t *pool) {
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::get_return_bytes((apr_pool_t *)pool);
		}
	}
	return ((impl::Pool *)pool)->allocmngr.returned;
}

allocator_t *get_allocator(pool_t *pool) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return (allocator_t *)apr::pool::get_allocator((apr_pool_t *)pool);
		}
	}
	return (allocator_t *)(((impl::Pool *)pool)->allocator);
}

void *pmemdup(pool_t *pool, const void *m, size_t n) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::pmemdup((apr_pool_t *)pool, m, n);
		}
	}
	return ((impl::Pool *)pool)->pmemdup(m, n);
}

char *pstrdup(pool_t *pool, const char *s) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			return apr::pool::pstrdup((apr_pool_t *)pool, s);
		}
	}
	return ((impl::Pool *)pool)->pstrdup(s);
}

const char *get_tag(pool_t *pool) {
	if constexpr (config::AprCompatible) {
		if (isStappler(pool)) {
			return ((impl::Pool *)pool)->allocmngr.name;
		} else {
			return apr::pool::get_tag((apr_pool_t *)pool);
		}
	} else {
		return ((impl::Pool *)pool)->allocmngr.name;
	}
}

void set_pool_info(pool_t *pool, uint32_t tag, const void *ptr) {
	sprt_passert(pool, "Memory pool can not be NULL");
	if constexpr (config::AprCompatible) {
		if (!isStappler(pool)) {
			apr::pool::set_pool_info((apr_pool_t *)pool, tag, ptr);
			return;
		}
	}

	if (auto mngr = &((impl::Pool *)pool)->allocmngr) {
		if (tag > mngr->tag) {
			mngr->tag = tag;
		}
		mngr->ptr = ptr;
	}
}

size_t get_active_count() { return s_activePools.load(); }

} // namespace sprt::memory::pool

namespace sprt::memory {

bool AllocPool::isStapplerPool(pool_t *p) {
	if (p && ((impl::Pool *)p)->magic == static_cast<uintptr_t>(config::POOL_MAGIC)) {
		return true;
	} else {
		return false;
	}
}

} // namespace sprt::memory
