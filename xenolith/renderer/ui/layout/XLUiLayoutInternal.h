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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTINTERNAL_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTINTERNAL_H_

#include "XLUiLayoutSystem.h" // IWYU pragma: keep

/* Private helpers shared by the layout backends (Flex, Grid, Table subunits of XLUi.scu.cpp).
Helpers are `inline`, not `static`, to avoid unused-function warnings in the single TU.
Not part of the module API: include only from layout/. */

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// True if the node can be measured: an enabled `SystemFlags::HandleMeasure` system or a
// MeasureComponent. A cheap predicate; measureNode() does the actual measuring.
inline bool LayoutSystem_canMeasure(Node *node) {
	for (auto &it : node->getSystems()) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			return true;
		}
	}
	return node->getComponent<MeasureComponent>() != nullptr;
}

// True if the style gives the node a definite size on the axis (MeasureComponent::normal >= 0);
// that CSS `width`/`height` wins over content sizing.
inline bool LayoutSystem_hasDefiniteSize(Node *node, bool horizontal) {
	if (auto mc = node->getComponent<MeasureComponent>()) {
		return (horizontal ? mc->normal.width : mc->normal.height) >= 0.0f;
	}
	return false;
}

// Let every child apply its style; must run before layout inspects the children's systems,
// which the style pass installs.
inline void LayoutSystem_settleChildren(Node *owner) {
	for (auto &child : owner->getChildren()) { child->settleForMeasure(); }
}

// Notify the node's measuring systems that a layout engine committed `size`
// to it (copy the list - handlers mutate node state, e.g. a label re-wraps)
inline void dispatchLayoutApplied(Node *node, const Size2 &size) {
	auto span = node->getSystems();
	Vector<Rc<System>> tmpSystems(span.begin(), span.end());
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			it->handleLayoutApplied(size);
		}
	}
}

// A child's style-requested size, the input to layout: MeasureComponent::normal per axis, or the
// current ContentSize where unspecified (< 0). The style never writes ContentSize, so the
// LayoutSystem stays its only writer.
inline Size2 intrinsicSize(Node *node) {
	Size2 cs = node->getContentSize();
	if (auto mc = node->getComponent<MeasureComponent>()) {
		if (mc->normal.width >= 0.0f) {
			cs.width = mc->normal.width;
		}
		if (mc->normal.height >= 0.0f) {
			cs.height = mc->normal.height;
		}
	}
	return cs;
}

// a resolved track after sizing
struct GridTrackSize {
	GridTrack def;
	float base = 0.0f; // resolved size in px
	float position = 0.0f; // start offset from the content-box start along its axis
};

// One item's demand on a track axis: the tracks it covers and the size it wants. The only input
// track sizing takes from a grid item or table cell.
struct TrackContribution {
	uint32_t start = 0;
	uint32_t span = 1;
	float size = 0.0f;
};

// Resolve track base sizes along one axis: Fixed/Percent from the definition, Auto from covering
// contributions (single-track first, then spanning deficits), Fraction from the remaining free
// space. Defined in XLUiLayoutGrid.cc.
SP_PUBLIC void resolveTrackSizes(Vector<GridTrackSize> &tracks, SpanView<TrackContribution> items,
		float axisContent, float gap);

// Assign each track its start offset along the axis, distributing leftover space according to
// `contentAlign` (justify-content / align-content).
SP_PUBLIC void positionTrackSizes(Vector<GridTrackSize> &tracks, float axisContent, float gap,
		GridAlign contentAlign);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTINTERNAL_H_
