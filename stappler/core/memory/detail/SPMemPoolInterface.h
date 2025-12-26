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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLINTERFACE_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLINTERFACE_H_

#include "SPMemPoolConfig.h"
#include "SPStatus.h"

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

namespace STAPPLER_VERSIONIZED stappler::memory {

// Hide pool implementation details from users
// Note that OpaquePool and OpaqueAllocator are never defined
class OpaquePool;
class OpaqueAllocator;

using pool_t = OpaquePool;
using allocator_t = OpaqueAllocator;

using cleanup_fn = Status (*)(void *);

// use when you need to create pool from application root pool
constexpr pool_t *app_root_pool = nullptr;

// Use this for a static init with memory::* types
SP_PUBLIC pool_t *get_zero_pool();

// Receives information about all memory pools on the stack via callback
SP_PUBLIC void foreach_info(void *userdata,
		bool (*)(void *userdata, pool_t *, uint32_t, const void *));

} // namespace stappler::memory


namespace STAPPLER_VERSIONIZED stappler::memory::allocator {

/*
	Creates an allocator for memory pools
	An allocator can be tied to a specific pool or exist separately.
	The user must ensure that the pool does not outlive its allocator.
	All allocators in stappler are thread-safe (but allocating memory from pools is not).
	That is, you can freely transfer allocators between threads.
*/
SP_PUBLIC allocator_t *create();


#if MODULE_STAPPLER_APR
/*
	Creates an allocator for Apache Portable Runtime, compatible with stappler functions.
	Technically, any APR allocator is compatible with stappler values ​​if the stappler_apr module is built
*/
SP_PUBLIC allocator_t *create_apr(void *mutex = nullptr);
#endif


SP_PUBLIC void owner_set(allocator_t *alloc, pool_t *pool);
SP_PUBLIC pool_t *owner_get(allocator_t *alloc);

SP_PUBLIC void max_free_set(allocator_t *alloc, size_t size);

SP_PUBLIC void destroy(allocator_t *);

} // namespace stappler::memory::allocator


