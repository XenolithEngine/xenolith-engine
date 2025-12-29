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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_CONTEXT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_CONTEXT_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/invoke.h>
#include <sprt/runtime/source_location.h>

namespace sprt::memory {

struct PoolCtxInfo {
	uint32_t tag;
	const void *ptr;
	const char *source;
};

// Receives information about all memory pools on the stack via callback
SPRT_API void foreach_info(void *userdata, bool (*)(void *userdata, pool_t *, const PoolCtxInfo &));

} // namespace sprt::memory

namespace sprt::memory::pool {

// Gets the memory pool from the current context
SPRT_API pool_t *acquire();

// Gets the tag and additional pointer associated with the current context
SPRT_API PoolCtxInfo info();

// Adds a memory pool to the current context's stack
// These functions should not be used directly. Use context or perform*
// source - pointer to debugging information about the location of adding the memory pool
SPRT_API void push(pool_t *, const char *source = __SPRT_FUNC);

// Adds a memory pool to the current context's stack
// These functions should not be used directly. Use context or perform*
// tag - numeric tag for the context
// ptr - pointer to user data for the context
// source - pointer to debugging information about the location of adding the memory pool
SPRT_API void push(pool_t *, PoolCtxInfo &&);

// Removes the memory pool from the stack
// These functions should not be used directly. Use context or perform*
// A pointer to the memory pool is used to control the integrity of the stack; it must match the pool at the top of the stack
// source - pointer to debugging information about the location of adding the memory pool
SPRT_API void pop(pool_t *, const char *source = __SPRT_FUNC);

} // namespace sprt::memory::pool


namespace sprt::memory {

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

	explicit context(const pool_type &__m, finalize_flag = discard,
			const char *source = __SPRT_FUNC);

	context(const pool_type &__m, uint32_t tag, void *userdata, finalize_flag = discard,
			const char *source = __SPRT_FUNC);
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
inline auto perform(const Callback &cb, pool_t *p, const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	The standard 'perform' function uses the specified pool to work with
*/
template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	Conditional function does not add a pool to the stack if it is already on top of the stack
*/
template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	Conditional function does not add a pool to the stack if it is already on top of the stack
*/
template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, uint32_t tag,
		void *userdata = nullptr, const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_clear' function calls pool::clear after callback execution
*/
template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_clear' function calls pool::clear after callback execution
*/
template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_temporary' creates a temporary pool based on the one passed in, and deletes it after execution completes
*/
template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p = nullptr, const char * = __SPRT_FUNC);

/*
	The 'perform' group functions allow you to execute a task in a context where a specific
	pool is on top of the stack. This is the recommended way to use memory pools.

	'perform_temporary' creates a temporary pool based on the one passed in, and deletes it after execution completes
*/
template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, uint32_t tag, void *userdata = nullptr,
		const char * = __SPRT_FUNC);

} // namespace sprt::memory


//
// Implementation details
//

namespace sprt::memory {

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

	context(sprt::move(u)).swap(*this);

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
	sprt::swap(_pool, u._pool);
	sprt::swap(_owns, u._owns);
	sprt::swap(_flag, u._flag);
}

template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::discard, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform(const Callback &cb, pool_t *p, uint32_t tag, void *ptr, const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::discard, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::conditional, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_conditional(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::conditional, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, const char *source) {
	context<decltype(p)> holder(p, context<decltype(p)>::clear, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_clear(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	context<decltype(p)> holder(p, tag, ptr, context<decltype(p)>::clear, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, const char *source) {
	auto pool = pool::create(p ? p : pool::acquire());
	context<decltype(p)> holder(pool, context<decltype(p)>::destroy, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

template <typename Callback>
inline auto perform_temporary(const Callback &cb, pool_t *p, uint32_t tag, void *ptr,
		const char *source) {
	auto pool = pool::create(p ? p : pool::acquire());
	context<decltype(p)> holder(pool, tag, ptr, context<decltype(p)>::destroy, source);
	if constexpr (is_invocable_v<Callback, pool_t *>) {
		return cb(p);
	} else {
		static_assert(is_invocable_v<Callback>, "Callback should receive pool_t * or nothing");
		return cb();
	}
}

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_CONTEXT_H_
