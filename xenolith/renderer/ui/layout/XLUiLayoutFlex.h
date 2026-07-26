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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTFLEX_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTFLEX_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "SPDocStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::document::FlexDirection;
using stappler::document::FlexWrap;

// CSS `justify-content`: distribution of free space along the main axis.
enum class FlexJustify : uint8_t {
	FlexStart,
	FlexEnd,
	Center,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly,
};

// CSS `align-items` / `align-self` / `align-content`: cross-axis alignment.
// Not every value is meaningful in every context (see member docs below).
enum class FlexAlign : uint8_t {
	Auto, // align-self only: inherit the container's `alignItems`
	FlexStart,
	FlexEnd,
	Center,
	Stretch,
	SpaceBetween, // align-content only
	SpaceAround, // align-content only
};

// Component attached to the *container* node in flex mode. Describes the
// parameters shared by every item, exactly like a CSS flex container.
struct SP_PUBLIC FlexLayoutInfo {
	static ComponentId Id;

	FlexDirection direction = FlexDirection::Row;
	FlexWrap wrap = FlexWrap::NoWrap;

	// distribution of items along the main axis
	FlexJustify justifyContent = FlexJustify::FlexStart;

	// alignment of items within their line (cross axis)
	FlexAlign alignItems = FlexAlign::Stretch;

	// alignment of the lines themselves when wrapping is enabled (cross axis)
	FlexAlign alignContent = FlexAlign::FlexStart;

	// gap between columns (`column-gap`) and between rows (`row-gap`)
	float columnGap = 0.0f;
	float rowGap = 0.0f;

	// inner padding of the container's content box (top, right, bottom, left)
	Padding padding;

	bool operator==(const FlexLayoutInfo &) const = default;
	bool operator!=(const FlexLayoutInfo &) const = default;
};

// Component attached to a *direct child* of the container in flex mode.
// Describes the per-node properties, like a CSS flex item.
struct SP_PUBLIC FlexItemInfo {
	static ComponentId Id;

	// sentinel for `flex-basis: auto` and for "no maximum" main size
	static constexpr float Auto = -1.0f;

	// sentinel for `fit-content`: the size is measured from the node's actual
	// content via the measurement protocol (System::handleMeasure) instead of
	// being read from the node's current content size.
	// - basis == FitContent: main size = min(max-content, available main),
	//   clamped to [minMain, maxMain] (the `max(min-content, ...)` floor of the
	//   CSS formula is not derived automatically - use minMain for it);
	// - crossSize == FitContent: the hypothetical cross size is re-measured
	//   with the item's final main size as the constraint (e.g. a wrapped
	//   label: width -> resulting height). Implied when basis == FitContent
	//   and crossSize == Auto; `Stretch` alignment still overrides it, same
	//   as it overrides an explicit crossSize.
	static constexpr float FitContent = -2.0f;

	float grow = 0.0f; // `flex-grow`: share of positive free space
	float shrink = 1.0f; // `flex-shrink`: share of negative free space
	float basis = Auto; // `flex-basis`: main size before flexing; Auto -> node's content size

	// Definite cross-axis size of the item (the analog of `height` for a row or
	// `width` for a column). Auto -> the item keeps its node's current cross size
	// for non-stretch alignment and fills the line when stretched.
	float crossSize = Auto;

	// per-item override of the container's `alignItems`
	FlexAlign alignSelf = FlexAlign::Auto;

	// visual order within the container (CSS `order`); lower comes first
	int32_t order = 0;

	float minMain = 0.0f; // minimal resolved main size
	float maxMain = Auto; // maximal resolved main size; Auto -> unbounded

	// outer margin around the item, kept outside of its flex base size
	// (Padding and Margin are the same geometry type: top, right, bottom, left)
	Padding margin;

	bool operator==(const FlexItemInfo &) const = default;
	bool operator!=(const FlexItemInfo &) const = default;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTFLEX_H_
