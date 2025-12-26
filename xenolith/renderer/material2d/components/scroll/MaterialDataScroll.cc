/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "MaterialDataScroll.h"
#include "XLDirector.h"
#include "XLAppThread.h"
#include "XL2dScrollController.h"
#include "XL2dIcons.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool DataScroll::Loader::init(const Function<void()> &cb) {
	if (!Node::init()) {
		return false;
	}

	_callback = cb;

	setCascadeOpacityEnabled(true);

	_icon = addChild(Rc<IconSprite>::create(IconName::Dynamic_Loader));
	_icon->setContentSize(Size2(36.0f, 36.0f));
	_icon->setAnchorPoint(Vec2(0.5f, 0.5f));

	return true;
}

void DataScroll::Loader::handleContentSizeDirty() {
	Node::handleContentSizeDirty();
	_icon->setPosition(_contentSize / 2.0f);
}

void DataScroll::Loader::handleEnter(Scene *scene) {
	Node::handleEnter(scene);
	_icon->animate();
	if (_callback) {
		_callback();
	}
}

void DataScroll::Loader::handleExit() {
	stopAllActions();
	Node::handleExit();
	_icon->stopAllActions();
}


bool DataScroll::Item::init(Value &&val, Vec2 pos, Size2 size) {
	_data = sp::move(val);
	_position = pos;
	_size = size;
	return true;
}

const Value &DataScroll::Item::getData() const { return _data; }

Size2 DataScroll::Item::getContentSize() const { return _size; }

Vec2 DataScroll::Item::getPosition() const { return _position; }

void DataScroll::Item::setPosition(Vec2 pos) { _position = pos; }

void DataScroll::Item::setContentSize(Size2 size) { _size = size; }

void DataScroll::Item::setId(uint64_t id) { _id = id; }
uint64_t DataScroll::Item::getId() const { return _id; }

void DataScroll::Item::setControllerId(size_t value) { _controllerId = value; }
size_t DataScroll::Item::getControllerId() const { return _controllerId; }

bool DataScroll::Handler::init(DataScroll *s) {
	_size = s->getRoot()->getContentSize();
	_layout = s->getLayout();
	_scroll = s;

	return true;
}

void DataScroll::Handler::setCompleteCallback(CompleteCallback &&cb) { _callback = sp::move(cb); }

const DataScroll::Handler::CompleteCallback &DataScroll::Handler::getCompleteCallback() const {
	return _callback;
}

Size2 DataScroll::Handler::getContentSize() const { return _size; }

DataScroll *DataScroll::Handler::getScroll() const { return _scroll; }

bool DataScroll::init(DataSource *source, Layout l) {
	if (!ScrollView::init(l)) {
		return false;
	}

	setScrollMaxVelocity(5000.0f);

	_sourceListener = addSystem(Rc<DataListener<DataSource>>::create(
			std::bind(&DataScroll::onSourceDirty, this), source));
	_sourceListener->setSubscription(source);

	setController(Rc<ScrollController>::create());

	return true;
}

void DataScroll::handleContentSizeDirty() {
	ScrollView::handleContentSizeDirty();
	if ((isVertical() && _savedSize != _contentSize.width)
			|| (!isVertical() && _savedSize != _contentSize.height)) {
		_savedSize = isVertical() ? _contentSize.width : _contentSize.height;
		onSourceDirty();
	}
}

void DataScroll::reset() {
	_controller->clear();

	auto min = getScrollMinPosition();
	if (!isnan(min)) {
		setScrollPosition(min);
	} else {
		setScrollPosition(0.0f - (isVertical() ? _paddingGlobal.top : _paddingGlobal.left));
	}
}

Value DataScroll::save() const {
	Value ret;
	ret.setDouble(getScrollRelativePosition(), "value");
	ret.setInteger(_currentSliceStart.get(), "start");
	ret.setInteger(_currentSliceLen, "len");
	return ret;
}

void DataScroll::load(const Value &d) {
	if (d.isDictionary()) {
		_savedRelativePosition = d.getDouble("value");
		_currentSliceStart = DataSource::Id(d.getInteger("start"));
		_currentSliceLen = (size_t)d.getInteger("len");
		updateSlice();
	}
}

const DataScroll::ItemMap &DataScroll::getItems() const { return _items; }

void DataScroll::setSource(DataSource *c) {
	if (c != _sourceListener->getSubscription()) {
		_sourceListener->setSubscription(c);
		_categoryDirty = true;

		_invalidateAfter = stappler::Time::now();

		if (_contentSize != Size2::ZERO) {
			_controller->clear();

			if (isVertical()) {
				_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Reset),
						_loaderSize, 0.0f);
			} else {
				auto size = _contentSize.width - _paddingGlobal.left - _loaderSize;
				_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Reset),
						max(_loaderSize, size), 0.0f);
			}

			setScrollPosition(0.0f);
		}
	}
}

