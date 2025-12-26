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

#ifndef STAPPLER_DATA_SPDATASOURCE_H_
#define STAPPLER_DATA_SPDATASOURCE_H_

#include "SPMemory.h" // IWYU pragma: keep
#include "SPDataValue.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::data {

class SP_PUBLIC Source : public SubscriptionTemplate<memory::StandartInterface>,
						 public InterfaceObject<memory::StandartInterface> {
public:
	using ChildsCount = ValueWrapper<size_t, class ChildsCountClassFlag>;

	static Id Self;

	using Interface = memory::StandartInterface;
	using Value = ValueTemplate<Interface>;
	using Subscription = SubscriptionTemplate<memory::StandartInterface>;
	using Id = Subscription::Id;

	using BatchCallback = Function<void(Map<Id, Value> &)>;
	using BatchSourceCallback = Function<void(const BatchCallback &, Id::Type first, size_t size)>;

	using DataCallback = Function<void(Value &&)>;
	using DataSourceCallback = Function<void(const DataCallback &, Id)>;

	using RemoveSourceCallback = Function<bool(Id, const Value &)>;

	virtual ~Source();

	template <class T, class... Args>
	bool init(const T &t, Args &&...args) {
		auto ret = initValue(t);
		if (ret) {
			return init(args...);
		}
		return false;
	}

	bool init();

	Source *getCategory(size_t n);

	size_t getCount(uint32_t l = 0, bool subcats = false) const;
	size_t getSubcatCount() const; // number of subcats
	size_t getItemsCount() const; // number of data items (not subcats)
	size_t getGlobalCount() const; // number of all data items in cat and subcats

	void setCategoryBounds(Id &first, size_t &count, uint32_t l = 0, bool subcats = false);

	bool getItemData(const DataCallback &, Id index);
	bool getItemData(const DataCallback &, Id index, uint32_t l, bool subcats = false);
	size_t getSliceData(const BatchCallback &, Id first, size_t count, uint32_t l = 0,
			bool subcats = false);

	bool removeItem(Id index, const Value &);
	bool removeItem(Id index, const Value &, uint32_t l, bool subcats = false);

	std::pair<Source *, bool> getItemCategory(Id itemId, uint32_t l, bool subcats = false);

	Id getId() const;

	void setSubCategories(const Vector<Rc<Source>> &);
	void setSubCategories(Vector<Rc<Source>> &&);
	const Vector<Rc<Source>> &getSubCategories() const;

	void setChildsCount(size_t count);
	size_t getChildsCount() const;

	void setData(const Value &);
	void setData(Value &&);
	const Value &getData() const;

	void clear();
	void addSubcategry(Source *);

	void setDirty();

protected:
	struct BatchRequest;
	struct SliceRequest;
	struct Slice;

	void onSlice(std::vector<Slice> &, size_t &first, size_t &count, uint32_t l, bool subcats);

	virtual bool initValue();
	virtual bool initValue(const DataSourceCallback &);
	virtual bool initValue(const BatchSourceCallback &);
	virtual bool initValue(const Id &);
	virtual bool initValue(const ChildsCount &);
	virtual bool initValue(const Value &);
	virtual bool initValue(Value &&);

	virtual void onSliceRequest(const BatchCallback &, Id::Type first, size_t size);

	Vector<Rc<Source>> _subCats;

	Id _categoryId;
	size_t _count = 0;
	size_t _orphanCount = 0;
	Value _data;

	DataSourceCallback _sourceCallback = nullptr;
	BatchSourceCallback _batchCallback = nullptr;
	RemoveSourceCallback _removeCallback = nullptr;
};

} // namespace stappler::data

#endif /* STAPPLER_DATA_SPDATASOURCE_H_ */
