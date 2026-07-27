/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_SPMEMORY_H_
#define STAPPLER_CORE_SPMEMORY_H_

#include "SPCore.h"
#include "SPString.h" // IWYU pragma: keep

#ifdef MODULE_STAPPLER_DATA
#include "SPData.h" // IWYU pragma: keep
#endif

#include <sprt/runtime/detail/emplace_ordered.h>

namespace STAPPLER_VERSIONIZED stappler {

/** VectorAdapter<Type> - унифицированный адаптер для доступа к типу Vector
 * вне зависимости от интерфейса памяти
 *
 * Адаптер захватывает Vector<Type> и использует его для чтения и записи
 */

template <typename T>
class SP_PUBLIC VectorAdapter {
public:
	size_t size() const { return size_fn(target); }
	T &back() const { return back_fn(target); }
	T &front() const { return front_fn(target); }
	bool empty() const { return empty_fn(target); }
	T &at(size_t pos) const { return at_fn(target, pos); }
	T &emplace_back(T &&v) const { return emplace_back_fn(target, move(v)); }
	void insert(size_t pos, T &&v) const { insert_fn(target, pos, move(v)); }

	T *begin() const { return begin_fn(target); }
	T *end() const { return end_fn(target); }

	void clear() const { clear_fn(target); }
	void reserve(size_t count) const { reserve_fn(target, count); }
	void resize(size_t count) const { resize_fn(target, count); }

	explicit operator bool() const noexcept { return target != nullptr; }

	VectorAdapter() noexcept = default;

