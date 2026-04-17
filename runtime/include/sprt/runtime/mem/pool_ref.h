/**
Copyright (c) 2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_POOL_REF_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_POOL_REF_H_

#include <sprt/cxx/map>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/set>
#include <sprt/cxx/unordered_set>
#include <sprt/cxx/function>
#include <sprt/runtime/stream.h>
#include <sprt/runtime/ref.h>

namespace sprt {

class SPRT_API AllocRef : public Ref {
public:
	virtual ~AllocRef() { sprt::memory::allocator::destroy(_allocator); }

	AllocRef() { _allocator = sprt::memory::allocator::create(); }

	memory::allocator_t *getAllocator() const { return _allocator; }

	void setOwner(memory::pool_t *p) { sprt::memory::allocator::owner_set(_allocator, p); }
	memory::pool_t *getOwner() const { return sprt::memory::allocator::owner_get(_allocator); }

protected:
	memory::allocator_t *_allocator = nullptr;
};

class SPRT_API PoolRef : public Ref {
public:
	virtual ~PoolRef() {
		if (_ownsAllocator) {
			_allocator->setOwner(nullptr);
		}
		sprt::memory::pool::destroy(_pool);
		_pool = nullptr;
		_allocator = nullptr;
		_ownsAllocator = false;
	}

	PoolRef(AllocRef *alloc = nullptr) {
		_allocator = alloc;
		if (!_allocator) {
			_allocator = Rc<AllocRef>::alloc();
			_ownsAllocator = true;
		}
		_pool = sprt::memory::pool::create(_allocator->getAllocator());
		if (_ownsAllocator) {
			_allocator->setOwner(_pool);
		}
	}

	PoolRef(PoolRef *pool) {
		_ownsAllocator = false;
		_allocator = pool->_allocator;
		_pool = sprt::memory::pool::create(_allocator->getAllocator());
	}

	memory::pool_t *getPool() const { return _pool; }

	void *palloc(size_t size) { return sprt::memory::pool::palloc(_pool, size); }

	template <typename Callable>
	auto perform(const Callable &cb) {
		return memory::perform(cb, _pool);
	}

protected:
	Rc<AllocRef> _allocator;
	memory::pool_t *_pool = nullptr;
	bool _ownsAllocator = false;
};

class PoolObject : public sprt::detail::AllocPool {
public:
	template <typename T>
	using Allocator = sprt::detail::AllocatorPool<T>;

	using String = sprt::__pool_string;
	using WideString = sprt::__pool_u16string;
	using Bytes = sprt::__pool_vector<uint8_t>;

	template <typename Value>
	using BasicString = sprt::__pool_basic_string<Value>;

	template <typename Value>
	using Vector = sprt::__pool_vector<Value>;

	template <typename K, typename V, typename Compare = sprt::less<void>>
	using Map = sprt::__pool_map<K, V, Compare>;

	template <typename T, typename Compare = sprt::less<void>>
	using Set = sprt::__pool_set<T, Compare>;

	using StringStream = sprt::__pool_stringstream;

	template <typename T>
	using Function = sprt::__pool_function<T>;

	virtual ~PoolObject() = default;

	PoolObject(Ref *ref, memory::pool_t *p) : _ref(ref), _pool(p) { }

	Ref *getRef() const { return _ref; }

	memory::pool_t *getPool() const { return _pool; }

	template <typename Callback>
	auto perform(Callback &&cb) {
		return memory::perform(sprt::forward<Callback>(cb), _pool);
	}

protected:
	Ref *_ref = nullptr;
	memory::pool_t *_pool = nullptr;
};


} // namespace sprt

#endif
