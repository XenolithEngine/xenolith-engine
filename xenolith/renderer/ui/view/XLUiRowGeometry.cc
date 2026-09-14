/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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


#include "XLUiRowGeometry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

/* The row's box in the scroll's own space. The root is anchored top-left at
`scrollPosition + scrollSize` and a row sits at `-pos.y` in it, so the row's top edge is
`scrollPosition + scrollSize - pos.y` and the box grows downward. */
static bool RowGeometry_boxInScroll(const RowGeometrySource &source,
		const basic2d::ScrollController::Item &item, Vec2 &low, Vec2 &high) {
	auto root = source.scroll->getRoot();
	if (!root) {
		return false;
	}

	const float top =
			source.scroll->getScrollPosition() + source.scroll->getScrollSize() - item.pos.y;
	const float height = item.size.height;
	if (sprt::isnan(top) || sprt::isnan(height)) {
		return false;
	}

	// The width comes from the root: an item's width is nan() until a node is built for it.
	const float left = root->getPosition().x;
	const float width = root->getContentSize().width;

	low = Vec2(left, top - height);
	high = Vec2(left + width, top);
	return true;
}

// The scroll offset a point in the view's space corresponds to: distance from the top of the
// content, which is the space Item::pos lives in.
static bool RowGeometry_offsetAt(const RowGeometrySource &source, const Vec2 &viewLocation,
		float &out) {
	auto scrollLocal =
			source.scroll->convertToNodeSpace(source.view->convertToWorldSpace(viewLocation));
	out = source.scroll->getScrollPosition() + source.scroll->getScrollSize() - scrollLocal.y;
	return !sprt::isnan(out);
}

static Rect RowGeometry_toViewSpace(const RowGeometrySource &source, Vec2 low, Vec2 high) {
	// Converted through world space: the scroll and the view need not share an origin.
	const Vec2 corners[4] = {
		source.view->convertToNodeSpace(source.scroll->convertToWorldSpace(low)),
		source.view->convertToNodeSpace(source.scroll->convertToWorldSpace(Vec2(high.x, low.y))),
		source.view->convertToNodeSpace(source.scroll->convertToWorldSpace(Vec2(low.x, high.y))),
		source.view->convertToNodeSpace(source.scroll->convertToWorldSpace(high)),
	};

	Vec2 outLow = corners[0];
	Vec2 outHigh = corners[0];
	for (uint32_t i = 1; i < 4; ++i) {
		outLow.x = sprt::min(outLow.x, corners[i].x);
		outLow.y = sprt::min(outLow.y, corners[i].y);
		outHigh.x = sprt::max(outHigh.x, corners[i].x);
		outHigh.y = sprt::max(outHigh.y, corners[i].y);
	}
	return Rect(outLow.x, outLow.y, outHigh.x - outLow.x, outHigh.y - outLow.y);
}

} // namespace

size_t RowGeometrySource::getRowCount() const {
	return controller ? controller->getItems().size() : 0;
}

bool getRowRect(const RowGeometrySource &source, size_t index, Rect &out) {
	if (source.empty()) {
		return false;
	}

	auto &items = source.controller->getItems();
	if (index >= items.size()) {
		return false;
	}

	Vec2 low, high;
	if (!RowGeometry_boxInScroll(source, items.at(index), low, high)) {
		return false;
	}

	out = RowGeometry_toViewSpace(source, low, high);
	return out.size.height > 0.0f;
}

size_t getRowIndexAt(const RowGeometrySource &source, const Vec2 &viewLocation) {
	if (source.empty()) {
		return maxOf<size_t>();
	}

	auto &items = source.controller->getItems();
	if (items.empty()) {
		return maxOf<size_t>();
	}

	float offset = 0.0f;
	if (!RowGeometry_offsetAt(source, viewLocation, offset) || offset < 0.0f) {
		return maxOf<size_t>();
	}

	// Binary search: items are ordered by pos.y, and a drag queries this on every pointer move.
	size_t low = 0;
	size_t high = items.size();
	while (low < high) {
		const size_t mid = low + (high - low) / 2;
		auto &item = items.at(mid);
		if (offset < item.pos.y) {
			high = mid;
		} else if (offset >= item.pos.y + item.size.height) {
			low = mid + 1;
		} else {
			return mid;
		}
	}
	return maxOf<size_t>();
}

bool getRowBoundaryRect(const RowGeometrySource &source, size_t boundary, Rect &out,
		float thickness) {
	if (source.empty()) {
		return false;
	}

	auto &items = source.controller->getItems();
	if (items.empty() || boundary > items.size()) {
		return false;
	}

	// The top edge of row `boundary`, or the bottom edge of the last row when one past the end,
	// as a zero-height box that is inflated after conversion.
	Vec2 low, high;
	if (boundary < items.size()) {
		if (!RowGeometry_boxInScroll(source, items.at(boundary), low, high)) {
			return false;
		}
		low.y = high.y; // the boundary is this row's top edge
	} else {
		if (!RowGeometry_boxInScroll(source, items.at(items.size() - 1), low, high)) {
			return false;
		}
		high.y = low.y; // one past the end: the last row's bottom edge
	}

	auto rect = RowGeometry_toViewSpace(source, low, high);
	rect.origin.y -= thickness / 2.0f;
	rect.size.height = thickness;
	out = rect;
	return true;
}

size_t getRowBoundaryAt(const RowGeometrySource &source, const Vec2 &viewLocation,
		Rect *boundaryRect, float thickness) {
	if (source.empty()) {
		return maxOf<size_t>();
	}

	auto &items = source.controller->getItems();
	if (items.empty()) {
		return maxOf<size_t>();
	}

	float offset = 0.0f;
	if (!RowGeometry_offsetAt(source, viewLocation, offset)) {
		return maxOf<size_t>();
	}

	size_t boundary = 0;
	if (offset <= 0.0f) {
		boundary = 0;
	} else {
		auto &last = items.at(items.size() - 1);
		if (offset >= last.pos.y + last.size.height) {
			boundary = items.size();
		} else {
			const size_t index = getRowIndexAt(source, viewLocation);
			if (index == maxOf<size_t>()) {
				return maxOf<size_t>();
			}
			// The half of the row the pointer is in decides which side it lands on.
			auto &item = items.at(index);
			boundary = (offset < item.pos.y + item.size.height / 2.0f) ? index : index + 1;
		}
	}

	if (boundaryRect) {
		getRowBoundaryRect(source, boundary, *boundaryRect, thickness);
	}
	return boundary;
}

} // namespace stappler::xenolith::ui
