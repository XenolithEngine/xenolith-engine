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

#include "SPDataSource.h"

namespace STAPPLER_VERSIONIZED stappler::data {

Source::Id Source::Self(Source::Id::max());

struct Source::Slice {
	Id::Type idx;
	size_t len;
	Source *cat;
	size_t offset;
	bool recieved;

	Slice(Id::Type idx, size_t len, Source *cat)
	: idx(idx), len(len), cat(cat), offset(0), recieved(false) { }
};

struct Source::SliceRequest : public Ref {
	sprt::__malloc_vector<Slice> vec;
	BatchCallback cb;
	size_t ready = 0;
	size_t requests = 0;
	Map<Id, Value> data;

	SliceRequest(const BatchCallback &cb) : cb(cb) { }

	size_t request(size_t off) {
		size_t ret = 0;

		requests = vec.size();
		for (auto &it : vec) {
			it.offset = off;
			off += it.len;
			auto ptr = &it;
			auto cat = it.cat;

			auto callId = sprt::retain(this);
			auto linkId = sprt::retain(cat);
			cat->onSliceRequest([this, ptr, cat, linkId, callId](Map<Id, Value> &val) {
				onSliceData(ptr, val);
				sprt::release(cat, linkId);
				sprt::release(this, callId);
			}, it.idx, it.len);
			ret += it.len;
		}
		return ret;
	}

	bool onSliceData(Slice *ptr, Map<Id, Value> &val) {
		ptr->recieved = true;

		// An empty answer still completes its slice. The rebase below reads val.begin(), so a
		// source that returned nothing would otherwise take the whole request down with it.
		if (!val.empty()) {
			// Keys are rebased on the SMALLEST key actually returned, which is why a
			// BatchSourceCallback must answer with exactly first..first+size-1: a sparse answer
			// silently shifts every value onto the wrong index.
			auto front = val.begin()->first;
			for (auto &it : val) {
				if (it.first != Self) {
					data.emplace(it.first + Id(ptr->offset) - front, sp::move(it.second));
				} else {
					data.emplace(Id(ptr->offset), sp::move(it.second));
				}
			}
		}

		ready++;
		requests--;

		if (ready >= vec.size() && requests == 0) {
			for (auto &it : vec) {
				if (!it.recieved) {
					return false;
				}
			}

			cb(data);
			return true;
		}

		return false;
	}
};

struct Source::BatchRequest {
	sprt::__malloc_vector<Id> vec;
	BatchCallback cb;
	size_t requests = 0;
	Rc<Source> cat;
	Map<Id, Value> map;

	static void request(const BatchCallback &cb, Id::Type first, size_t size, Source *cat,
			const DataSourceCallback &scb) {
		// Nothing to wait for: an empty range (or a category with no item accessor at all) would
		// build a request whose counter can never reach zero - one that never answers and never
		// frees itself. Complete it here instead.
		if (size == 0 || !scb) {
			Map<Id, Value> empty;
			cb(empty);
			return;
		}

		if (auto req = new (sprt::nothrow) BatchRequest(cb, first, size, cat)) {
			req->run(scb);
		}
	}

	BatchRequest(const BatchCallback &cb, Id::Type first, size_t size, Source *cat)
	: cb(cb), cat(cat) {
		for (auto i = first; i < first + size; i++) { vec.emplace_back(i); }

		// The loop in run() counts as a pending request of its own. A DataSourceCallback that
		// answers inline - a directory walk, a cache hit - would otherwise drive the counter to
		// zero on the last item and delete this object while the loop is still walking its vector.
		requests = vec.size() + 1;
	}

	void run(const DataSourceCallback &scb) {
		for (auto &it : vec) {
			scb([this, it](Value &&val) {
				if (val.isArray()) {
					onData(it, sp::move(val.getValue(0)));
				} else {
					onData(it, sp::move(val));
				}
			}, it);
		}
		complete();
	}

