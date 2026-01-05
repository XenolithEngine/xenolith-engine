/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_UTILS_SPREFCONTAINER_H_
#define STAPPLER_CORE_UTILS_SPREFCONTAINER_H_

#include "SPMemInterface.h"

namespace STAPPLER_VERSIONIZED stappler {

template <typename Item, typename Interface>
struct RefContainer {
	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	static constexpr size_t ReserveItems =
			std::max(sizeof(Vector<Item *>) / sizeof(Item *), size_t(4));
	static constexpr size_t ContainerSize =
			std::max(sizeof(Vector<Item *>), sizeof(Item *) * ReserveItems);

	std::array<uint8_t, ContainerSize> _container;
	size_t _nitems = 0;

	~RefContainer();
	RefContainer();

	Item *getItemByTag(uint32_t tag) const;

	Item *addItem(Item *);
	void removeItem(Item *);

	bool invalidateItemByTag(uint32_t tag);
	void invalidateAllItemsByTag(uint32_t tag);

	bool removeItemByTag(uint32_t tag);
	void removeAllItemsByTag(uint32_t tag);

	bool cleanup();

	template <typename Callback>
	void foreach (const Callback &) const;

	void clear();

	bool empty() const;
	size_t size() const;
};

template <typename Item, typename Interface>
RefContainer<Item, Interface>::~RefContainer() {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		while (target != end) {
			(*target)->release(0);
			++target;
		}
	} else {
		auto target = (Vector<Item *> *)_container.data();
		for (auto &it : *target) { it->release(0); }
		target->clear();
		target->~vector();
	}
}

template <typename Item, typename Interface>
RefContainer<Item, Interface>::RefContainer() { }

template <typename Item, typename Interface>
auto RefContainer<Item, Interface>::getItemByTag(uint32_t tag) const -> Item * {
	Item *ret = nullptr;
	foreach ([&](Item *item) {
		if (item->getTag() == tag) {
			ret = item;
			return false;
		}
		return true;
	});
	return ret;
}

template <typename Item, typename Interface>
auto RefContainer<Item, Interface>::addItem(Item *item) -> Item * {
	if (_nitems < ReserveItems) {
		auto target = _container.data() + sizeof(Item *) * _nitems;
		item->retain();
		memcpy(target, &item, sizeof(Item *));
		++_nitems;
	} else if (_nitems > ReserveItems) {
		auto target = (Vector<Item *> *)_container.data();
		item->retain();
		target->emplace_back(item);
	} else {
		// update to Vector storage
		Vector<Item *> acts;
		acts.reserve(_nitems + 1);
		foreach ([&](Item *a) {
			acts.emplace_back(a);
			return true;
		});
		item->retain();
		acts.emplace_back(item);

		new (_container.data()) Vector<Item *>(sp::move(acts));
		++_nitems;
	}
	return item;
}

template <typename Item, typename Interface>
void RefContainer<Item, Interface>::removeItem(Item *item) {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		auto it = std::remove(target, end, item);
		if (std::distance(it, end) > 0) {
			item->release(0);
			--_nitems;
		}
	} else {
		auto target = (Vector<Item *> *)_container.data();
		auto it = target->begin();
		while (it != target->end()) {
			if (*it == item) {
				item->release(0);
				it = target->erase(it);
				break;
			} else {
				++it;
			}
		}
	}
}

template <typename Item, typename Interface>
bool RefContainer<Item, Interface>::invalidateItemByTag(uint32_t tag) {
	bool ret = false;
	foreach ([&](Item *item) {
		if (item->getTag() == tag) {
			item->invalidate();
			ret = true;
			return false;
		}
		return true;
	});
	return ret;
}

template <typename Item, typename Interface>
void RefContainer<Item, Interface>::invalidateAllItemsByTag(uint32_t tag) {
	foreach ([&](Item *item) {
		if (item->getTag() == tag) {
			item->invalidate();
		}
		return true;
	});
}

template <typename Item, typename Interface>
bool RefContainer<Item, Interface>::removeItemByTag(uint32_t tag) {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		while (target != end) {
			if ((*target)->getTag() == tag) {
				(*target)->invalidate();
				(*target)->release(0);
				if (target + 1 != end) {
					memmove(target, target + 1, (end - (target + 1)) * sizeof(Item *));
				}
				--_nitems;
				return true;
			} else {
				++target;
			}
		}
	} else {
		auto target = (Vector<Item *> *)_container.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->getTag() == tag) {
				(*it)->invalidate();
				(*it)->release(0);
				it = target->erase(it);
				return true;
			} else {
				++it;
			}
		}
	}
	return false;
}

#ifdef __clang__
// We ignore std::remove_if result becouse we dont need erase in this usecase
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
#endif

template <typename Item, typename Interface>
void RefContainer<Item, Interface>::removeAllItemsByTag(uint32_t tag) {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		static_cast<void>(std::remove_if(target, end, [&, this](Item *a) {
			if (a->getTag() == tag) {
				a->invalidate();
				a->release(0);
				--_nitems;
				return true;
			}
			return false;
		}));
	} else {
		auto target = (Vector<Item *> *)_container.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->getTag() == tag) {
				(*it)->invalidate();
				(*it)->release(0);
				it = target->erase(it);
			} else {
				++it;
			}
		}
	}
}

template <typename Item, typename Interface>
bool RefContainer<Item, Interface>::cleanup() {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		static_cast<void>(std::remove_if(target, end, [&, this](Item *a) {
			if (a->isDone()) {
				a->release(0);
				--_nitems;
				return true;
			}
			return false;
		}));
		return _nitems == 0;
	} else {
		auto target = (Vector<Item *> *)_container.data();
		auto it = target->begin();
		while (it != target->end()) {
			if ((*it)->isDone()) {
				(*it)->release(0);
				it = target->erase(it);
			} else {
				++it;
			}
		}
		return target->empty();
	}
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

template <typename Item, typename Interface>
template <typename Callback>
void RefContainer<Item, Interface>::foreach (const Callback &cb) const {
	static_assert(std::is_invocable_v<Callback, Item *>, "Invalid callback type");
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		while (target != end) {
			if (!cb(*target)) {
				return;
			}
			++target;
		}
	} else {
		auto target = (Vector<Item *> *)_container.data();
		for (auto &it : *target) {
			if (!cb(it)) {
				return;
			}
		}
	}
}


template <typename Item, typename Interface>
void RefContainer<Item, Interface>::clear() {
	if (_nitems <= ReserveItems) {
		auto target = (Item **)_container.data();
		auto end = (Item **)(_container.data()) + _nitems;
		while (target != end) {
			(*target)->release(0);
			++target;
		}
	} else {
		auto target = (Vector<Item *> *)_container.data();
		for (auto &it : *target) { it->release(0); }
		target->clear();
		target->~vector();
	}
	_nitems = 0;
}

template <typename Item, typename Interface>
bool RefContainer<Item, Interface>::empty() const {
	if (_nitems <= ReserveItems) {
		return _nitems == 0;
	} else {
		auto target = (Vector<Item *> *)_container.data();
		return target->empty();
	}
}

template <typename Item, typename Interface>
size_t RefContainer<Item, Interface>::size() const {
	if (_nitems <= ReserveItems) {
		return _nitems;
	} else {
		auto target = (Vector<Item *> *)_container.data();
		return target->size();
	}
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_CORE_UTILS_SPREFCONTAINER_H_ */
