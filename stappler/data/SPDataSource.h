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

class SP_PUBLIC Source : public SubscriptionTemplate<mem_std::Interface>,
						 public InterfaceObject<mem_std::Interface> {
public:
	using ChildsCount = ValueWrapper<size_t, class ChildsCountClassFlag>;

	static Id Self;

	using Interface = mem_std::Interface;
	using Value = ValueTemplate<Interface>;
	using Subscription = SubscriptionTemplate<mem_std::Interface>;
	using Id = Subscription::Id;

	using BatchCallback = Function<void(Map<Id, Value> &)>;

	// Answers a range of this category's own items. The answer must carry EXACTLY the keys
	// first..first+size-1: a slice spanning several categories rebases each answer on the smallest
	// key it returned, so a sparse one silently shifts every value onto the wrong index. An empty
	// map is a valid answer and completes the request.
	using BatchSourceCallback = Function<void(const BatchCallback &, Id::Type first, size_t size)>;

	using DataCallback = Function<void(Value &&)>;
	using DataSourceCallback = Function<void(const DataCallback &, Id)>;

	using RemoveSourceCallback = Function<bool(Id, const Value &)>;

	// Produces this category's children on its first request. Fill `self` in (setChildsCount /
	// setSubCategories / addSubcategry) and then invoke `complete` EXACTLY ONCE - inline for a
	// source that can answer immediately, such as a directory walk, later for one that has to
	// fetch.
	//
	// `self` is a parameter rather than something the callback captures on purpose: the callback is
	// stored IN the Source, so capturing it would build a cycle the Source could never break.
	using ChildsSourceCallback = Function<void(Source *self, const Function<void()> &complete)>;

	enum class ChildsState {
		Empty, // no lazy callback: the children are whatever was set explicitly
		Pending, // there is a callback and it has not run
		Loading, // the callback ran and its completion has not fired yet
		Loaded, // the completion fired
	};

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

	sprt::pair<Source *, bool> getItemCategory(Id itemId, uint32_t l, bool subcats = false);

	Id getId() const;

	void setSubCategories(const Vector<Rc<Source>> &);
	void setSubCategories(Vector<Rc<Source>> &&);
	const Vector<Rc<Source>> &getSubCategories() const;

	void setChildsCount(size_t count);
	size_t getChildsCount() const;

	ChildsState getChildsState() const;
	bool hasChildsSource() const;

	// Runs the lazy-children callback unless it has already run. `onComplete` fires when the
	// children are available: inline when they already are, at the end of the callback otherwise,
	// and after the pending load when one is already in flight. The completion also calls
	// setDirty(), so a subscriber that never called this still learns about the new children.
	//
	// Returns true when children were loaded or a load is in flight.
	bool requestChilds(Function<void()> &&onComplete = nullptr);

	// Drop the loaded children and go back to Pending, so the next requestChilds() reloads. Pending
	// completions are dropped WITHOUT being called - that is how an owner that is going away
	// releases a Loading category's hold on it.
	void resetChilds();

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

	void onSlice(sprt::__malloc_vector<Slice> &, size_t &first, size_t &count, uint32_t l,
			bool subcats);

	// Recompute the cached global count from the subcategories and the own items. The incremental
	// paths (addSubcategry, setChildsCount) maintain it themselves; a wholesale replacement of the
	// subcategories cannot.
	void updateCount();

	virtual bool initValue();
	virtual bool initValue(const DataSourceCallback &);
	virtual bool initValue(const BatchSourceCallback &);
	virtual bool initValue(const Id &);
	virtual bool initValue(const ChildsCount &);
	virtual bool initValue(const Value &);
	virtual bool initValue(Value &&);
	virtual bool initValue(const ChildsSourceCallback &);
	virtual bool initValue(const RemoveSourceCallback &);

	virtual void onSliceRequest(const BatchCallback &, Id::Type first, size_t size);

	Vector<Rc<Source>> _subCats;

	Id _categoryId;
	size_t _count = 0;
	size_t _orphanCount = 0;
	Value _data;

	DataSourceCallback _sourceCallback = nullptr;
	BatchSourceCallback _batchCallback = nullptr;
	RemoveSourceCallback _removeCallback = nullptr;

	ChildsSourceCallback _childsCallback = nullptr;
	Vector<Function<void()>> _childsComplete;
	ChildsState _childsState = ChildsState::Empty;
};

} // namespace stappler::data

#endif /* STAPPLER_DATA_SPDATASOURCE_H_ */