	void onData(Id id, Value &&val) {
		map.insert(sprt::make_pair(id, sp::move(val)));
		complete();
	}

	void complete() {
		--requests;
		if (requests == 0) {
			cb(map);
			sprt::__delete(this);
		}
	}
};

void Source::updateCount() {
	_count = _orphanCount;
	for (auto &it : _subCats) { _count += it->getGlobalCount(); }
}

void Source::clear() {
	_subCats.clear();
	_count = _orphanCount;
	setDirty();
}

void Source::addSubcategry(Source *cat) {
	_subCats.emplace_back(cat);
	_count += cat->getGlobalCount();
	setDirty();
}

Source::~Source() { }

Source *Source::getCategory(size_t n) {
	if (n < getSubcatCount()) {
		return _subCats.at(n);
	}
	return nullptr;
}

size_t Source::getCount(uint32_t l, bool subcats) const {
	auto c = _orphanCount + ((subcats) ? _subCats.size() : 0);
	if (l > 0) {
		for (auto cat : _subCats) { c += cat->getCount(l - 1, subcats); }
	}
	return c;
}

size_t Source::getSubcatCount() const { return _subCats.size(); }
size_t Source::getItemsCount() const { return _orphanCount; }
size_t Source::getGlobalCount() const { return _count; }

Source::Id Source::getId() const { return _categoryId; }

void Source::setSubCategories(Vector<Rc<Source>> &&vec) {
	_subCats = sp::move(vec);
	updateCount();
	setDirty();
}
void Source::setSubCategories(const Vector<Rc<Source>> &vec) {
	_subCats = vec;
	updateCount();
	setDirty();
}
auto Source::getSubCategories() const -> const Vector<Rc<Source>> & { return _subCats; }

void Source::setChildsCount(size_t count) {
	_count -= _orphanCount;
	_orphanCount = count;
	_count += _orphanCount;
	setDirty();
}

size_t Source::getChildsCount() const { return _orphanCount; }

auto Source::getChildsState() const -> ChildsState { return _childsState; }

bool Source::hasChildsSource() const { return _childsCallback != nullptr; }

bool Source::requestChilds(Function<void()> &&onComplete) {
	switch (_childsState) {
	case ChildsState::Empty:
	case ChildsState::Loaded:
		if (onComplete) {
			onComplete();
		}
		return _childsState == ChildsState::Loaded;
	case ChildsState::Loading:
		if (onComplete) {
			_childsComplete.emplace_back(sp::move(onComplete));
		}
		return true;
	case ChildsState::Pending: break;
	}

	if (onComplete) {
		_childsComplete.emplace_back(sp::move(onComplete));
	}
	_childsState = ChildsState::Loading;

	// The completion may outlive every other reference to this category - a fetch that answers
	// after the view that asked for it went away - so it carries one of its own. resetChilds() is
	// the escape hatch for the completion that will never fire.
	auto linkId = sprt::retain(this);
	_childsCallback(this, [this, linkId] {
		_childsState = ChildsState::Loaded;

		// Moved out before they run: a completion is free to ask for more children, and the
		// vector it would append to must not be the one being walked.
		auto complete = sp::move(_childsComplete);
		_childsComplete.clear();

		setDirty();
		for (auto &it : complete) { it(); }

		sprt::release(this, linkId);
	});
	return true;
}

void Source::resetChilds() {
	if (!_childsCallback) {
		return;
	}

	_childsComplete.clear();
	_childsState = ChildsState::Pending;
	_subCats.clear();
	_orphanCount = 0;
	updateCount();
	setDirty();
}

void Source::setData(const Value &val) { _data = val; }

void Source::setData(Value &&val) { _data = sp::move(val); }

auto Source::getData() const -> const Value & { return _data; }

void Source::setDirty() { Subscription::setDirty(); }

void Source::setCategoryBounds(Id &first, size_t &count, uint32_t l, bool subcats) {
	// first should be 0 or bound value, that <= first
	if (l == 0 || _subCats.size() == 0) {
		first = Id(0);
		count = getCount(l, subcats);
		return;
	}

	size_t lowerBound = 0, subcat = 0, offset = 0;
	do {
		lowerBound += offset;
		offset = _subCats.at(subcat)->getCount(l - 1, subcats);
		subcat++;
	} while (subcat < (size_t)_subCats.size() && lowerBound + offset <= (size_t)first.get());

	// check if we should skip last subcategory
	if (lowerBound + offset <= first.get()) {
		lowerBound += offset;
	}

	offset = size_t(first.get()) - lowerBound;
	first = Id(lowerBound);
	count += offset; // increment size to match new bound

	size_t upperBound = getCount(l, subcats);
	if (upperBound - _orphanCount >= lowerBound + count) {
		upperBound -= _orphanCount;
	}

	offset = 0;
	subcat = _subCats.size();
	while (subcat > 0 && upperBound - offset >= lowerBound + count) {
		upperBound -= offset;
		offset = _subCats.at(subcat - 1)->getCount(l - 1, subcats);
		subcat--;
	}

	count = upperBound - lowerBound;
}

bool Source::getItemData(const DataCallback &cb, Id index) {
	if (index.get() >= _orphanCount && index != Self) {
		return false;
	}

	// The category's own record is served from _data when it has one. Falling through to the item
	// accessor below would deliver the same row a second time.
	if (index == Self && _data) {
		cb(Value(_data));
		return true;
	}

	if (!_sourceCallback) {
		return false;
	}

	_sourceCallback(cb, index);
	return true;
}

bool Source::getItemData(const DataCallback &cb, Id n, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto &cat : _subCats) {
			if (subcats) {
				if (n.empty()) {
					return cat->getItemData(cb, Self);
				} else {
					n--;
				}
			}
			auto c = Id(cat->getCount(l - 1, subcats));
			if (n < c) {
				return cat->getItemData(cb, n, l - 1, subcats);
			} else {
				n -= c;
			}
		}
	}

	if (!subcats) {
		return getItemData(cb, Id(n));
	} else {
		if (!_subCats.empty() && n < Id(_subCats.size())) {
			return _subCats.at(size_t(n.get()))->getItemData(cb, Self);
		}

		return getItemData(cb, n - Id(_subCats.size()));
	}
}