namespace STAPPLER_VERSIONIZED stappler::memory::pool {

// Gets the memory pool from the current context
SP_PUBLIC pool_t *acquire();

// Gets the tag and additional pointer associated with the current context
SP_PUBLIC Pair<uint32_t, const void *> info();

// Adds a memory pool to the current context's stack
// These functions should not be used directly. Use context or perform*
// source - pointer to debugging information about the location of adding the memory pool
SP_PUBLIC void push(pool_t *, const char *source = SP_FUNC);

// Adds a memory pool to the current context's stack
// These functions should not be used directly. Use context or perform*
// tag - numeric tag for the context
// ptr - pointer to user data for the context
// source - pointer to debugging information about the location of adding the memory pool
SP_PUBLIC void push(pool_t *, uint32_t tag, const void *ptr = nullptr,
		const char *source = SP_FUNC);

// Removes the memory pool from the stack
// These functions should not be used directly. Use context or perform*
// A pointer to the memory pool is used to control the integrity of the stack; it must match the pool at the top of the stack
// source - pointer to debugging information about the location of adding the memory pool
SP_PUBLIC void pop(pool_t *, const char *source);

/*
	Initializes the memory pool subsystem
	The call is thread safe
	Every call to initialize must be balanced with terminate

	Before initialization, it is impossible to use memory pools other than independent ones and those with a dedicated allocator.
*/
SP_PUBLIC void initialize();
SP_PUBLIC void terminate();

/*
	Creates a memory pool associated with its own internal allocator.
	Such memory pools do not require prior stappler initialization
*/
SP_PUBLIC pool_t *create();

/*
	Creates a memory pool associated with its own internal allocator.
	Such memory pools do not require prior stappler initialization
*/
SP_PUBLIC pool_t *create_tagged(const char *);

/*
	Create a pool with a dedicated an allocator
	Such memory pools do not require prior stappler initialization
*/
SP_PUBLIC pool_t *create(allocator_t *);

/*
	Creates a standard memory pool inherited from the specified one
	Such a memory pool cannot live longer than the specified one
	To create a pool that inherits from the root memory pool, pass app_root_pool
*/
SP_PUBLIC pool_t *create(pool_t *);

/*
	Creates a standard memory pool inherited from the specified one
	Such a memory pool cannot live longer than the specified one
	To create a pool that inherits from the root memory pool, pass app_root_pool
*/
SP_PUBLIC pool_t *create_tagged(pool_t *, const char *);

#if MODULE_STAPPLER_APR
/*
	Creates an memory pool for Apache Portable Runtime, compatible with stappler functions.
	Technically, any APR pool is compatible with stappler values ​​if the stappler_apr module is built
*/
SP_PUBLIC pool_t *create_apr(allocator_t * = nullptr);

/*
	Creates an memory pool for Apache Portable Runtime, compatible with stappler functions.
	Technically, any APR pool is compatible with stappler values ​​if the stappler_apr module is built
*/
SP_PUBLIC pool_t *create_apr_tagged(const char *);
#endif

SP_PUBLIC void destroy(pool_t *);
SP_PUBLIC void clear(pool_t *);

/*
	Allocates memory from the memory pool
	size - input - the size of the memory block in bytes
	     - output - the actual allocated block size which may be larger than the required one
*/
SP_PUBLIC void *alloc(pool_t *, size_t &size);

/*
	Allocates memory from the memory pool
	size - input - the size of the memory block in bytes
	     - output - the actual allocated block size which may be larger than the required one
	alignment - memory alignment requirement (if less then DefaultAlignment - ignored)

	NOTE: APR pools cannot return aligned memory, so they return nullptr
*/
SP_PUBLIC void *alloc(pool_t *, size_t &size, uint32_t alignment);

/*
	Allocates memory from the memory pool
	size - the size of the memory block in bytes
*/
SP_PUBLIC void *palloc(pool_t *, size_t);

/*
	Allocates memory from the memory pool
	size - the size of the memory block in bytes
	alignment - memory alignment requirement (if less then DefaultAlignment - ignored)

	NOTE: APR pools cannot return aligned memory, so they return nullptr
*/
SP_PUBLIC void *palloc(pool_t *, size_t, uint32_t alignment);

SP_PUBLIC void *calloc(pool_t *, size_t count, size_t eltsize);

/*
	Returns a block of memory to be re-allocated if possible
	The dimension must be greater than BlockThreshold
*/
SP_PUBLIC void free(pool_t *, void *ptr, size_t size);

SP_PUBLIC void cleanup_kill(pool_t *, void *, cleanup_fn);
SP_PUBLIC void cleanup_register(pool_t *, void *, cleanup_fn);
SP_PUBLIC void cleanup_register(pool_t *p, function<void()> &&cb);
SP_PUBLIC void pre_cleanup_register(pool_t *, void *, cleanup_fn);
SP_PUBLIC void pre_cleanup_register(pool_t *p, function<void()> &&cb);

SP_PUBLIC Status userdata_set(const void *data, const char *key, cleanup_fn, pool_t *);
SP_PUBLIC Status userdata_setn(const void *data, const char *key, cleanup_fn, pool_t *);
SP_PUBLIC Status userdata_get(void **data, const char *key, pool_t *);
SP_PUBLIC Status userdata_get(void **data, const char *key, size_t, pool_t *);

SP_PUBLIC allocator_t *get_allocator(pool_t *);

SP_PUBLIC void *pmemdup(pool_t *a, const void *m, size_t n);
SP_PUBLIC char *pstrdup(pool_t *a, const char *s);

SP_PUBLIC const char *get_tag(pool_t *);

// debug counters
SP_PUBLIC size_t get_allocated_bytes(pool_t *);
SP_PUBLIC size_t get_return_bytes(pool_t *);
SP_PUBLIC size_t get_active_count();

// start recording additional pool info on creation
SP_PUBLIC bool debug_begin(pool_t *pool = nullptr);

// stop recording and return info
SP_PUBLIC std::map<pool_t *, const char **, std::less<void>> debug_end();

SP_PUBLIC void debug_foreach(void *, void (*)(void *, pool_t *));

} // namespace stappler::memory::pool


namespace STAPPLER_VERSIONIZED stappler::memory {

/*
	Class for adding a pool to the context stack in RAII style
	As long as an object of this type exists, the corresponding memory pool is added to the context stack
*/
template <typename _Pool = pool_t *>
class context {
public:
	using pool_type = _Pool;

	enum finalize_flag {
		discard, // do nothing
		conditional, // do not push pool if current context pool is the same
		clear, // clear pool after pop
		destroy, // destroy pool after pop
	};

	explicit context(const pool_type &__m, finalize_flag = discard, const char *source = SP_FUNC);

	context(const pool_type &__m, uint32_t tag, void *userdata, finalize_flag = discard,
			const char *source = SP_FUNC);
	~context();

	context(const context &) = delete;
	context &operator=(const context &) = delete;

	context(context &&u) noexcept;

	context &operator=(context &&u) noexcept;

