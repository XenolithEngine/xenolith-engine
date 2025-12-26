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

#include "MaterialDataScrollHandlerSlice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool DataScrollHandlerSlice::init(DataScroll *s, DataCallback &&cb) {
	if (!Handler::init(s)) {
		return false;
	}

	auto &items = s->getItems();
	if (!items.empty()) {
		_originFront = items.begin()->second->getPosition();
		_originBack = items.rbegin()->second->getPosition();
		_originBack.y += items.rbegin()->second->getContentSize().height;
	}

	_dataCallback = sp::move(cb);

	return true;
}

void DataScrollHandlerSlice::setDataCallback(DataCallback &&cb) { _dataCallback = sp::move(cb); }

DataScroll::ItemMap DataScrollHandlerSlice::run(Request t, DataMap &&data) {
	DataScroll::ItemMap ret;

	auto origin = getOrigin(t);

	if (t == DataScroll::Request::Front) {
		for (auto it = data.rbegin(); it != data.rend(); it++) {
			auto item = onItem(sp::move(it->second), origin);

			item->setPosition((_layout == DataScroll::Layout::Vertical)
							? Vec2(origin.x, origin.y - item->getContentSize().height)
							: Vec2(origin.x - item->getContentSize().height, origin.y));

			if (_layout == DataScroll::Layout::Vertical) {
				origin.y -= item->getContentSize().height;
			} else {
				origin.x -= item->getContentSize().width;
			}

			ret.insert(std::make_pair(it->first, item));
		}
	} else {
		for (auto &it : data) {
			auto item = onItem(sp::move(it.second), origin);

			if (_layout == DataScroll::Layout::Vertical) {
				origin.y += item->getContentSize().height;
			} else {
				origin.x += item->getContentSize().width;
			}

			ret.insert(std::make_pair(it.first, item));
		}
	}
	return ret;
}

Vec2 DataScrollHandlerSlice::getOrigin(Request t) const {
	Vec2 origin = Vec2::ZERO;

	switch (t) {
	case DataScroll::Request::Reset: origin = Vec2::ZERO; break;
	case DataScroll::Request::Update:
	case DataScroll::Request::Front: origin = _originFront; break;
	case DataScroll::Request::Back: origin = _originBack; break;
	}

	return origin;
}

Rc<DataScroll::Item> DataScrollHandlerSlice::onItem(Value &&d, const Vec2 &o) {
	if (_dataCallback) {
		return _dataCallback(this, sp::move(d), o);
	}
	return nullptr;
}

} // namespace stappler::xenolith::material2d
