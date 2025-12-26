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

#include "MaterialDataScrollHandlerGrid.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool DataScrollHandlerGrid::init(DataScroll *s) {
	if (!Handler::init(s)) {
		return false;
	}

	_padding = s->getPadding();

	return true;
}

bool DataScrollHandlerGrid::init(DataScroll *s, const Padding &p) {
	if (!Handler::init(s)) {
		return false;
	}

	_padding = p;

	return true;
}

DataScroll::ItemMap DataScrollHandlerGrid::run(Request t, DataMap &&data) {
	DataScroll::ItemMap ret;
	auto size = _size;
	size.width -= (_padding.left + _padding.right);

	uint32_t cols = uint32_t(floorf(size.width / _cellMinWidth));
	if (cols == 0) {
		cols = 1;
	}

	auto cellWidth = (_autoPaddings?(std::min(_cellMinWidth, size.width / cols)):(size.width / cols));
	auto cellHeight = (_fixedHeight?_cellHeight:cellWidth / _cellAspectRatio);

	_currentCellSize = Size2(cellWidth, cellHeight);
	_currentCols = uint32_t(cols);
	_widthPadding = (_size.width - _currentCellSize.width * _currentCols) / 2.0f;

	for (auto &it : data) {
		auto item = onItem(sp::move(it.second), it.first);
		ret.insert(std::make_pair(it.first, item));
	}
	return ret;
}

void DataScrollHandlerGrid::setCellMinWidth(float v) {
	_cellMinWidth = v;
}

void DataScrollHandlerGrid::setCellAspectRatio(float v) {
	_cellAspectRatio = v;
	_fixedHeight = false;
}
void DataScrollHandlerGrid::setCellHeight(float v) {
	_cellHeight = v;
	_fixedHeight = true;
}

void DataScrollHandlerGrid::setAutoPaddings(bool value) {
	_autoPaddings = value;
}

bool DataScrollHandlerGrid::isAutoPaddings() const {
	return _autoPaddings;
}

Rc<DataScroll::Item> DataScrollHandlerGrid::onItem(Value &&data, DataSource::Id id) {
	DataSource::Id::Type row = id.get() / _currentCols;
	DataSource::Id::Type col = id.get() % _currentCols;

	Vec2 pos(col * _currentCellSize.width + _widthPadding, row * _currentCellSize.height);

	return Rc<DataScroll::Item>::create(sp::move(data), pos, _currentCellSize);
}

}
