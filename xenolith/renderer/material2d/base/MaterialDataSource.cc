/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "MaterialDataSource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

DataSource::Id DataSource::Self(DataSource::Id::max());

struct DataSource::Slice {
	Id::Type idx;
	size_t len;
	DataSource *cat;
	size_t offset;
	bool recieved;

	Slice(Id::Type idx, size_t len, DataSource *cat)
	: idx(idx), len(len), cat(cat), offset(0), recieved(false) { }
};

struct DataSource::SliceRequest : public Ref {
	std::vector<Slice> vec;
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

			auto callId = retain();
			auto linkId = cat->retain();
			cat->onSliceRequest([this, ptr, cat, linkId, callId] (Map<Id, Value> &val) {
				onSliceData(ptr, val);
				cat->release(linkId);
				release(callId);
			}, it.idx, it.len);
			ret += it.len;
		}
		// this will destroy (*this), if direct data access available
		release(0); // free or decrement ref-count
		return ret;
	}

	bool onSliceData(Slice *ptr, Map<Id, Value> &val) {
		ptr->recieved = true;

		auto front = val.begin()->first;
		for(auto &it : val) {
			if (it.first != Self) {
				data.emplace(it.first + Id(ptr->offset) - front, sp::move(it.second));
			} else {
				data.emplace(Id(ptr->offset), sp::move(it.second));
			}
		}

		ready ++;
		requests --;

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

struct DataSource::BatchRequest {
	std::vector<Id> vec;
	BatchCallback cb;
	size_t requests = 0;
	Rc<DataSource> cat;
	Map<Id, Value> map;

	static void request(const BatchCallback &cb, Id::Type first, size_t size, DataSource *cat, const DataSourceCallback &scb) {
		new BatchRequest(cb, first, size, cat, scb);
	}

	BatchRequest(const BatchCallback &cb, Id::Type first, size_t size, DataSource *cat, const DataSourceCallback &scb)
	: cb(cb), cat(cat) {
		for (auto i = first; i < first + size; i++) {
			vec.emplace_back(i);
		}

		requests += vec.size();
		for (auto &it : vec) {
			scb([this, it] (Value &&val) {
				if (val.isArray()) {
					onData(it, sp::move(val.getValue(0)));
				} else {
					onData(it, sp::move(val));
				}
			}, it);
		}
	}

	void onData(Id id, Value &&val) {
		map.insert(std::make_pair(id, sp::move(val)));
		requests --;

		if (requests == 0) {
			cb(map);
			delete this;
		}
	}
};

void DataSource::clear() {
	_subCats.clear();
	_count = _orphanCount;
	setDirty();
}

void DataSource::addSubcategry(DataSource *cat) {
	_subCats.emplace_back(cat);
	_count += cat->getGlobalCount();
	setDirty();
}

DataSource::~DataSource() { }

DataSource * DataSource::getCategory(size_t n) {
	if (n < getSubcatCount()) {
		return _subCats.at(n);
	}
	return nullptr;
}

size_t DataSource::getCount(uint32_t l, bool subcats) const {
	auto c = _orphanCount + ((subcats)?_subCats.size():0);
	if (l > 0) {
		for (auto cat : _subCats) {
			c += cat->getCount(l - 1, subcats);
		}
	}
	return c;
}

size_t DataSource::getSubcatCount() const {
	return _subCats.size();
}
size_t DataSource::getItemsCount() const {
	return _orphanCount;
}
size_t DataSource::getGlobalCount() const {
	return _count;
}

DataSource::Id DataSource::getId() const {
	return _categoryId;
}

void DataSource::setSubCategories(Vector<Rc<DataSource>> &&vec) {
	_subCats = sp::move(vec);
	setDirty();
}
void DataSource::setSubCategories(const Vector<Rc<DataSource>> &vec) {
	_subCats = vec;
	setDirty();
}
const Vector<Rc<DataSource>> &DataSource::getSubCategories() const {
	return _subCats;
}

void DataSource::setChildsCount(size_t count) {
	_count -= _orphanCount;
	_orphanCount = count;
	_count += _orphanCount;
	setDirty();
}

size_t DataSource::getChildsCount() const {
	return _orphanCount;
}

void DataSource::setData(const Value &val) {
	_data = val;
}

void DataSource::setData(Value &&val) {
	_data = sp::move(val);
}

const Value &DataSource::getData() const {
	return _data;
}

void DataSource::setDirty() {
	Subscription::setDirty();
}

void DataSource::setCategoryBounds(Id & first, size_t & count, uint32_t l, bool subcats) {
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
		subcat ++;
	} while(subcat < (size_t)_subCats.size() && lowerBound + offset <= (size_t)first.get());

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
		subcat --;
	}

	count = upperBound - lowerBound;
}