DataSource *DataScroll::getSource() const { return _sourceListener->getSubscription(); }

void DataScroll::setLookupLevel(uint32_t level) {
	_categoryLookupLevel = level;
	_categoryDirty = true;
	_sourceListener->setDirty();
}

uint32_t DataScroll::getLookupLevel() const { return _categoryLookupLevel; }

void DataScroll::setItemsForSubcats(bool value) {
	_itemsForSubcats = value;
	_categoryDirty = true;
	_sourceListener->setDirty();
}

bool DataScroll::isItemsForSubcat() const { return _itemsForSubcats; }

void DataScroll::setCategoryBounds(bool value) {
	if (_useCategoryBounds != value) {
		_useCategoryBounds = value;
		_categoryDirty = true;
	}
}

bool DataScroll::hasCategoryBounds() const { return _useCategoryBounds; }

void DataScroll::setMaxSize(size_t max) {
	_sliceMax = max;
	_categoryDirty = true;
	_sourceListener->setDirty();
}

size_t DataScroll::getMaxSize() const { return _sliceMax; }

void DataScroll::setOriginId(DataSource::Id id) { _sliceOrigin = id; }

DataSource::Id DataScroll::getOriginId() const { return _sliceOrigin; }

void DataScroll::setLoaderSize(float value) { _loaderSize = value; }

float DataScroll::getLoaderSize() const { return _loaderSize; }

void DataScroll::setMinLoadTime(TimeInterval time) { _minLoadTime = time; }
TimeInterval DataScroll::getMinLoadTime() const { return _minLoadTime; }

void DataScroll::setHandlerCallback(HandlerCallback &&cb) { _handlerCallback = sp::move(cb); }

void DataScroll::setItemCallback(ItemCallback &&cb) { _itemCallback = sp::move(cb); }

void DataScroll::setLoaderCallback(LoaderCallback &&cb) { _loaderCallback = sp::move(cb); }

void DataScroll::onSourceDirty() {
	if ((isVertical() && _contentSize.height == 0.0f)
			|| (!isVertical() && _contentSize.width == 0.0f)) {
		return;
	}

	if (!_sourceListener->getSubscription() || _items.size() == 0) {
		_controller->clear();
		if (isVertical()) {
			_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Reset),
					_loaderSize, 0.0f);
		} else {
			auto size = _contentSize.width - _paddingGlobal.left - _loaderSize;
			_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Reset),
					std::max(_loaderSize, size), 0.0f);
		}
	}

	if (!_sourceListener->getSubscription()) {
		return;
	}

	auto source = _sourceListener->getSubscription();
	bool init = (_itemsCount == 0);
	_itemsCount = source->getCount(_categoryLookupLevel, _itemsForSubcats);

	if (_itemsCount == 0) {
		_categoryDirty = true;
		_currentSliceStart = DataSource::Id(0);
		_currentSliceLen = 0;
		return;
	} else if (_itemsCount <= _sliceMax) {
		_slicesCount = 1;
		_sliceSize = _itemsCount;
	} else {
		_slicesCount = (_itemsCount + _sliceMax - 1) / _sliceMax;
		_sliceSize = _itemsCount / _slicesCount + 1;
	}

	if ((!init && _categoryDirty) || _currentSliceLen == 0) {
		resetSlice();
	} else {
		updateSlice();
	}

	_scrollDirty = true;
	_categoryDirty = false;
}

size_t DataScroll::getMaxId() const {
	if (!_sourceListener->getSubscription()) {
		return 0;
	}

	auto ret = _sourceListener->getSubscription()->getCount(_categoryLookupLevel, _itemsForSubcats);
	if (ret == 0) {
		return 0;
	} else {
		return ret - 1;
	}
}

std::pair<DataSource *, bool> DataScroll::getSourceCategory(int64_t id) {
	if (!_sourceListener->getSubscription()) {
		return std::make_pair(nullptr, false);
	}

	return _sourceListener->getSubscription()->getItemCategory(DataSource::Id(id),
			_categoryLookupLevel, _itemsForSubcats);
}

