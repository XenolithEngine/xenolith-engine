/**
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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLL_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLL_H_

#include "XL2dScrollView.h"
#include "MaterialDataSource.h"
#include "MaterialSurface.h"
#include "MaterialIconSprite.h"
#include "XLSubscriptionListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC DataScroll : public ScrollView {
public:
	enum class Request {
		Reset,
		Update,
		Front,
		Back,
	};

	class Item;
	class Handler;
	class Loader;

	using ItemMap = Map<DataSource::Id, Rc<Item>>;
	using DataMap = Map<DataSource::Id, Value>;

	using HandlerCallback = Function<Rc<Handler> (DataScroll *)>;
	using ItemCallback = Function<Rc<Surface> (Item *)>;
	using LoaderCallback = Function<Rc<Loader>(Request, const Function<void()> &)>;

	virtual ~DataScroll() { }

	virtual bool init(DataSource *dataCategory = nullptr, Layout = Layout::Vertical);

	virtual void handleContentSizeDirty() override;
	virtual void reset();

	virtual Value save() const override;
	virtual void load(const Value &) override;

	virtual const ItemMap &getItems() const;

	virtual void setSource(DataSource *);
	virtual DataSource *getSource() const;

	virtual void setLookupLevel(uint32_t level);
	virtual uint32_t getLookupLevel() const;

	virtual void setItemsForSubcats(bool value);
	virtual bool isItemsForSubcat() const;

	virtual void setCategoryBounds(bool value);
	virtual bool hasCategoryBounds() const;

	virtual void setLoaderSize(float);
	virtual float getLoaderSize() const;

	virtual void setMinLoadTime(TimeInterval time);
	virtual TimeInterval getMinLoadTime() const;

	virtual void setMaxSize(size_t max);
	virtual size_t getMaxSize() const;

	virtual void setOriginId(DataSource::Id);
	virtual DataSource::Id getOriginId() const;

public:
	// if you need to share some resources with slice loader thread, use this callback
	// resources will be retained and released by handler in main thread
	virtual void setHandlerCallback(HandlerCallback &&);

	// this callback will be called when scroll tries to load next item in slice
	virtual void setItemCallback(ItemCallback &&);

	// you can use custom loader with this callback
	virtual void setLoaderCallback(LoaderCallback &&);

protected:
	virtual void onSourceDirty();

	virtual size_t getMaxId() const;
	virtual Pair<DataSource *, bool> getSourceCategory(int64_t id);

protected:
	virtual bool requestSlice(DataSource::Id, size_t, Request);

	virtual bool updateSlice();
	virtual bool resetSlice();
	virtual bool downloadFrontSlice(size_t = 0);
	virtual bool downloadBackSlice(size_t = 0);

	virtual void onSliceData(DataMap &, Time, Request);
	virtual void onSliceItems(ItemMap &&, Time, Request);

	virtual void updateItems();
	virtual Rc<Handler> onHandler();
	virtual Rc<Surface> onItemRequest(const ScrollController::Item &, DataSource::Id);
	virtual Rc<Loader> onLoaderRequest(Request type);

	virtual void onOverscroll(float delta) override;
	virtual void updateIndicatorPosition() override;

	uint32_t _categoryLookupLevel = 0;
	bool _itemsForSubcats = false;
	bool _categoryDirty = true;
	bool _useCategoryBounds = false;

	DataListener<DataSource> *_sourceListener = nullptr;

	Rc<Ref> _dataRef;
	HandlerCallback _handlerCallback = nullptr;
	ItemCallback _itemCallback = nullptr;
	LoaderCallback _loaderCallback = nullptr;

	DataSource::Id _currentSliceStart = DataSource::Id(0);
	size_t _currentSliceLen = 0;

	DataSource::Id _sliceOrigin = DataSource::Id(0);

	size_t _sliceMax = 24;
	size_t _sliceSize = 0;
	size_t _slicesCount = 0;
	size_t _itemsCount = 0;

	ItemMap _items;

	Time _invalidateAfter;

	float _savedSize = nan();
	float _loaderSize = 48.0f;
	TimeInterval _minLoadTime = TimeInterval::milliseconds(600);
};

class SP_PUBLIC DataScroll::Loader : public Node {
public:
	virtual ~Loader() { }
	virtual bool init(const Function<void()> &);
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(xenolith::Scene *) override;
	virtual void handleExit() override;

protected:
	IconSprite *_icon = nullptr;
	Function<void()> _callback = nullptr;
};

class SP_PUBLIC DataScroll::Item : public Ref {
public:
	virtual ~Item() { }
	virtual bool init(Value &&val, Vec2, Size2);

	virtual const Value &getData() const;
	virtual Size2 getContentSize() const;
	virtual Vec2 getPosition() const;

	virtual void setPosition(Vec2);
	virtual void setContentSize(Size2);

	virtual void setId(uint64_t);
	virtual uint64_t getId() const;

	virtual void setControllerId(size_t);
	virtual size_t getControllerId() const;

protected:
	uint64_t _id = 0;
	Size2 _size;
	Vec2 _position;
	Value _data;
	size_t _controllerId = 0;
};

class SP_PUBLIC DataScroll::Handler : public Ref {
public:
	using Request = DataScroll::Request;
	using DataMap = DataScroll::DataMap;
	using ItemMap = DataScroll::ItemMap;
	using Layout = DataScroll::Layout;
	using CompleteCallback = Function<void()>;

	virtual ~Handler() { }
	virtual bool init(DataScroll *);

	virtual void setCompleteCallback(CompleteCallback &&);
	virtual const CompleteCallback &getCompleteCallback() const;

	virtual Size2 getContentSize() const;
	virtual DataScroll * getScroll() const;

	virtual ItemMap run(Request t, DataMap &&) = 0; // on worker thread

protected:
	Size2 _size;
	Layout _layout;
	Rc<DataScroll> _scroll;
	CompleteCallback _callback;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLL_H_ */