bool DataSource::getItemData(const DataCallback &cb, Id index) {
	if (index.get() >= _orphanCount && index != Self) {
		return false;
	}

	if (index == Self && _data) {
		cb(Value(_data));
	}

	_sourceCallback(cb, index);
	return true;
}

bool DataSource::getItemData(const DataCallback &cb, Id n, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto &cat : _subCats) {
			if (subcats) {
				if (n.empty()) {
					return cat->getItemData(cb, Self);
				} else {
					n --;
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

bool DataSource::removeItem(Id index, const Value &v) {
	if (index.get() >= _orphanCount && index != Self) {
		return false;
	}

	if (_removeCallback && index != Self) {
		if (_removeCallback(index, v)) {
			_orphanCount -= 1;
			return true;
		}
	}
	return false;
}

bool DataSource::removeItem(Id n, const Value &v, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto catIt = _subCats.begin(); catIt != _subCats.end(); ++ catIt) {
			auto &cat = *catIt;
			if (subcats) {
				if (n.empty()) {
					if (cat->removeItem(Self, v)) {
						_subCats.erase(catIt);
						return true;
					}
					return false;
				} else {
					n --;
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

size_t DataSource::getSliceData(const BatchCallback &cb, Id first, size_t count, uint32_t l, bool subcats) {
	SliceRequest *req = new (std::nothrow) SliceRequest(cb);

	size_t f = size_t(first.get());
	onSlice(req->vec, f, count, l, subcats);

	if (!req->vec.empty()) {
		return req->request(size_t(first.get()));
	} else {
		delete req;
		return 0;
	}
}

std::pair<DataSource *, bool> DataSource::getItemCategory(Id n, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto &cat : _subCats) {
			if (subcats) {
				if (n.empty()) {
					return std::make_pair(cat, true);
				} else {
					n --;
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
		return std::make_pair(this, false);
	} else {
		if (!_subCats.empty() && n.get() < (size_t)_subCats.size()) {
			return std::make_pair(_subCats.at(size_t(n.get())), true);
		}

		return std::make_pair(this, false);
	}
}

void DataSource::onSlice(std::vector<Slice> &vec, size_t &first, size_t &count, uint32_t l, bool subcats) {
	if (l > 0) {
		for (auto it = _subCats.begin(); it != _subCats.end(); it ++) {
			if (first > 0) {
				if (subcats) {
					first --;
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
		auto c = std::min(count, _orphanCount - first);
		vec.push_back(Slice(first, c, this));

		first = 0;
		count -= c;
	} else if (first >= _orphanCount) {
		first -= _orphanCount;
	}
}

void DataSource::onSliceRequest(const BatchCallback &cb, Id::Type first, size_t size) {
	if (first == Self.get()) {
		if (!_data) {
			_sourceCallback([cb] (Value &&val) {
				Map<Id, Value> map;
				if (val.isArray()) {
					map.insert(std::make_pair(Self, sp::move(val.getValue(0))));
				} else {
					map.insert(std::make_pair(Self, sp::move(val)));
				}
				cb(map);
			}, Self);
		} else {
			Map<Id, Value> map;
			map.insert(std::make_pair(Self, _data));
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

bool DataSource::init() {
	return true;
}

bool DataSource::initValue() {
	return true;
}

bool DataSource::initValue(const DataSourceCallback &cb) {
	_sourceCallback = cb;
	return true;
}

bool DataSource::initValue(const BatchSourceCallback &cb) {
	_batchCallback = cb;
	return true;
}

bool DataSource::initValue(const Id &id) {
	_categoryId = id;
	return true;
}

bool DataSource::initValue(const ChildsCount &count) {
	_orphanCount = count.get();
	return true;
}

bool DataSource::initValue(const Value &val) {
	_data = val;
	return true;
}

bool DataSource::initValue(Value &&val) {
	_data = sp::move(val);
	return true;
}

}