bool DataScroll::requestSlice(DataSource::Id first, size_t count, Request type) {
	if (!_sourceListener->getSubscription()) {
		return false;
	}

	if (first.get() >= _itemsCount) {
		return false;
	}

	if (first.get() + count > _itemsCount) {
		count = _itemsCount - size_t(first.get());
	}

	if (_useCategoryBounds) {
		_sourceListener->getSubscription()->setCategoryBounds(first, count, _categoryLookupLevel,
				_itemsForSubcats);
	}

	_invalidateAfter = stappler::Time::now();
	_sourceListener->getSubscription()->getSliceData(
			std::bind(&DataScroll::onSliceData, Rc<DataScroll>(this), std::placeholders::_1,
					Time::now(), type),
			first, count, _categoryLookupLevel, _itemsForSubcats);

	return true;
}

bool DataScroll::updateSlice() {
	auto size = std::max(_currentSliceLen, _sliceSize);
	auto first = _currentSliceStart;
	if (size > _itemsCount) {
		size = _itemsCount;
	}
	if (first.get() > _itemsCount - size) {
		first = DataSource::Id(_itemsCount - size);
	}
	return requestSlice(first, size, Request::Update);
}

bool DataScroll::resetSlice() {
	if (_sourceListener->getSubscription()) {
		int64_t start = int64_t(_sliceOrigin.get()) - int64_t(_sliceSize) / 2;
		if (start + int64_t(_sliceSize) > int64_t(_itemsCount)) {
			start = int64_t(_itemsCount) - int64_t(_sliceSize);
		}
		if (start < 0) {
			start = 0;
		}
		return requestSlice(DataSource::Id(start), _sliceSize, Request::Reset);
	}
	return false;
}

bool DataScroll::downloadFrontSlice(size_t size) {
	if (size == 0) {
		size = _sliceSize;
	}

	if (_sourceListener->getSubscription() && !_currentSliceStart.empty()) {
		DataSource::Id first(0);
		if (_currentSliceStart.get() > _sliceSize) {
			first = _currentSliceStart - DataSource::Id(_sliceSize);
		} else {
			size = size_t(_currentSliceStart.get());
		}

		return requestSlice(first, size, Request::Front);
	}
	return false;
}

bool DataScroll::downloadBackSlice(size_t size) {
	if (size == 0) {
		size = _sliceSize;
	}

	if (_sourceListener->getSubscription()
			&& _currentSliceStart.get() + _currentSliceLen != _itemsCount) {
		DataSource::Id first(_currentSliceStart.get() + _currentSliceLen);
		if (first.get() + size > _itemsCount) {
			size = _itemsCount - size_t(first.get());
		}

		return requestSlice(first, size, Request::Back);
	}
	return false;
}

void DataScroll::onSliceData(DataMap &val, Time time, Request type) {
	if (time < _invalidateAfter || !_handlerCallback) {
		return;
	}

	if (!_director) {
		return;
	}

	if (_items.empty() && type != Request::Update) {
		type = Request::Reset;
	}

	auto itemPtr = new ItemMap();
	auto dataPtr = new DataMap(sp::move(val));
	auto handler = onHandler();

	auto deferred = _director->getApplication();

	deferred->perform(Rc<thread::Task>::create(
			[handler, itemPtr, dataPtr, time, type](const thread::Task &) -> bool {
		(*itemPtr) = handler->run(type, sp::move(*dataPtr));
		for (auto &it : (*itemPtr)) { it.second->setId(it.first.get()); }
		return true;
	}, [this, handler, itemPtr, dataPtr, time, type](const thread::Task &, bool) {
		onSliceItems(sp::move(*itemPtr), time, type);

		auto interval = Time::now() - time;
		if (interval < _minLoadTime && type != Request::Update) {
			auto a = Rc<Sequence>::create(_minLoadTime - interval,
					[guard = Rc<DataScroll>(this), handler, itemPtr, dataPtr] {
				if (guard->isRunning()) {
					const auto &cb = handler->getCompleteCallback();
					if (cb) {
						cb();
					}
				}

				delete itemPtr;
				delete dataPtr;
			});
			runAction(a);
		} else {
			const auto &cb = handler->getCompleteCallback();
			if (cb) {
				cb();
			}

			delete itemPtr;
			delete dataPtr;
		}
	}, this));
}

