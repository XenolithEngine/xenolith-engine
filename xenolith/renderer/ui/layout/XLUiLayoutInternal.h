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

/* Implementation-private shared leaves of the LayoutSystem subunits.

The layout backends live in one .cc each (XLUiLayoutFlex.cc, XLUiLayoutGrid.cc,
XLUiLayoutTable.cc), all of them included into the module's single translation unit by
XLUi.scu.cpp. What follows is what more than one of them needs, and therefore what cannot stay
file-local to any single one.

Helpers here are `inline` rather than `static` deliberately: with one TU, `static` would emit an
unused-function warning in every subunit that includes the header without calling that particular
helper. This header is not part of the module's API - nothing outside layout/ should include it. */

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Can this node answer the content-measurement protocol at all? Either a system opted into it
// (`SystemFlags::HandleMeasure` — Label's own, a flex container's, or one an application wrote),
// or the precomputed MeasureComponent fallback is present. Both are resolved by measureNode();
// this is only the cheap "is it worth asking" predicate, so it calls nothing.
inline bool LayoutSystem_canMeasure(Node *node) {
	for (auto &it : node->getSystems()) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			return true;
		}
	}
	return node->getComponent<MeasureComponent>() != nullptr;
}

// Does the style give this node a definite size on the axis? That is what the resolver publishes
// in MeasureComponent::normal (a per-axis value < 0 means "unspecified"), and it is the CSS
// `width`/`height` of the item - which wins over content sizing.
inline bool LayoutSystem_hasDefiniteSize(Node *node, bool horizontal) {
	if (auto mc = node->getComponent<MeasureComponent>()) {
		return (horizontal ? mc->normal.width : mc->normal.height) >= 0.0f;
	}
	return false;
}

// Let every child re-derive what its style asks for before anything reads it. A child's systems
// are themselves installed by the style pass, so this has to run before the layout looks at them.
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

// A child's intrinsic (style-requested) size - the INPUT to layout. An explicit per-axis size that
// the style resolver published in a MeasureComponent wins; an axis it left unspecified (value < 0)
// falls back to the child's current ContentSize. Keeping the requested size in a component rather
// than in ContentSize is what lets the LayoutSystem be the SOLE writer of a child's ContentSize,
// breaking the cycle where the style both writes ContentSize and has the layout read it back.
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

// One item's demand on a track axis, as track sizing sees it: which tracks it covers and how big
// it wants to be. It is the entire coupling between an item model (grid item, table cell) and the
// track algorithm, which is why grid and table can share the algorithm without sharing anything
// else.
struct TrackContribution {
	uint32_t start = 0;
	uint32_t span = 1;
	float size = 0.0f;
};

// Resolve track base sizes along one axis: Fixed/Percent from their definition, Auto from the
// contributions covering them (single-track first, then the deficit of spanning ones spread over
// the Auto tracks they cover), Fraction from whatever free space is left. Defined in
// XLUiLayoutGrid.cc - the track vocabulary is grid's - and used by the table backend too.
SP_PUBLIC void resolveTrackSizes(Vector<GridTrackSize> &tracks, SpanView<TrackContribution> items,
		float axisContent, float gap);

// Assign each track its start offset along the axis, distributing leftover space according to
// `contentAlign` (justify-content / align-content).
SP_PUBLIC void positionTrackSizes(Vector<GridTrackSize> &tracks, float axisContent, float gap,
		GridAlign contentAlign);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTINTERNAL_H_
