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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTGRID_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTGRID_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "SPDocStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// CSS `grid-auto-flow`: direction and packing of auto-placed items.
using stappler::document::GridAutoFlow;

// CSS Box Alignment keyword, reused for grid justify/align of both content
// (whole grid within the container) and items/self (item within its cell).
// Not every value is meaningful in every context.
enum class GridAlign : uint8_t {
	Auto, // *-self only: inherit the container's justify/align-items
	Start,
	End,
	Center,
	Stretch,
	SpaceBetween, // *-content only
	SpaceAround, // *-content only
	SpaceEvenly, // *-content only
};

// A single resolved grid track (column or row). Track lists are stored already
// expanded (repeat() flattened) - see parseGridTemplate below.
struct GridTrack {
	enum Type : uint8_t {
		Fixed, // absolute length, `value` is px
		Percent, // percentage of the container content box, `value` is 0..100
		Fraction, // flexible `fr` unit, `value` is the fr count
		Auto, // content-sized track, `value` unused
	} type = Auto;
	float value = 0.0f;

	bool operator==(const GridTrack &) const = default;
	bool operator!=(const GridTrack &) const = default;
};

// Component attached to the *container* node in grid mode. Describes the shared
// grid parameters, like the properties set on a CSS grid container.
struct SP_PUBLIC GridLayoutInfo {
	static ComponentId Id;

	// explicit track lists (already repeat()-expanded); empty -> implicit only
	Vector<GridTrack> columnTracks;
	Vector<GridTrack> rowTracks;

	// direction and packing of auto-placed items
	GridAutoFlow autoFlow = GridAutoFlow::Row;

	// default sizing of implicit tracks created by auto-placement
	GridTrack autoColumn;
	GridTrack autoRow;

	// gap between columns (`column-gap`) and between rows (`row-gap`)
	float columnGap = 0.0f;
	float rowGap = 0.0f;

	// inner padding of the container's content box
	Padding padding;

	// distribution of the whole grid within the content box when it is smaller
	// (justify == columns / X axis, align == rows / Y axis)
	GridAlign justifyContent = GridAlign::Start;
	GridAlign alignContent = GridAlign::Start;

	// default per-item alignment within its cell (may be overridden per item)
	GridAlign justifyItems = GridAlign::Stretch;
	GridAlign alignItems = GridAlign::Stretch;

	bool operator==(const GridLayoutInfo &) const = default;
	bool operator!=(const GridLayoutInfo &) const = default;
};

// Component attached to a *direct child* of the container in grid mode.
struct SP_PUBLIC GridItemInfo {
	static ComponentId Id;

	// sentinel for "use the node's own content size"
	static constexpr float Auto = -1.0f;

	// line placement: 0 == auto, N >= 1 == explicit 1-based grid line.
	// `*Span` is used when the matching end is auto.
	uint32_t gridColumnStart = 0;
	uint32_t gridColumnEnd = 0;
	uint32_t gridRowStart = 0;
	uint32_t gridRowEnd = 0;
	uint32_t columnSpan = 1;
	uint32_t rowSpan = 1;

	// per-item override of the container's justify/align-items (Auto inherits)
	GridAlign justifySelf = GridAlign::Auto;
	GridAlign alignSelf = GridAlign::Auto;

	// explicit item size; Auto -> the node's current content size
	float width = Auto;
	float height = Auto;

	// outer margin around the item, kept inside its cell
	Padding margin;

	// placement order within the container (CSS `order`); lower comes first
	int32_t order = 0;

	bool operator==(const GridItemInfo &) const = default;
	bool operator!=(const GridItemInfo &) const = default;
};

// Parse a CSS `grid-template-columns` / `grid-template-rows` track list into a
// flat GridTrack vector. Supports `<length>`, `<percentage>`, `<number>fr`,
// `auto`, and `repeat(<count>, <tracks>)`. Named lines / minmax() / fit-content()
// / auto-fill|auto-fit are not supported (silently skipped). An empty or
// unparseable input yields an empty list.
SP_PUBLIC Vector<GridTrack> parseGridTemplate(StringView);

// Parse a single CSS grid line placement string of the form `N`, `span N`, or
// `N / M` into 1-based start/end lines (0 == auto) and a span count. Returns
// false and leaves the outputs untouched on a fully empty / unparseable input.
SP_PUBLIC bool parseGridLine(StringView, uint32_t &start, uint32_t &end, uint32_t &span);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTGRID_H_
