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

/** A CSS-inspired placement engine for the ui kit: flexbox, grid and table layout.

The container node carries a `FlexLayoutInfo` / `GridLayoutInfo` / `TableLayoutInfo` component;
each direct child may carry the matching item component (children without one get the defaults).
`LayoutSystem` on the container reads them and places the children; `LayoutMode` selects the model.

Xenolith's origin is bottom-left (y up); the engine compensates, so `FlexStart` on the main axis is
left for rows and top for columns, and grid row 0 is the top row, as in CSS. */

// Marker on a direct child: the container's layout ignores it (CSS `position: absolute`). The node
// is still visited and drawn; its position and size stay with whoever placed it. Written by
// ui::StyleResolver from `position: absolute`, or set directly by code.
struct SP_PUBLIC OutOfFlowComponent {
	static ComponentId Id;

	// True when ui::StyleResolver added this; only then may the resolver remove it, so a marker set
	// in code survives style passes.
	bool styleManaged = false;

	bool operator==(const OutOfFlowComponent &) const = default;
};

// Resolved CSS `overflow-x` / `overflow-y` for a node. Written by ui::StyleResolver, read by
// LayoutSystem (which axes may exceed the box) and ui::ScrollSystem (what to clip and slide).
// The axes are independent: unlike the web, a `visible` axis is not computed to `auto` when the
// other one clips (the scissor is built per axis, see ui::ScissorAxes).
struct SP_PUBLIC OverflowComponent {
	static ComponentId Id;

	document::Overflow x = document::Overflow::Visible;
	document::Overflow y = document::Overflow::Visible;

	// Same contract as OutOfFlowComponent::styleManaged.
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
	Table, // CSS table, reads TableLayoutInfo / TableRowInfo; writes TableColumnsComponent
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
	// placed them, without the scroll offset. Size2::ZERO before the first pass. May be larger than
	// the owner's ContentSize (the scroll range) or smaller (the room left over).
	Size2 getContentExtent() const { return _contentExtent; }

	// Which axes the pass may exceed the box on. On such an axis content keeps its natural size:
	// base sizes are not truncated, the axis does not wrap, and flex-shrink is off (standing in for
	// CSS's automatic minimum size, which is not implemented). Written by ui::ScrollSystem.
	void setOverflowAxes(bool horizontal, bool vertical);
	bool isOverflowX() const { return _overflowX; }
	bool isOverflowY() const { return _overflowY; }

	// Translation applied to every in-flow child on top of its placement, in CSS scroll orientation
	// (y grows down). Replays the cached placement: one setPosition per child, no measurement.
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
	static Size2 measureItem(Node *, const MeasureConstraints &, bool parentIsRow);

	/* True when the node really answers a measurement: a system with `SystemFlags::HandleMeasure`
	or a `MeasureComponent`. Otherwise `measureNode` just echoes the current ContentSize, which must
	not be used to decide content sizing. Measures nothing. */
	static bool canMeasure(NotNull<Node>);

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

	static void markItemDirty(NotNull<Node>);

	/* The node's intrinsic size changed: dirties every `fit-content` ancestor that measured it,
	not just the one level `markItemDirty` does. */
	static void markMeasureDirty(NotNull<Node>);

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

	// Imposes a resolved column geometry on a row; used by LayoutMode::Table on its rows and by a
	// virtualized view (ui::TableView) whose rows have no table ancestor. `generation` is bumped
	// only when the geometry differs, so an unchanged pass keeps the row and a view's node reuse.
	static void setTableColumns(NotNull<Node>, const TableColumnsComponent &);

protected:
	// apply a mutation to the owner's FlexLayoutInfo component, creating it if needed
	void updateInfo(const Callback<bool(FlexLayoutInfo &)> &);

	// give the owner the container component the current mode reads, if it has none yet
	void ensureModeComponent(Node *owner);

	// Fallback content extent for the modes whose backend does not publish one: the union of the
	// in-flow children's boxes, with the scroll offset added back.
	Size2 measureChildrenExtent() const;

	// Placement backends, dispatched by `apply()` from `_mode`; defined in XLUiLayoutFlex.cc,
	// XLUiLayoutGrid.cc and XLUiLayoutTable.cc.
	void layoutFlex();
	void layoutGrid();
	void layoutTable();
	void layoutTableRow();

	// Per-mode measurement, dispatched by `measure()`, which already checked the owner and mode.
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

	// Unscrolled bottom-left of every in-flow child from the last pass, in owner space, replayed by
	// setScrollOffset. Rc keeps removed children alive; setScrollOffset still checks getParent().
	Vector<Pair<Rc<Node>, Vec2>> _placement;

	Size2 _contentExtent;
	Vec2 _scrollOffset;
	bool _overflowX = false;
	bool _overflowY = false;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTSYSTEM_H_ */