	void push() noexcept;

	void push(uint32_t tag, void *userdata) noexcept;

	void pop() noexcept;

	void swap(context &u) noexcept;

	bool owns() const noexcept { return _owns; }

	operator pool_type() const noexcept { return _pool; }

private:
	pool_type _pool;
	bool _owns;
	finalize_flag _flag;
	const char *_source = nullptr;
};

using finalize_flag = context<pool_t *>::finalize_flag;

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	The standard 'perform' function uses the specified pool to work with
*/
template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	The standard 'perform' function uses the specified pool to work with
*/
template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	Conditional function does not add a pool to the stack if it is already on top of the stack
*/
template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	Conditional function does not add a pool to the stack if it is already on top of the stack
*/
template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, uint32_t tag,
		void *userdata = nullptr, const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_clear' function calls pool::clear after callback execution
*/
template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_clear' function calls pool::clear after callback execution
*/
template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_temporary' creates a temporary pool based on the one passed in, and deletes it after execution completes
*/
template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p = nullptr, const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_temporary' creates a temporary pool based on the one passed in, and deletes it after execution completes
*/
template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = SP_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_main' is intended to be called at the entry point into program execution.
	It correctly initializes and deinitializes all stappler systems,
*/
template <typename Callback>
inline int perform_main(int argc, const char *argv[], const Callback &cb);

} // namespace stappler::memory


//
// Implementation details
//

namespace STAPPLER_VERSIONIZED stappler::memory {

template <typename _Pool>
context<_Pool>::context(const pool_type &__m, finalize_flag f, const char *source)
: _pool(__m), _owns(false), _flag(f), _source(source) {
	push();
}

template <typename _Pool>
context<_Pool>::context(const pool_type &__m, uint32_t tag, void *userdata, finalize_flag f,
		const char *source)
: _pool(__m), _owns(false), _flag(f), _source(source) {
	push(tag, userdata);
}

template <typename _Pool>
context<_Pool>::~context() {
	if (!_owns) {
		return;
	}

	pop();
}

template <typename _Pool>
context<_Pool>::context(context &&u) noexcept : _pool(u._pool), _owns(u._owns), _flag(u._flag) {
	u._pool = 0;
	u._owns = false;
}

template <typename _Pool>
auto context<_Pool>::operator=(context &&u) noexcept -> context & {
	if (this == &u) {
		return *this;
	}

	if (_owns) {
		pop();
	}

	context(sp::move(u)).swap(*this);

	u._pool = 0;
	u._owns = false;
	return *this;
}

template <typename _Pool>
void context<_Pool>::push() noexcept {
	if (_pool && !_owns) {
		if (_flag != conditional || pool::acquire() != _pool) {
			pool::push(_pool, _source);
			_owns = true;
		}
	}
}

template <typename _Pool>
void context<_Pool>::push(uint32_t tag, void *userdata) noexcept {
	if (_pool && !_owns) {
		if (_flag != conditional || pool::acquire() != _pool) {
			pool::push(_pool, tag, userdata, _source);
			_owns = true;
		}
	}
}

template <typename _Pool>
void context<_Pool>::pop() noexcept {
	if (!_owns) {
		return;
	}

	pool::pop(_pool, _source);

	switch (_flag) {
	case discard:
	case conditional: break;
	case clear: pool::clear(_pool); break;
	case destroy:
		pool::destroy(_pool);
		_pool = nullptr;
		break;
	}

	_owns = false;
}

template <typename _Pool>
void context<_Pool>::swap(context &u) noexcept {
	std::swap(_pool, u._pool);
	std::swap(_owns, u._owns);
	std::swap(_flag, u._flag);
}

template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::discard, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, uint32_t tag, void *ptr, const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::discard, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::conditional, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::conditional, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::clear, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::clear, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, const char *source) {
	auto pool = pool::create(p ? p : pool::acquire());
	context<decltype(p)> holder(pool, context<decltype(p)>::destroy, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	auto pool = pool::create(p ? p : pool::acquire());
	context<decltype(p)> holder(pool, tag, ptr, context<decltype(p)>::destroy, source);
	if constexpr (std::is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(std::is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline int perform_main(int argc, const char *argv[], const Callback &cb) {
	int resultCode = 0;
	if (sp::initialize(argc, argv, resultCode)) {
		auto ret = cb();

		sp::terminate();
		return ret;
	} else {
		return resultCode;
	}
}

} // namespace stappler::memory


#endif /* STAPPLER_CORE_MEMORY_DETAIL_SPMEMPOOLINTERFACE_H_ */
