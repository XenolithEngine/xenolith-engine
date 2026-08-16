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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTSYSTEM_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTSYSTEM_H_

#include "XLUiLayoutFlex.h"
#include "XLUiLayoutGrid.h"
#include "XLUiLayoutTable.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A CSS-inspired placement engine for the ui kit, covering both the
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

// Marker on a direct child: leave it out of the container's flow entirely.
//
// The CSS counterpart is `position: absolute` - an absolutely positioned box is not a flex item
// or a grid item, it does not take part in sizing or in distributing free space, and its
// container behaves as if it were not there. The node is still visited and drawn; only the
// layout ignores it, so whoever placed it (the style resolver's absolute positioning, or the
// application) keeps full control of its position and size.
//
// Written by ui::StyleResolver from `position: absolute`; an overlay built in code can set it
// directly instead of having to live outside the container.
struct SP_PUBLIC OutOfFlowComponent {
	static ComponentId Id;

	// True when ui::StyleResolver added this from `position: absolute`, and therefore the only case
	// in which the resolver may take it away again. Without the distinction a style pass - which
	// runs over EVERY node, whether or not a rule matched it - would strip the marker off any
	// overlay an application set in code, silently putting it back into its container's flow.
	bool styleManaged = false;

	bool operator==(const OutOfFlowComponent &) const = default;
};

// Resolved CSS `overflow-x` / `overflow-y` for a node.
//
// Written by ui::StyleResolver, read by LayoutSystem (which axes may exceed the box) and by
// ui::ScrollSystem (what to clip and what to slide).
//
// The two axes are already reconciled by the time they land here: CSS computes a `visible` axis to
// `auto` when the other one is not `visible`, and this engine has no say in the matter - the only
// clip it has is an axis-aligned scissor RECT, which cannot clip one axis and leave the other
// alone.
struct SP_PUBLIC OverflowComponent {
	static ComponentId Id;

	document::Overflow x = document::Overflow::Visible;
	document::Overflow y = document::Overflow::Visible;

	// Same contract as OutOfFlowComponent::styleManaged: only a component the resolver created may
	// the resolver take away, so overflow set in code survives a pass that matched nothing.
	bool styleManaged = false;

	bool clipsX() const { return x != document::Overflow::Visible; }
	bool clipsY() const { return y != document::Overflow::Visible; }

	// `hidden`/`clip` are clipped but not scrollable; only `scroll`/`auto` slide.
	bool scrollsX() const {
		return x == document::Overflow::Scroll || x == document::Overflow::Auto;
	}
	bool scrollsY() const {
		return y == document::Overflow::Scroll || y == document::Overflow::Auto;
	}

	bool operator==(const OverflowComponent &) const = default;
};

// Which layout model the LayoutSystem runs for its owner.
enum class LayoutMode : uint8_t {
	Flex, // CSS Flexible Box, reads FlexLayoutInfo / FlexItemInfo
	Grid, // CSS Grid, reads GridLayoutInfo / GridItemInfo
	Table, // CSS table container, reads TableLayoutInfo / TableRowInfo; writes TableColumnsComponent
	TableRow, // one table row, reads the TableColumnsComponent on its own node + TableCellInfo
};

// System that performs flexbox, grid or table placement for its owner node.
//
// Add it to the container node (the one that also holds the matching
// FlexLayoutInfo / GridLayoutInfo / TableLayoutInfo component). The layout is recomputed whenever
// the container is resized, its children are added / removed / reordered, or its
// components change.
class SP_PUBLIC LayoutSystem : public System {
public:
	// should be after styling
	static constexpr uint32_t LayoutDefaultPriority = System::DefaultPriority - 100;

	// Frame-stack tag: the container publishes itself here so descendants deliver their
	// content-size changes to the nearest ancestor LayoutSystem (fit-content invalidation)
	static uint64_t SystemFrameTag;

	virtual ~LayoutSystem() = default;

	virtual bool init() override; // defaults to flex mode

	// initialize into flex mode and assign the container parameters in one step
	virtual bool init(const FlexLayoutInfo &);

	// initialize into grid mode and assign the container parameters in one step
	virtual bool init(const GridLayoutInfo &);

	// initialize into table mode and assign the container parameters in one step
	virtual bool init(const TableLayoutInfo &);

	// initialize into table-row mode; the columns arrive later, via setTableColumns()
	virtual bool init(LayoutMode);

	virtual void handleAdded(Node *owner) override;

	virtual void handleComponentsDirty(const ComponentMask &) override;

