/**
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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLELAYOUTSYSTEM_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLELAYOUTSYSTEM_H_

#include "XLSimpleUiConfig.h" // IWYU pragma: keep
#include "XLNode.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

/** A CSS-inspired placement engine for the simpleui kit, covering both the
Flexible Box (flexbox) and Grid layout models.

The layout is driven entirely through the engine's component system:
- the container node carries a `FlexLayoutInfo` (flex mode) or a `GridLayoutInfo`
  (grid mode) component, describing the parameters shared by all items - the
  analog of the CSS flex/grid container properties;
- every direct child may carry a `FlexItemInfo` / `GridItemInfo` component,
  describing the per-node properties (grow/shrink/basis, order, alignment,
  placement, ...) - the analog of the CSS flex/grid item properties. Children
  without the component are laid out with the default item parameters.

The `LayoutSystem` system, attached to the container node, reads those
components and positions/sizes the children accordingly. `LayoutMode` selects
which model runs. The terminology mirrors the CSS specifications as closely as
the 2d node model allows.

Note that Xenolith uses a bottom-left coordinate origin (Y axis points up),
while CSS uses a top-left origin. The engine compensates internally, so
`FlexStart` on the main axis means "left" for rows and "top" for columns, and
grid row 0 is the top row, just like in CSS. */

// CSS `flex-direction`: orientation of the main axis and the flow direction.
enum class FlexDirection : uint8_t {
	Row, // main axis is horizontal, items flow left -> right
	RowReverse, // main axis is horizontal, items flow right -> left
	Column, // main axis is vertical, items flow top -> bottom
	ColumnReverse, // main axis is vertical, items flow bottom -> top
};

// CSS `flex-wrap`: whether items are forced onto a single line or may wrap.
enum class FlexWrap : uint8_t {
	NoWrap, // single line, items may overflow / shrink
	Wrap, // multiple lines, new lines are added after the cross-start one
	WrapReverse, // multiple lines, new lines are added before the cross-start one
};

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

// Which layout model the LayoutSystem runs for its owner.
enum class LayoutMode : uint8_t {
	Flex, // CSS Flexible Box, reads FlexLayoutInfo / FlexItemInfo
	Grid, // CSS Grid, reads GridLayoutInfo / GridItemInfo
};

// CSS `grid-auto-flow`: direction and packing of auto-placed items.
enum class GridAutoFlow : uint8_t {
	Row, // fill rows first (columns are the fixed/minor axis), sparse packing
	Column, // fill columns first (rows are the minor axis), sparse packing
	RowDense, // as Row, but backfill earlier holes
	ColumnDense, // as Column, but backfill earlier holes
};

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

// System that performs flexbox or grid placement for its owner node.
//
// Add it to the container node (the one that also holds the matching
// FlexLayoutInfo / GridLayoutInfo component). The layout is recomputed whenever
// the container is resized, its children are added / removed / reordered, or its
// components change.
class SP_PUBLIC LayoutSystem : public System {
public:
	virtual ~LayoutSystem() = default;

	virtual bool init() override; // defaults to flex mode

	// initialize into flex mode and assign the container parameters in one step
	virtual bool init(const FlexLayoutInfo &);

	// initialize into grid mode and assign the container parameters in one step
	virtual bool init(const GridLayoutInfo &);

	virtual void handleAdded(Node *owner) override;

	virtual void handleComponentsDirty() override;

	// layout-children phase: position/size the children (own size + order fixed)
	virtual void handleLayoutChildren() override;

	// recompute the placement of all children for the current geometry
	void apply();

	LayoutMode getMode() const { return _mode; }
	void setMode(LayoutMode);

	// --- flex mode ---------------------------------------------------------
	// access / replace the flex container parameters (owner component)
	const FlexLayoutInfo *getInfo() const;
	void setInfo(const FlexLayoutInfo &);

	// convenience mutators for individual flex container parameters
	void setDirection(FlexDirection);
	void setWrap(FlexWrap);
	void setJustifyContent(FlexJustify);
	void setAlignItems(FlexAlign);
	void setAlignContent(FlexAlign);
	void setGap(float row, float column);
	void setPadding(Padding);

	// helpers to read / assign per-item flex parameters via the component system
	static const FlexItemInfo *getItem(NotNull<Node>);
	static void setItem(NotNull<Node>, const FlexItemInfo &);

	// --- grid mode ---------------------------------------------------------
	// access / replace the grid container parameters (owner component)
	const GridLayoutInfo *getGridInfo() const;
	void setGridInfo(const GridLayoutInfo &);

	// helpers to read / assign per-item grid parameters via the component system
	static const GridItemInfo *getGridItem(NotNull<Node>);
	static void setGridItem(NotNull<Node>, const GridItemInfo &);

protected:
	// apply a mutation to the owner's FlexLayoutInfo component, creating it if needed
	void updateInfo(const Callback<bool(FlexLayoutInfo &)> &);

	// the two placement backends, dispatched by `apply()` from `_mode`
	void layoutFlex();
	void layoutGrid();

	LayoutMode _mode = LayoutMode::Flex;
	FlexLayoutInfo _initialInfo;
	GridLayoutInfo _initialGridInfo;
};

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLELAYOUTSYSTEM_H_ */
