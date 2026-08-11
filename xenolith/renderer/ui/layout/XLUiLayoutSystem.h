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

	bool operator==(const OutOfFlowComponent &) const = default;
};

// Which layout model the LayoutSystem runs for its owner.
enum class LayoutMode : uint8_t {
	Flex, // CSS Flexible Box, reads FlexLayoutInfo / FlexItemInfo
	Grid, // CSS Grid, reads GridLayoutInfo / GridItemInfo
};

// System that performs flexbox or grid placement for its owner node.
//
// Add it to the container node (the one that also holds the matching
// FlexLayoutInfo / GridLayoutInfo component). The layout is recomputed whenever
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

protected:
	// apply a mutation to the owner's FlexLayoutInfo component, creating it if needed
	void updateInfo(const Callback<bool(FlexLayoutInfo &)> &);

	// the two placement backends, dispatched by `apply()` from `_mode`
	void layoutFlex();
	void layoutGrid();

	LayoutMode _mode = LayoutMode::Flex;
	FlexLayoutInfo _initialInfo;
	GridLayoutInfo _initialGridInfo;

	// guards against self-triggering: while apply() commits child sizes, the
	// resulting handleChildContentSizeDirty notifications are ignored
	bool _inApply = false;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTSYSTEM_H_ */