	// content measurement protocol: report the container's natural size by
	// dry-running the flex algorithm over the children (grid: no answer in v1,
	// the node's current content size is used instead)
	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result) override;

	// a descendant resized itself (e.g. a label re-shaped after a text change), delivered via the
	// frame stack: schedule a coalesced re-layout on the next visit. Nested fit-content chains
	// re-measure automatically - the container's own resize is delivered to its ancestor container
	// via the frame stack during the container's own visit, so no manual re-bubble is needed
	virtual void handleChildContentSizeDirty(Node *) override;
	virtual void handleChildComponentsDirty(Node *, const ComponentMask &) override;

	// layout-children phase: position/size the children (own size + order fixed)
	virtual void handleLayoutChildren() override;

	// recompute the placement of all children for the current geometry
	void apply();

	// --- overflow / scrolling ----------------------------------------------
	// The union of the in-flow children's margin boxes plus the container padding, as the last pass
	// placed them, WITHOUT the scroll offset. Size2::ZERO before the first pass.
	//
	// It is the size the content OCCUPIES, so it can be either larger than the owner's ContentSize
	// (that surplus is what ui::ScrollSystem turns into a scroll range) or smaller (that shortfall
	// is the room left over, which a caller may want to give to something else).
	Size2 getContentExtent() const { return _contentExtent; }

	// Which axes the pass may exceed the box on. On such an axis the content is laid out at its
	// natural size instead of being squeezed into the box: a measured base size is not truncated to
	// the available space, the axis does not wrap, and flex-shrink does not crush the items. That
	// last one stands in for CSS's automatic minimum size (`min-height: auto` == min-content on a
	// flex item), which this engine has never implemented - without it the default shrink of 1
	// would simply squash the content and there would be nothing left to scroll.
	//
	// Written by ui::ScrollSystem from the OverflowComponent.
	void setOverflowAxes(bool horizontal, bool vertical);
	bool isOverflowX() const { return _overflowX; }
	bool isOverflowY() const { return _overflowY; }

	// Translation applied to every in-flow child on top of its placement, in CSS scroll orientation
	// (x grows right, y grows DOWN - the engine's own Y grows up). Cheap: it replays the placement
	// the last pass cached instead of re-running the algorithm, so a wheel tick costs one
	// setPosition per child and no measurement.
	void setScrollOffset(Vec2);
	Vec2 getScrollOffset() const { return _scrollOffset; }

	// measure the container's natural content size under the given constraints
	// without committing anything (dry-run of the flex pass; grow/shrink are
	// ignored, as in CSS content sizing)
	Size2 measure(const MeasureConstraints &);

	// ask an arbitrary node for its natural content size via the measurement
	// protocol: the first system with SystemFlags::HandleMeasure answers;
	// nodes without one report their current content size (the same value the
	// legacy flex-basis:auto fallback reads)
	static Size2 measureNode(Node *, const MeasureConstraints &);

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

	// --- table mode --------------------------------------------------------
	// access / replace the table container parameters (owner component)
	const TableLayoutInfo *getTableInfo() const;
	void setTableInfo(const TableLayoutInfo &);

	// helpers to read / assign per-row and per-cell parameters via the component system
	static const TableRowInfo *getTableRow(NotNull<Node>);
	static void setTableRow(NotNull<Node>, const TableRowInfo &);
	static const TableCellInfo *getTableCell(NotNull<Node>);
	static void setTableCell(NotNull<Node>, const TableCellInfo &);

	// Impose a resolved column geometry on a row. This is the channel a virtualized view
	// (ui::TableView) uses to make a row that has no table ancestor lay itself out, and the one a
	// LayoutMode::Table pass uses on its own row children - the same call in both cases.
	//
	// `generation` is carried over and bumped only when the geometry actually differs, so an
	// unchanged pass neither re-lays-out the row nor invalidates a view's node reuse.
	static void setTableColumns(NotNull<Node>, const TableColumnsComponent &);

protected:
	// apply a mutation to the owner's FlexLayoutInfo component, creating it if needed
	void updateInfo(const Callback<bool(FlexLayoutInfo &)> &);

	// give the owner the container component the current mode reads, if it has none yet
	void ensureModeComponent(Node *owner);

	// Fallback content extent for the modes whose backend does not publish one: the union of the
	// in-flow children's boxes, with the scroll offset added back.
	Size2 measureChildrenExtent() const;

	// The placement backends, dispatched by `apply()` from `_mode`. One per subunit of the module's
	// SCU: layoutFlex in XLUiLayoutFlex.cc, layoutGrid in XLUiLayoutGrid.cc, the two table modes in
	// XLUiLayoutTable.cc.
	void layoutFlex();
	void layoutGrid();
	void layoutTable();
	void layoutTableRow();

	// Per-mode measurement, dispatched by `measure()` from `_mode`, and defined beside the matching
	// backend. The null-owner guard and the mode switch belong to the dispatcher, so each of these
	// runs with an owner that exists and a mode that already matches.
	Size2 measureFlex(const MeasureConstraints &);
	Size2 measureTable(const MeasureConstraints &);
	Size2 measureTableRow(const MeasureConstraints &);

	LayoutMode _mode = LayoutMode::Flex;
	FlexLayoutInfo _initialInfo;
	GridLayoutInfo _initialGridInfo;
	TableLayoutInfo _initialTableInfo;

	// guards against self-triggering: while apply() commits child sizes, the
	// resulting handleChildContentSizeDirty notifications are ignored
	bool _inApply = false;

	// Unscrolled bottom-left of every in-flow child from the last pass, in owner space, so
	// setScrollOffset can re-place them without re-running the algorithm. Rc rather than a raw
	// pointer: a child removed between a pass and a scroll would otherwise dangle; setScrollOffset
	// still re-checks getParent(), because a re-parented node must not be moved either.
	Vector<Pair<Rc<Node>, Vec2>> _placement;

	Size2 _contentExtent;
	Vec2 _scrollOffset;
	bool _overflowX = false;
	bool _overflowY = false;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTSYSTEM_H_ */