	VectorAdapter(memory::StandartInterface::VectorType<T> &vec) noexcept;
	VectorAdapter(memory::PoolInterface::VectorType<T> &vec) noexcept;

public:
	void *target = nullptr;
	size_t (*size_fn)(void *) = nullptr;
	T &(*back_fn)(void *) = nullptr;
	T &(*front_fn)(void *) = nullptr;
	bool (*empty_fn)(void *) = nullptr;
	T &(*at_fn)(void *, size_t) = nullptr;
	T &(*emplace_back_fn)(void *, T &&) = nullptr;
	void (*insert_fn)(void *, size_t, T &&) = nullptr;
	T *(*begin_fn)(void *) = nullptr;
	T *(*end_fn)(void *) = nullptr;
	void (*clear_fn)(void *) = nullptr;
	void (*reserve_fn)(void *, size_t) = nullptr;
	void (*resize_fn)(void *, size_t) = nullptr;
};

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::mem_pool {

namespace pool = sprt::memory::pool;
namespace allocator = sprt::memory::allocator;

using CharGroupId = stappler::CharGroupId;

using memory::allocator_t;
using memory::pool_t;

using stappler::Time;
using stappler::TimeInterval;

using stappler::StringView;
using stappler::StringViewUtf8;
using stappler::WideStringView;
using stappler::BytesView;
using stappler::SpanView;

using AllocBase = stappler::memory::AllocPool;

template <typename T>
using Allocator = sprt::detail::AllocatorPool<T>;

using String = sprt::__pool_string;
using WideString = sprt::__pool_u16string;
using Bytes = sprt::__pool_vector<uint8_t>;

template <typename T>
using Vector = sprt::__pool_vector<T>;

template <typename T>
using List = sprt::__pool_list<T>;

template <typename K, typename V, typename Compare = sprt::less<void>>
using Map = sprt::__pool_map<K, V, Compare>;

template <typename K, typename V>
using HashMap = sprt::__pool_unordered_map<K, V>;

template <typename T, typename Compare = sprt::less<void>>
using Set = sprt::__pool_set<T, Compare>;

template <typename V>
using HashSet = sprt::__pool_unordered_set<V>;

using StringStream = typename Interface::StringStreamType;

template <typename T>
using Function = sprt::__pool_function<T>;

using stappler::Callback;
using stappler::CallbackStream;

using sprt::makeSpanView;

using memory::perform;
using memory::perform_clear;
using memory::perform_temporary;

using sprt::emplace_ordered;
using sprt::exists_ordered;

} // namespace stappler::mem_pool


namespace STAPPLER_VERSIONIZED stappler::mem_std {

namespace pool = sprt::memory::pool;
namespace allocator = sprt::memory::allocator;

using memory::allocator_t;
using memory::pool_t;

using stappler::Time;
using stappler::TimeInterval;

using stappler::StringView;
using stappler::StringViewUtf8;
using stappler::WideStringView;
using stappler::BytesView;
using stappler::SpanView;

using AllocBase = stappler::memory::StandartInterface::AllocBaseType;

template <typename T>
using Allocator = sprt::detail::AllocatorMalloc<T>;

using String = sprt::__malloc_string;
using WideString = sprt::__malloc_u16string;
using Bytes = sprt::__malloc_vector<uint8_t>;

template <typename T>
using Vector = sprt::__malloc_vector<T>;

template <typename T>
using List = sprt::__malloc_list<T>;

template <typename K, typename V, typename Compare = sprt::less<void>>
using Map = sprt::__malloc_map<K, V, Compare>;

template <typename T, typename V>
using HashMap = sprt::__malloc_unordered_map<T, V, sprt::hash<void>, sprt::equal_to<void>>;

template <typename T, typename Compare = sprt::less<void>>
using Set = sprt::__malloc_set<T, Compare>;

template <typename T, typename Hash = sprt::hash<void>, typename Equal = sprt::equal_to<void>>
using HashSet = sprt::__malloc_unordered_set<T, Hash, Equal>;

using StringStream = typename Interface::StringStreamType;

template <typename T>
using Function = sprt::__malloc_function<T>;

using stappler::Callback;
using stappler::CallbackStream;

using sprt::makeSpanView;

using memory::perform;
using memory::perform_clear;
using memory::perform_temporary;

using sprt::emplace_ordered;
using sprt::exists_ordered;

} // namespace stappler::mem_std


//
// MODULE_STAPPLER_DATA extension
//

#ifdef MODULE_STAPPLER_DATA

namespace STAPPLER_VERSIONIZED stappler::mem_pool {

using Value = stappler::data::ValueTemplate<stappler::memory::PoolInterface>;
using Array = Value::ArrayType;
using Dictionary = Value::DictionaryType;
using EncodeFormat = stappler::data::EncodeFormat;

inline bool emplace_ordered(Vector<Value> &vec, const Value &val) {
	auto lb = sprt::lower_bound(vec.begin(), vec.end(), val,
			[&](const Value &l, const Value &r) { return l.getInteger() < r.getInteger(); });
	if (lb == vec.end()) {
		vec.emplace_back(val);
		return true;
	} else if (*lb != val) {
		vec.emplace(lb, val);
		return true;
	}
	return false;
}

} // namespace stappler::mem_pool


namespace STAPPLER_VERSIONIZED stappler::mem_std {

using Value = data::ValueTemplate<stappler::memory::StandartInterface>;
using Array = Value::ArrayType;
using Dictionary = Value::DictionaryType;
using EncodeFormat = stappler::data::EncodeFormat;

inline bool emplace_ordered(Vector<Value> &vec, const Value &val) {
	auto lb = sprt::lower_bound(vec.begin(), vec.end(), val,
			[&](const Value &l, const Value &r) { return l.getInteger() < r.getInteger(); });
	if (lb == vec.end()) {
		vec.emplace_back(val);
		return true;
	} else if (*lb != val) {
		vec.emplace(lb, val);
		return true;
	}
	return false;
}

} // namespace stappler::mem_std

#endif // MODULE_STAPPLER_DATA


//
// Implementation details
//

namespace STAPPLER_VERSIONIZED stappler {

template <typename T>
VectorAdapter<T>::VectorAdapter(memory::StandartInterface::VectorType<T> &vec) noexcept
: target(&vec)
, size_fn([](void *target) { return ((mem_std::Vector<T> *)target)->size(); })
, back_fn([](void *target) -> T & { return ((mem_std::Vector<T> *)target)->back(); })
, front_fn([](void *target) -> T & { return ((mem_std::Vector<T> *)target)->front(); })
, empty_fn([](void *target) { return ((mem_std::Vector<T> *)target)->empty(); })
, at_fn([](void *target, size_t pos) -> T & { return ((mem_std::Vector<T> *)target)->at(pos); })
, emplace_back_fn([](void *target, T &&v) -> T & {
	return ((mem_std::Vector<T> *)target)->emplace_back(move(v));
})
, insert_fn([](void *target, size_t pos, T &&v) {
	auto v_ = (mem_std::Vector<T> *)target;
	v_->insert(v_->begin() + pos, move(v));
})
, begin_fn([](void *target) -> T * { return &*((mem_std::Vector<T> *)target)->begin(); })
, end_fn([](void *target) -> T * { return &*((mem_std::Vector<T> *)target)->end(); })
, clear_fn([](void *target) { ((mem_std::Vector<T> *)target)->clear(); })
, reserve_fn([](void *target, size_t s) { ((mem_std::Vector<T> *)target)->reserve(s); })
, resize_fn([](void *target, size_t s) { ((mem_std::Vector<T> *)target)->resize(s); }) { }

template <typename T>
VectorAdapter<T>::VectorAdapter(memory::PoolInterface::VectorType<T> &vec) noexcept
: target(&vec)
, size_fn([](void *target) { return ((mem_pool::Vector<T> *)target)->size(); })
, back_fn([](void *target) -> T & { return ((mem_pool::Vector<T> *)target)->back(); })
, front_fn([](void *target) -> T & { return ((mem_pool::Vector<T> *)target)->front(); })
, empty_fn([](void *target) { return ((mem_pool::Vector<T> *)target)->empty(); })
, at_fn([](void *target, size_t pos) -> T & { return ((mem_pool::Vector<T> *)target)->at(pos); })
, emplace_back_fn([](void *target, T &&v) -> T & {
	return ((mem_pool::Vector<T> *)target)->emplace_back(move(v));
})
, insert_fn([](void *target, size_t pos, T &&v) {
	auto v_ = (mem_pool::Vector<T> *)target;
	v_->insert(v_->begin() + pos, move(v));
})
, begin_fn([](void *target) -> T * { return &*((mem_pool::Vector<T> *)target)->begin(); })
, end_fn([](void *target) -> T * { return &*((mem_pool::Vector<T> *)target)->end(); })
, clear_fn([](void *target) { ((mem_pool::Vector<T> *)target)->clear(); })
, reserve_fn([](void *target, size_t s) { ((mem_pool::Vector<T> *)target)->reserve(s); })
, resize_fn([](void *target, size_t s) { ((mem_pool::Vector<T> *)target)->resize(s); }) { }

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_SPMEMORY_H_ */