void DataScroll::onSliceItems(ItemMap &&val, Time time, Request type) {
	if (time < _invalidateAfter) {
		return;
	}

	if (_items.size() > _sliceSize) {
		if (type == Request::Back) {
			auto newId = _items.begin()->first + DataSource::Id(_items.size() - _sliceSize);
			_items.erase(_items.begin(), _items.lower_bound(newId));
		} else if (type == Request::Front) {
			auto newId = _items.begin()->first + DataSource::Id(_sliceSize);
			_items.erase(_items.upper_bound(newId), _items.end());
		}
	}

	if (type == Request::Front || type == Request::Back) {
		val.insert(_items.begin(), _items.end()); // merge item maps
	}

	_items = sp::move(val);

	_currentSliceStart = DataSource::Id(_items.begin()->first);
	_currentSliceLen = size_t(_items.rbegin()->first.get()) + 1 - size_t(_currentSliceStart.get());

	float relPos = getScrollRelativePosition();

	updateItems();

	if (type == Request::Update) {
		setScrollRelativePosition(relPos);
	} else if (type == Request::Reset) {
		if (_sliceOrigin.empty()) {
			setScrollRelativePosition(0.0f);
		} else {
			auto it = _items.find(DataSource::Id(_sliceOrigin));
			if (it == _items.end()) {
				setScrollRelativePosition(0.0f);
			} else {
				auto start = _items.begin()->second->getPosition();
				auto end = _items.rbegin()->second->getPosition();

				if (isVertical()) {
					setScrollRelativePosition(
							fabs((it->second->getPosition().y - start.y) / (end.y - start.y)));
				} else {
					setScrollRelativePosition(
							fabs((it->second->getPosition().x - start.x) / (end.x - start.x)));
				}
			}
		}
	}
}

void DataScroll::updateItems() {
	_controller->clear();

	if (!_items.empty()) {
		if (_items.begin()->first.get() > 0) {
			Item *first = _items.begin()->second;
			_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Front),
					_loaderSize,
					(_layout == Vertical) ? (first->getPosition().y - _loaderSize)
										  : (first->getPosition().x - _loaderSize));
		}

		for (auto &it : _items) {
			it.second->setControllerId(_controller->addItem(
					std::bind(&DataScroll::onItemRequest, this, std::placeholders::_1, it.first),
					it.second->getContentSize(), it.second->getPosition()));
		}

		if (_items.rbegin()->first.get() < _itemsCount - 1) {
			Item *last = _items.rbegin()->second;
			_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Back),
					_loaderSize,
					(_layout == Vertical) ? (last->getPosition().y + last->getContentSize().height)
										  : (last->getPosition().x + last->getContentSize().width));
		}
	} else {
		_controller->addItem(std::bind(&DataScroll::onLoaderRequest, this, Request::Reset),
				_loaderSize, 0.0f);
	}

	auto b = _movement;
	_movement = Movement::None;

	updateScrollBounds();
	onPosition();

	_movement = b;
}

Rc<DataScroll::Handler> DataScroll::onHandler() { return _handlerCallback(this); }

Rc<Surface> DataScroll::onItemRequest(const ScrollController::Item &item, DataSource::Id id) {
	if ((isVertical() && item.size.height > 0.0f) || (isHorizontal() && item.size.width > 0)) {
		auto it = _items.find(id);
		if (it != _items.end()) {
			if (_itemCallback) {
				return _itemCallback(it->second);
			}
		}
	}
	return nullptr;
}

Rc<DataScroll::Loader> DataScroll::onLoaderRequest(Request type) {
	if (type == Request::Back) {
		if (_loaderCallback) {
			return _loaderCallback(type, [this] { downloadBackSlice(_sliceSize); });
		} else {
			return Rc<Loader>::create([this] { downloadBackSlice(_sliceSize); });
		}
	} else if (type == Request::Front) {
		if (_loaderCallback) {
			return _loaderCallback(type, [this] { downloadFrontSlice(_sliceSize); });
		} else {
			return Rc<Loader>::create([this] { downloadFrontSlice(_sliceSize); });
		}
	} else {
		if (_loaderCallback) {
			return _loaderCallback(type, nullptr);
		} else {
			return Rc<Loader>::create(nullptr);
		}
	}
	return nullptr;
}

void DataScroll::updateIndicatorPosition() {
	if (!_indicatorVisible) {
		return;
	}

	const float scrollWidth = _contentSize.width;
	const float scrollHeight = _contentSize.height;

	const float itemSize = getScrollLength() / _currentSliceLen;
	const float scrollLength = itemSize * _itemsCount;

	const float min = getScrollMinPosition() - _currentSliceStart.get() * itemSize;
	const float max = getScrollMaxPosition()
			+ (_itemsCount - _currentSliceStart.get() - _currentSliceLen) * itemSize;

	const float value = (_scrollPosition - min) / (max - min);

	ScrollView::updateIndicatorPosition(_indicator,
			(isVertical() ? scrollHeight : scrollWidth) / scrollLength, value, true, 20.0f);
}

void DataScroll::onOverscroll(float delta) {
	if (delta > 0 && _currentSliceStart.get() + _currentSliceLen == _itemsCount) {
		ScrollView::onOverscroll(delta);
	} else if (delta < 0 && _currentSliceStart.empty()) {
		ScrollView::onOverscroll(delta);
	}
}

} // namespace stappler::xenolith::material2d