bool Source::removeItem(Id index, const Value &v) {
	if (index.get() >= _orphanCount && index != Self) {
		return false;
	}

	if (_removeCallback && index != Self) {
		if (_removeCallback(index, v)) {
			_orphanCount -= 1;
			_count -= 1;
			setDirty();
			return true;
		}
	}
	return false;
}

bool Source::removeItem(Id n, const Value &v, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto catIt = _subCats.begin(); catIt != _subCats.end(); ++catIt) {
			auto &cat = *catIt;
			if (subcats) {
				if (n.empty()) {
					if (cat->removeItem(Self, v)) {
						_subCats.erase(catIt);
						return true;
					}
					return false;
				} else {
					n--;
				}
			}
			auto c = Id(cat->getCount(l - 1, subcats));
			if (n < c) {
				return cat->removeItem(n, v, l - 1, subcats);
			} else {
				n -= c;
			}
		}
	}

	if (!subcats) {
		return removeItem(Id(n), v);
	} else {
		if (!_subCats.empty() && n < Id(_subCats.size())) {
			_subCats.at(size_t(n.get()))->removeItem(Self, v);
		}

		return removeItem(n - Id(_subCats.size()), v);
	}
}

size_t Source::getSliceData(const BatchCallback &cb, Id first, size_t count, uint32_t l,
		bool subcats) {
	auto req = Rc<SliceRequest>::create(cb);

	size_t f = size_t(first.get());
	onSlice(req->vec, f, count, l, subcats);

	if (!req->vec.empty()) {
		return req->request(size_t(first.get()));
	} else {
		return 0;
	}
}

