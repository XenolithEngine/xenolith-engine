/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_UTILS_SPHASHTABLE_H_
#define STAPPLER_CORE_UTILS_SPHASHTABLE_H_

#include "SPLog.h"
#include "SPRef.h"

namespace STAPPLER_VERSIONIZED stappler {

class SP_PUBLIC NamedRef : public Ref {
public:
	virtual ~NamedRef() { }
	virtual StringView getName() const = 0;
};

struct SP_PUBLIC NamedMem : memory::AllocPool {
	virtual ~NamedMem() { }

	StringView key;
};

using HashFunc = uint32_t (*)(const char *key, size_t *klen);

template <typename Value>
class HashTable;

template <typename Value>
struct HashTraits;

template <>
struct HashTraits<NamedRef *> {
	static uint32_t hash(uint32_t salt, const NamedRef *value) {
		auto name = value->getName();
		return hash::hash32(name.data(), uint32_t(name.size()), salt);
	}

	static bool equal(const NamedRef *l, const NamedRef *r) { return l == r; }
};

template <>
struct HashTraits<Rc<NamedRef>> {
	static uint32_t hash(uint32_t salt, const NamedRef *value) {
		auto name = value->getName();
		return hash::hash32(name.data(), uint32_t(name.size()), salt);
	}

	static uint32_t hash(uint32_t salt, StringView value) {
		return hash::hash32(value.data(), uint32_t(value.size()), salt);
	}

	static bool equal(const NamedRef *l, const NamedRef *r) { return l == r; }

	static bool equal(const NamedRef *l, StringView value) { return l->getName() == value; }
};

template <>
struct HashTraits<NamedMem *> {
	static uint32_t hash(uint32_t salt, const NamedMem *value) {
		return hash::hash32(value->key.data(), uint32_t(value->key.size()), salt);
	}

	static uint32_t hash(uint32_t salt, StringView value) {
		return hash::hash32(value.data(), uint32_t(value.size()), salt);
	}

	static bool equal(const NamedMem *l, const NamedMem *r) { return l->key == r->key; }

	static bool equal(const NamedMem *l, StringView value) { return l->key == value; }
};

template <typename Value>
struct HashTraitDiscovery;

template <typename Value>
struct HashTraitDiscovery<Rc<Value>> {
	using type = typename std::conditional< std::is_base_of<NamedRef, Value>::value,
			HashTraits<Rc<NamedRef>>, typename HashTraitDiscovery<Value *>::type>::type;
};

template <typename Value>
struct HashTraitDiscovery<Value *> {
	using type = typename std::conditional< std::is_base_of<NamedMem, Value>::value,
			HashTraits<NamedMem *>, HashTraits<Value *>>::type;
};

template <typename Value>
struct HashEntry {
	using Type = typename std::remove_cv<typename std::remove_reference<Value>::type>::type;
	using Traits = typename HashTraitDiscovery<Value>::type;

