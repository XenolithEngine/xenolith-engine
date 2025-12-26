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

#include "XL2dDataScrollHandlerFixed.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool DataScrollHandlerFixed::init(DataScrollView *s, float size) {
	if (!Handler::init(s)) {
		return false;
	}

	_dataSize = size;

	return true;
}

DataScrollView::ItemMap DataScrollHandlerFixed::run(Request t, DataMap &&data) {
	DataScrollView::ItemMap ret;
	Size2 size = (_layout == DataScrollView::Layout::Vertical) ? Size2(_size.width, _dataSize)
															   : Size2(_dataSize, _size.height);
	for (auto &it : data) {
		Vec2 origin = (_layout == DataScrollView::Layout::Vertical)
				? Vec2(0.0f, it.first.get() * _dataSize)
				: Vec2(it.first.get() * _dataSize, 0.0f);

		auto item = Rc<DataScrollView::Item>::create(sp::move(it.second), origin, size);
		ret.insert(std::make_pair(it.first, item));
	}
	return ret;
}

} // namespace stappler::xenolith::basic2d