sprt::pair<Source *, bool> Source::getItemCategory(Id n, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto &cat : _subCats) {
			if (subcats) {
				if (n.empty()) {
					return sprt::make_pair(cat, true);
				} else {
					n--;
				}
			}
			auto c = cat->getCount(l - 1, subcats);
			if (n.get() < c) {
				return cat->getItemCategory(n, l - 1, subcats);
			} else {
				n -= Id(c);
			}
		}
	}

	if (!subcats) {
		return sprt::make_pair(this, false);
	} else {
		if (!_subCats.empty() && n.get() < (size_t)_subCats.size()) {
			return sprt::make_pair(_subCats.at(size_t(n.get())), true);
		}

		return sprt::make_pair(this, false);
	}
}

void Source::onSlice(sprt::__malloc_vector<Slice> &vec, size_t &first, size_t &count, uint32_t l,
		bool subcats) {
	if (l > 0) {
		for (auto it = _subCats.begin(); it != _subCats.end(); it++) {
			if (first > 0) {
				if (subcats) {
					first--;
				}

				auto sCount = (*it)->getCount(l - 1, subcats);
				if (sCount <= first) {
					first -= sCount;
				} else {
					(*it)->onSlice(vec, first, count, l - 1, subcats);
				}
			} else if (count > 0) {
				if (subcats) {
					vec.push_back(Slice(Self.get(), 1, *it));
					count -= 1;
				}

				if (count > 0) {
					(*it)->onSlice(vec, first, count, l - 1, subcats);
				}
			}
		}
	}

	if (count > 0 && first < _orphanCount) {
		auto c = sprt::min(count, _orphanCount - first);
		vec.push_back(Slice(first, c, this));

		first = 0;
		count -= c;
	} else if (first >= _orphanCount) {
		first -= _orphanCount;
	}
}

void Source::onSliceRequest(const BatchCallback &cb, Id::Type first, size_t size) {
	if (first == Self.get()) {
		if (!_data) {
			// A category with neither its own record nor an item accessor has nothing to answer
			// with. Complete the request empty rather than leave the caller waiting forever.
			if (!_sourceCallback) {
				Map<Id, Value> map;
				cb(map);
				return;
			}

			_sourceCallback([cb](Value &&val) {
				Map<Id, Value> map;
				if (val.isArray()) {
					map.insert(sprt::make_pair(Self, sp::move(val.getValue(0))));
				} else {
					map.insert(sprt::make_pair(Self, sp::move(val)));
				}
				cb(map);
			}, Self);
		} else {
			Map<Id, Value> map;
			map.insert(sprt::make_pair(Self, _data));
			cb(map);
		}
	} else {
		if (!_batchCallback) {
			BatchRequest::request(cb, first, size, this, _sourceCallback);
		} else {
			_batchCallback(cb, first, size);
		}
	}
}

bool Source::init() { return true; }

bool Source::initValue() { return true; }

bool Source::initValue(const DataSourceCallback &cb) {
	_sourceCallback = cb;
	return true;
}

bool Source::initValue(const BatchSourceCallback &cb) {
	_batchCallback = cb;
	return true;
}

bool Source::initValue(const Id &id) {
	_categoryId = id;
	return true;
}

bool Source::initValue(const ChildsCount &count) {
	_orphanCount = count.get();
	return true;
}

bool Source::initValue(const Value &val) {
	_data = val;
	return true;
}

bool Source::initValue(Value &&val) {
	_data = sp::move(val);
	return true;
}

bool Source::initValue(const ChildsSourceCallback &cb) {
	_childsCallback = cb;
	_childsState = ChildsState::Pending;
	return true;
}

bool Source::initValue(const RemoveSourceCallback &cb) {
	_removeCallback = cb;
	return true;
}

} // namespace stappler::data
