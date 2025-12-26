/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_UTILS_SPPRIORITYLIST_H_
#define STAPPLER_CORE_UTILS_SPPRIORITYLIST_H_

#include "SPHashTable.h"

namespace STAPPLER_VERSIONIZED stappler {

template <typename Value>
struct PriorityListEntry : memory::AllocPool {
	PriorityListEntry *prev;
	PriorityListEntry *next;
	void *target;
	int32_t priority;
	Value value;
};

template <typename Value>
struct HashTraits<PriorityListEntry<Value> *> {
	using Entry = PriorityListEntry<Value>;
	static constexpr auto ValueSize = sizeof(PriorityListEntry<Value>);
	static constexpr auto ValueOffset = std::countr_zero(ValueSize);

	static uint32_t hash(uint32_t salt, const Entry *value) {
		return uint32_t(reinterpret_cast<uintptr_t>(value->target) >> ValueOffset);
	}

	static uint32_t hash(uint32_t salt, const void * value) {
		return uint32_t(reinterpret_cast<uintptr_t>(value) >> ValueOffset);
	}

	static bool equal(const Entry *l, const Entry *r) {
		return l->target == r->target;
	}

	static bool equal(const Entry *l, const void * value) {
		return l->target == value;
	}
};

template <typename Value>
class PriorityList {
public:
	using Pool = memory::pool_t;
	using Entry = PriorityListEntry<Value>;

	PriorityList(Pool *p = nullptr)
	: _pool(p ? p : memory::pool::acquire()), _hash(p ? p : memory::pool::acquire()) { }

	~PriorityList() {
		clear();
	}

	template <typename ... Args>
	Value * emplace(void *, int32_t p, Args && ... args);

	Value * find(void *) const;

	// callback returns true if Entry should be removed
	void foreach(const Callback<bool(void *target, int32_t priority, Value &)> &);

	bool erase(const void *);

	void clear();

	bool empty() const;

protected:
	template <typename ... Args>
	Value * emplace_list(bool ordered, Entry **, void *, int32_t p, Args && ... args);

	template <typename ... Args>
	Entry *allocate(void *, int32_t p, Args && ... args);

	void erase_entry(Entry *);
	void free(Entry *);

	void foreach_list(Entry *, const Callback<bool(void *target, int32_t priority, Value &)> &);

	Pool *_pool = nullptr;
	size_t _size = 0;
	Entry *_negList = nullptr;
	Entry *_zeroList = nullptr;
	Entry *_posList = nullptr;
	HashTable<Entry *> _hash;

	Entry *_free = nullptr; /* List of recycled entries */
};

template <typename Value>
template <typename ... Args>
auto PriorityList<Value>::emplace(void *ptr, int32_t p, Args && ... args) -> Value * {
	auto it = _hash.find(ptr);
	if (it != _hash.end()) {
		auto entry = (Entry *)*it;
		if (entry->priority != p) {
			_hash.erase(it);
		} else {
			return &entry->value;
		}
	}

	if (p == 0) {
		return emplace_list(false, &_zeroList, ptr, p, std::forward<Args>(args)...);
	} else if (p < 0) {
		return emplace_list(true, &_negList, ptr, p, std::forward<Args>(args)...);
	} else if (p > 0) {
		return emplace_list(true, &_posList, ptr, p, std::forward<Args>(args)...);
	}

	return nullptr;
}

template <typename Value>
auto PriorityList<Value>::find(void *ptr) const -> Value * {
	auto it = _hash.find(ptr);
	if (it != _hash.end()) {
		auto entry = (Entry *)*it;
		return &entry->value;
	}
	return nullptr;
}

template <typename Value>
void PriorityList<Value>::foreach(const Callback<bool(void *target, int32_t priority, Value &)> &cb) {
	foreach_list(_negList, cb);
	foreach_list(_zeroList, cb);
	foreach_list(_posList, cb);
}

template <typename Value>
bool PriorityList<Value>::erase(const void *ptr) {
	auto it = _hash.find(ptr);
	if (it != _hash.end()) {
		auto v = *it;
		erase_entry(v);
		_hash.erase(it);
		return true;
	}
	return false;
}

template <typename Value>
void PriorityList<Value>::clear() {
	while (_zeroList) {
		auto v = _zeroList;
		_zeroList = _zeroList->next;
		free(v);
	}
	while (_negList) {
		auto v = _negList;
		_negList = _negList->next;
		free(v);
	}
	while (_posList) {
		auto v = _posList;
		_posList = _posList->next;
		free(v);
	}
	_hash.clear();
}

template <typename Value>
bool PriorityList<Value>::empty() const {
	return _hash.empty();
}

template <typename Value>
template <typename ... Args>
auto PriorityList<Value>::emplace_list(bool ordered, Entry **target, void *ptr, int32_t p, Args && ... args) -> Value * {
	Entry *newVal = allocate(ptr, p, std::forward<Args>(args)...);

	if (ordered && *target && (*target)->priority < p) {
		auto v = *target;
		while (v->next && v->next->priority < p) {
			target = &v->next;
			v = v->next;
		}

		// insert between v and v->next
		newVal->prev = v;
		newVal->next = v->next;
		if (newVal->next) {
			newVal->next->prev = newVal;
		}
		v->next = newVal;
	} else {
		// insert first
		newVal->prev = nullptr;
		newVal->next = *target;
		if (newVal->next) {
			newVal->next->prev = newVal;
		}
		*target = newVal;
	}

	_hash.emplace(newVal);

	return &newVal->value;
}

template <typename Value>
template <typename ... Args>
auto PriorityList<Value>::allocate(void *ptr, int32_t p, Args && ... args) -> Entry * {
	Entry *ret = nullptr;
	if (_free) {
		ret = _free;
		_free = ret->next;
		ret->next = nullptr;
	} else {
		ret = new (_pool) Entry;
	}

	ret->target = ptr;
	ret->priority = p;
	new (&ret->value) Value(std::forward<Args>(args)...);

	return ret;
}

template <typename Value>
void PriorityList<Value>::erase_entry(Entry *v) {
	if (v == _zeroList) {
		_zeroList = v->next;
		if (_zeroList) {
			_zeroList->prev = nullptr;
		}
		free(v);
	} else if (v == _negList) {
		_negList = v->next;
		if (_negList) {
			_negList->prev = nullptr;
		}
		free(v);
	} else if (v == _posList) {
		_posList = v->next;
		if (_posList) {
			_posList->prev = nullptr;
		}
		free(v);
	} else {
		if (v->prev) {
			v->prev->next = v->next;
		}
		if (v->next) {
			v->next->prev = v->prev;
		}
		free(v);
	}
}

template <typename Value>
void PriorityList<Value>::free(Entry *v) {
	v->value.~Value();
	v->next = _free;
	v->prev = nullptr;
	if (_free) {
		_free->prev = v;
	}
	_free = v;
}

template <typename Value>
void PriorityList<Value>::foreach_list(Entry *v, const Callback<bool(void *target, int32_t priority, Value &)> &cb) {
	while (v) {
		if (cb(v->target, v->priority, v->value)) {
			auto next = v->next;
			_hash.erase(v);
			erase_entry(v);
			v = next;
		} else {
			v = v->next;
		}
	}
}

}

#endif /* STAPPLER_CORE_UTILS_SPPRIORITYLIST_H_ */