	template <typename... Args>
	static uint32_t getHash(uint32_t salt, Args &&...args) {
		return Traits::hash(salt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static bool isEqual(const Value &l, Args &&...args) {
		return Traits::equal(l, std::forward<Args>(args)...);
	}

	HashEntry *next;
	uint32_t hash;
	std::array<uint8_t, sizeof(Value)> data;

	Value *get() { return (Value *)data.data(); }
	const Value *get() const { return (const Value *)data.data(); }
};

template <typename Value>
struct HashIndex {
	using Type = typename std::remove_cv<typename std::remove_reference<Value>::type>::type;

	HashTable<Value> *ht;
	HashEntry<Type> **_bucket;
	HashEntry<Type> *_self;
	HashEntry<Type> *_next;
	uint32_t index;

	HashIndex *next() {
		if (_self) {
			_bucket = &_self->next;
		}
		_self = _next;
		while (!_self) {
			if (index > ht->max) {
				_self = _next = nullptr;
				_bucket = nullptr;
				return nullptr;
			}
			_self = ht->array[index];
			_bucket = &ht->array[index];
			++index;
		}
		_next = _self->next;
		return this;
	}

	bool operator==(const HashIndex &l) const {
		return l.ht == ht && l._self == _self && l._next == _next && l.index == index;
	}
	bool operator!=(const HashIndex &l) const {
		return l.ht != ht || l._self != _self || l._next != _next || l.index != index;
	}

	HashIndex &operator++() {
		this->next();
		return *this;
	}
	HashIndex operator++(int) {
		auto tmp = *this;
		this->next();
		return tmp;
	}

	Type &operator*() const { return *_self->get(); }
	Type *operator->() const { return _self->get(); }
};

template <typename Value>
struct ConstHashIndex {
	using Type = typename std::remove_cv<typename std::remove_reference<Value>::type>::type;

	const HashTable<Value> *ht;
	const HashEntry<Type> *const *_bucket;
	const HashEntry<Type> *_self;
	const HashEntry<Type> *_next;
	uint32_t index;

	ConstHashIndex *next() {
		if (_self) {
			_bucket = &_self->next;
		}
		_self = _next;
		while (!_self) {
			if (index > ht->max) {
				_self = _next = nullptr;
				return nullptr;
			}
			_self = ht->array[index];
			_bucket = &ht->array[index];
			++index;
		}
		_next = _self->next;
		return this;
	}

	bool operator==(const ConstHashIndex &l) const {
		return l.ht == ht && l._self == _self && l._next == _next && l.index == index;
	}
	bool operator!=(const ConstHashIndex &l) const {
		return l.ht != ht || l._self != _self || l._next != _next || l.index != index;
	}

	ConstHashIndex &operator++() {
		this->next();
		return *this;
	}
	ConstHashIndex operator++(int) {
		auto tmp = *this;
		this->next();
		return tmp;
	}

	const Type &operator*() const { return *_self->get(); }
	const Type *operator->() const { return _self->get(); }
};

template <typename Value>
class HashTable {
public:
	using Pool = memory::pool_t;
	using ValueType = HashEntry<Value>;

	using merge_fn = void *(*)(Pool * p, const void *key, size_t klen, const void *h1_val,
			const void *h2_val, const void *data);
	using foreach_fn = bool (*)(void *rec, const void *key, size_t klen, const void *value);

	static constexpr auto INITIAL_MAX = 15; /* tunable == 2^n - 1 */

	using iterator = HashIndex<Value>;
	using const_iterator = ConstHashIndex<Value>;

	HashTable(Pool *p = nullptr) {
		if (!p) {
			p = memory::pool::acquire();
		}

		sprt_passert(p, "Pool should be defined");

		auto now = Time::now().toMicros();
		this->pool = p;
		this->free = nullptr;
		this->count = 0;
		this->max = INITIAL_MAX;
		this->seed = (unsigned int)((now >> 32) ^ now ^ (uintptr_t)pool ^ (uintptr_t)this
							 ^ (uintptr_t)&now)
				- 1;
		this->array = alloc_array(this->pool, max);
	}

	HashTable(const HashTable &copy, Pool *p = nullptr) {
		if (!p) {
			p = memory::pool::acquire();
		}

		sprt_passert(p, "Pool should be defined");

		this->pool = p;
		this->free = nullptr;
		this->count = copy.count;
		this->max = copy.max;
		this->seed = copy.seed;
		this->array = this->do_copy(copy.array, copy.max);
	}

	HashTable(HashTable &&copy, Pool *p = nullptr) {
		if (!p) {
			p = memory::pool::acquire();
		}

		sprt_passert(p, "Pool should be defined");

		this->pool = p;
		this->free = nullptr;
		this->count = copy.count;
		this->max = copy.max;
		this->seed = copy.seed;

		if (p == copy.pool) {
			this->array = this->do_copy(copy.array, copy.max);
		} else {
			this->free = copy.free;
			copy.free = nullptr;
			this->array = copy.array;
			copy.array = nullptr;
		}
	}

	~HashTable() {
		if (count) {
			clear();
		}

		if (array) {
			memory::pool::free(pool, array, (max + 1) * sizeof(ValueType));
			array = nullptr;
		}
	}

	HashTable &operator=(const HashTable &other) {
		clear();

		if (array) {
			memory::pool::free(pool, array, (max + 1) * sizeof(ValueType));
			array = nullptr;
		}

		this->free = nullptr;
		this->count = other.count;
		this->max = other.max;
		this->seed = other.seed;
		this->array = this->do_copy(other.array, other.max);

		return *this;
	}

	HashTable &operator=(HashTable &&other) {
		clear();

		if (array) {
			memory::pool::free(pool, array, (max + 1) * sizeof(ValueType));
			array = nullptr;
		}

		this->free = nullptr;
		this->count = other.count;
		this->max = other.max;
		this->seed = other.seed;

		if (this->pool == other.pool) {
			this->array = this->do_copy(other.array, other.max);
		} else {
			this->free = other.free;
			other.free = nullptr;
			this->array = other.array;
			other.array = nullptr;
		}

		return *this;
	}

	template <typename... Args>
	Pair<iterator, bool> assign(Args &&...args) {
		ValueType **hep = nullptr;
		iterator iter;
		auto ret = set_value(true, &hep, std::forward<Args>(args)...);
		iter._bucket = hep;
		iter._self = ret.first;
		iter._next = ret.first->next;
		iter.index = (ret.first->hash & this->max);
		iter.ht = this;
		return pair(iter, ret.second);
	}

	template <typename... Args>
	Pair<iterator, bool> emplace(Args &&...args) {
		ValueType **hep = nullptr;
		iterator iter;
		auto ret = set_value(false, &hep, std::forward<Args>(args)...);
		iter._bucket = hep;
		iter._self = ret.first;
		iter._next = ret.first->next;
		iter.index = (ret.first->hash & this->max);
		iter.ht = this;
		return pair(iter, ret.second);
	}

	template <typename... Args>
	bool contains(Args &&...args) const {
		if (auto ret = get_value(nullptr, std::forward<Args>(args)...)) {
			return true;
		}
		return false;
	}

	template <typename... Args>
	iterator find(Args &&...args) {
		ValueType **hep = nullptr;
		if (auto ret = get_value(&hep, std::forward<Args>(args)...)) {
			iterator iter;
			iter._bucket = hep;
			iter._self = ret;
			iter._next = ret->next;
			iter.index = (ret->hash & this->max);
			iter.ht = this;
			return iter;
		}
		return end();
	}

	template <typename... Args>
	const_iterator find(Args &&...args) const {
		ValueType **hep = nullptr;
		if (auto ret = get_value(&hep, std::forward<Args>(args)...)) {
			const_iterator iter;
			iter._bucket = hep;
			iter._self = ret;
			iter._next = ret->next;
			iter.index = (ret->hash & this->max);
			iter.ht = this;
			return iter;
		}
		return end();
	}

	template <typename... Args>
	const typename ValueType::Type get(Args &&...args) const {
		if (auto ret = get_value(nullptr, std::forward<Args>(args)...)) {
			return *ret->get();
		}
		return nullptr;
	}

	iterator erase(iterator iter) {
		if (this->count == 0) {
			return end();
		}
		ValueType *old = *iter._bucket;
		*iter._bucket = (*iter._bucket)->next;
		old->next = this->free;
		old->get()->~Value();
		this->free = old;
		--this->count;

		// bucket already points to next, try to offset _self quickly
		iter._self = iter._next;
		if (iter._self) {
			iter._next = iter._self->next;
		} else {
			// or search for next hash bucket
			iter.next();
		}

		if (this->count == 0) {
			return end();
		}
		return iter;
	}

	template <typename... Args>
	iterator erase(Args &&...args) {
		ValueType **hep, *he;
		const auto hash = ValueType::getHash(seed, std::forward<Args>(args)...);
		const auto idx = hash & this->max;

		/* scan linked list */
		for (hep = &this->array[idx], he = *hep; he; hep = &he->next, he = *hep) {
			if (he->hash == hash && ValueType::isEqual(*he->get(), std::forward<Args>(args)...)) {
				break;
			}
		}

		if (he) {
			// make iterator for current cell
			iterator iter;
			iter._bucket = hep;
			iter._self = he;
			iter._next = he->next;
			iter.index = (he->hash & this->max);
			iter.ht = this;

			return erase(iter);
		} else {
			return end();
		}
	}

	size_t size() const { return count; }
	bool empty() const { return count == 0; }

	void reserve(size_t c) {
		if (!this->array) {
			if constexpr (sizeof(size_t) == 8) {
				do_allocate_array(math::npot(uint64_t(c)) - 1);
			} else {
				do_allocate_array(math::npot(uint32_t(c)) - 1);
			}
			return;
		}

		if (c <= this->count) {
			return;
		}

		if (c > this->max) {
			expand_array(this, c);
		}

		auto mem = (ValueType *)memory::pool::palloc(this->pool,
				sizeof(ValueType) * (c - this->count));

		for (size_t i = this->count; i < c; ++i) {
			mem->next = free;
			free = mem;
			++mem;
		}
	}

	void clear() {
		if (!array) {
			return;
		}

		for (size_t i = 0; i <= max; ++i) {
			auto v = array[i];
			while (v) {
				auto tmp = v;
				v = v->next;

				tmp->next = free;
				free = tmp;
				tmp->get()->~Value();
			}
			array[i] = nullptr;
		}

		count = 0;
	}

	iterator begin() {
		if (!array) {
			return end();
		}

		HashIndex<Value> hi;
		hi.ht = this;
		hi.index = 0;
		hi._bucket = nullptr;
		hi._self = nullptr;
		hi._next = nullptr;
		hi.next();
		return hi;
	}
	iterator end() {
		HashIndex<Value> hi;
		hi.ht = this;
		hi.index = this->max + 1;
		hi._bucket = nullptr;
		hi._self = nullptr;
		hi._next = nullptr;
		return hi;
	}

	const_iterator begin() const {
		if (!array) {
			return end();
		}

		ConstHashIndex<Value> hi;
		hi.ht = this;
		hi.index = 0;
		hi._bucket = nullptr;
		hi._self = nullptr;
		hi._next = nullptr;
		hi.next();
		return hi;
	}

	const_iterator end() const {
		ConstHashIndex<Value> hi;
		hi.ht = this;
		hi.index = this->max + 1;
		hi._bucket = nullptr;
		hi._self = nullptr;
		hi._next = nullptr;
		return hi;
	}

	size_t get_cell_count() const {
		if (!array) {
			return 0;
		}

		size_t count = 0;
		for (size_t i = 0; i <= max; ++i) {
			if (array[i]) {
				++count;
			}
		}
		return count;
	}

	size_t get_free_count() const {
		size_t count = 0;
		auto f = free;
		while (f) {
			f = f->next;
			++count;
		}
		return count;
	}

	uint32_t get_seed() const { return seed; }

private:
	friend struct HashIndex<Value>;
	friend struct ConstHashIndex<Value>;

	ValueType *allocate_value() {
		return (ValueType *)memory::pool::palloc(this->pool, sizeof(ValueType));
	}

	static HashEntry<Value> **alloc_array(memory::pool_t *p, uint32_t max) {
		return (ValueType **)memory::pool::calloc(p, max + 1, sizeof(ValueType));
	}

	static void expand_array(HashTable *ht, uint32_t new_max = 0) {
		HashEntry<Value> **new_array;

		if (!new_max) {
			new_max = ht->max * 2 + 1;
		} else {
			new_max = math::npot(new_max) - 1;
			if (new_max <= ht->max) {
				return;
			}
		}

		new_array = alloc_array(ht->pool, new_max);

		auto end = ht->end();
		auto hi = ht->begin();
		while (hi != end) {
			uint32_t i = hi._self->hash & new_max;
			hi._self->next = new_array[i];
			new_array[i] = hi._self;
			++hi;
		}

		memory::pool::free(ht->pool, ht->array, (ht->max + 1) * sizeof(ValueType));

		ht->array = new_array;
		ht->max = new_max;
	}

	template <typename... Args>
	ValueType *get_value(ValueType ***bucket, Args &&...args) const {
		if (!this->array) {
			return nullptr;
		}

		ValueType **hep, *he;
		const auto hash = ValueType::getHash(seed, std::forward<Args>(args)...);
		const auto idx = hash & this->max;

		/* scan linked list */
		for (hep = &this->array[idx], he = *hep; he; hep = &he->next, he = *hep) {
			if (he->hash == hash && ValueType::isEqual(*he->get(), std::forward<Args>(args)...)) {
				break;
			}
		}

		if (bucket) {
			*bucket = hep;
		}
		return he;
	}

	template <typename... Args>
	Pair<ValueType *, bool> set_value(bool replace, ValueType ***bucket, Args &&...args) {
		if (!this->array) {
			do_allocate_array(INITIAL_MAX);
		}

		ValueType **hep, *he;
		const auto hash = ValueType::getHash(seed, std::forward<Args>(args)...);
		const auto idx = hash & this->max;

		/* scan linked list */
		for (hep = &this->array[idx], he = *hep; he; hep = &he->next, he = *hep) {
			if (he->hash == hash && ValueType::isEqual(*he->get(), std::forward<Args>(args)...)) {
				break;
			}
		}

		if (he) {
			if (replace) {
				he->get()->~Value();
				new (he->data.data()) Value(std::forward<Args>(args)...);
			}
			if (bucket) {
				*bucket = hep;
			}
			return pair(he, false);
		} else {
			/* add a new entry for non-NULL values */
			if ((he = this->free) != NULL) {
				this->free = he->next;
			} else {
				he = allocate_value();
			}

			this->count++;
			he->next = NULL;
			he->hash = hash;
			new (he->data.data()) Value(std::forward<Args>(args)...);

			*hep = he;

			/* check that the collision rate isn't too high */
			if (this->count > this->max) {
				expand_array(this);
			}
			if (bucket) {
				*bucket = hep;
			}
			return pair(he, true);
		}
	}

	HashEntry<Value> **do_copy(HashEntry<Value> **copy_array, uint32_t copy_max) {
		auto new_array = alloc_array(this->pool, copy_max);
		auto new_vals = (HashEntry<Value> *)memory::pool::palloc(this->pool,
				sizeof(ValueType) * this->count);

		size_t j = 0;
		for (size_t i = 0; i <= copy_max; i++) {
			auto target = &new_array[i];
			auto orig_entry = copy_array[i];
			while (orig_entry) {
				auto new_entry = &new_vals[j++];
				new_entry->next = nullptr;
				new_entry->hash = orig_entry->hash;
				new (new_entry->data.data()) Value(*orig_entry->get());
				*target = new_entry;
				target = &new_entry->next;
				orig_entry = orig_entry->next;
			}
		}
		return new_array;
	}

	void do_allocate_array(uint32_t max) {
		auto now = Time::now().toMicros();
		this->count = 0;
		this->max = max;
		this->seed = (unsigned int)((now >> 32) ^ now ^ (uintptr_t)pool ^ (uintptr_t)this
							 ^ (uintptr_t)&now)
				- 1;
		this->array = alloc_array(this->pool, this->max);
	}

	Pool *pool = nullptr;
	HashEntry<Value> **array;
	uint32_t count, max, seed;
	HashEntry<Value> *free = nullptr; /* List of recycled entries */
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_UTILS_SPHASHTABLE_H_ */
