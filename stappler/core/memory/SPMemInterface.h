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

#ifndef STAPPLER_CORE_MEMORY_SPMEMINTERFACE_H_
#define STAPPLER_CORE_MEMORY_SPMEMINTERFACE_H_

#include <sprt/cxx/function>
#include <sprt/cxx/forward_list>
#include <sprt/cxx/set>
#include <sprt/cxx/map>
#include <sprt/cxx/string>
#include <sprt/cxx/vector>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/stream.h>

#include "SPCore.h"

namespace STAPPLER_VERSIONIZED stappler::memory {

using sprt::memory::allocator_t;
using sprt::memory::pool_t;
using sprt::detail::AllocPool;

using sprt::memory::perform;
using sprt::memory::perform_conditional;
using sprt::memory::perform_clear;
using sprt::memory::perform_temporary;

using sprt::memory::context;
using sprt::callback;

} // namespace stappler::memory

namespace STAPPLER_VERSIONIZED stappler::memory::allocator {

using sprt::memory::allocator::create;
using sprt::memory::allocator::owner_set;
using sprt::memory::allocator::owner_get;
using sprt::memory::allocator::max_free_set;
using sprt::memory::allocator::destroy;

} // namespace stappler::memory::allocator

namespace STAPPLER_VERSIONIZED stappler::memory::pool {

using sprt::memory::pool::acquire;
using sprt::memory::pool::create;
using sprt::memory::pool::create_tagged;
using sprt::memory::pool::destroy;
using sprt::memory::pool::clear;
using sprt::memory::pool::cleanup_flags;
using sprt::memory::pool::cleanup_register;
using sprt::memory::pool::pre_cleanup_register;
using sprt::memory::pool::alloc;
using sprt::memory::pool::palloc;
using sprt::memory::pool::calloc;
using sprt::memory::pool::free;
using sprt::memory::pool::userdata_get;
using sprt::memory::pool::userdata_set;

} // namespace stappler::memory::pool

namespace STAPPLER_VERSIONIZED stappler::memory {

struct SP_PUBLIC PoolInterface final {
	using AllocBaseType = sprt::detail::AllocPool;

	template <typename T>
	using Allocator = sprt::detail::AllocatorPool<T>;

	using StringType = sprt::__pool_string;
	using WideStringType = sprt::__pool_u16string;
	using BytesType = sprt::__pool_vector<uint8_t>;

	template <typename Value>
	using BasicStringType = sprt::__pool_basic_string<Value>;
	template <typename Value>
	using ArrayType = sprt::__pool_vector<Value>;
	template <typename Value>
	using DictionaryType = sprt::__pool_map<StringType, Value, sprt::less<void>>;
	template <typename Value>
	using VectorType = sprt::__pool_vector<Value>;

	template <typename K, typename V, typename Compare = sprt::less<void>>
	using MapType = sprt::__pool_map<K, V, Compare>;

	template <typename T, typename Compare = sprt::less<void>>
	using SetType = sprt::__pool_set<T, Compare>;

	using StringStreamType = sprt::__pool_stringstream;

	template <typename T>
	using FunctionType = sprt::__pool_function<T>;

	static constexpr bool UsesMemoryPool = true;
};

struct SP_PUBLIC StandartInterface final {
	using AllocBaseType = sprt::AllocBase;

	template <typename T>
	using Allocator = sprt::detail::AllocatorMalloc<T>;

	using StringType = sprt::__malloc_string;
	using WideStringType = sprt::__malloc_u16string;
	using BytesType = sprt::__malloc_vector<uint8_t>;

	template <typename Value>
	using BasicStringType = sprt::__malloc_basic_string<Value>;
	template <typename Value>
	using ArrayType = sprt::__malloc_vector<Value>;
	template <typename Value>
	using DictionaryType = sprt::__malloc_map<StringType, Value, sprt::less<void>>;
	template <typename Value>
	using VectorType = sprt::__malloc_vector<Value>;

	template <typename K, typename V, typename Compare = sprt::less<void>>
	using MapType = sprt::__malloc_map<K, V, Compare>;

	template <typename T, typename Compare = sprt::less<void>>
	using SetType = sprt::__malloc_set<T, Compare>;

	using StringStreamType = sprt::__malloc_stringstream;

	template <typename T>
	using FunctionType = sprt::__malloc_function<T>;

	static constexpr bool UsesMemoryPool = false;
};

} // namespace stappler::memory

namespace STAPPLER_VERSIONIZED stappler {

template <typename InterfaceType>
struct InterfaceObject {
	using Interface = InterfaceType;

	using AllocBase = typename Interface::AllocBaseType;

	using String = typename Interface::StringType;
	using WideString = typename Interface::WideStringType;
	using Bytes = typename Interface::BytesType;

	template <typename Value>
	using BasicString = typename Interface::template BasicStringType<Value>;

	template <typename Value>
	using Vector = typename Interface::template VectorType<Value>;

	template <typename K, typename V, typename Compare = sprt::less<void>>
	using Map = typename Interface::template MapType<K, V, Compare>;

	template <typename T, typename Compare = sprt::less<void>>
	using Set = typename Interface::template SetType<T, Compare>;

	using StringStream = typename Interface::StringStreamType;

	template <typename T>
	using Function = typename Interface::template FunctionType<T>;
};

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::memory {

class PoolObject : public memory::PoolInterface::AllocBaseType,
				   public InterfaceObject<memory::PoolInterface> {
public:
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

} // namespace stappler::memory

namespace STAPPLER_VERSIONIZED stappler {

template <typename T>
auto StringToNumber(const memory::StandartInterface::StringType &str) -> T {
	return StringToNumber<T>(str.data(), nullptr, 0);
}

template <typename T>
auto StringToNumber(const memory::PoolInterface::StringType &str) -> T {
	return StringToNumber<T>(str.data(), nullptr, 0);
}

template <typename T>
auto StringToNumber(const char *str) -> T {
	return StringToNumber<T>(str, nullptr, 0);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_MEMORY_SPMEMINTERFACE_H_ */
