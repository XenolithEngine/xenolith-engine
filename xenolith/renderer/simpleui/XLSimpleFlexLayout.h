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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEFLEXLAYOUT_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEFLEXLAYOUT_H_

#include "XLSimpleUiConfig.h" // IWYU pragma: keep
#include "XLNode.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

/** A CSS-flexbox-inspired placement engine for the simpleui kit.

The layout is driven entirely through the engine's component system:
- the container node carries a `FlexLayoutInfo` component, which describes the
  parameters shared by all items (direction, wrapping, content distribution,
  gaps, padding) - this is the analog of the CSS flex container properties;
- every direct child may carry a `FlexItemInfo` component, which describes the
  per-node properties (grow/shrink factors, basis, self-alignment, order,
  margins) - this is the analog of the CSS flex item properties. Children
  without the component are laid out with the default item parameters.

The `FlexLayout` system, attached to the container node, reads those components
and positions/sizes the children accordingly. The terminology mirrors the CSS
Flexible Box Layout specification as closely as the 2d node model allows.

Note that Xenolith uses a bottom-left coordinate origin (Y axis points up),
while CSS uses a top-left origin. The engine compensates internally, so
`FlexStart` on the main axis means "left" for rows and "top" for columns, just
like in CSS. */

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

// Component attached to the *container* node. Describes the parameters that are
// shared by every item, exactly like the properties set on a CSS flex container.
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

// Component attached to a *direct child* of the container. Describes the
// per-node properties, like the properties set on a CSS flex item.
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

// System that performs the flexbox placement for its owner node.
//
// Add it to the container node (the one that also holds the `FlexLayoutInfo`
// component). The layout is recomputed whenever the container is resized, its
// children are added / removed / reordered, or its components change.
class SP_PUBLIC FlexLayout : public System {
public:
	virtual ~FlexLayout() = default;

	virtual bool init() override;

	// initialize and assign the container parameters in one step
	virtual bool init(const FlexLayoutInfo &);

	virtual void handleAdded(Node *owner) override;

	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty() override;
	virtual void handleReorderChildDirty() override;

	// recompute the placement of all children for the current geometry
	void apply();

	// access / replace the container parameters stored as a component on the owner
	const FlexLayoutInfo *getInfo() const;
	void setInfo(const FlexLayoutInfo &);

	// convenience mutators for individual container parameters
	void setDirection(FlexDirection);
	void setWrap(FlexWrap);
	void setJustifyContent(FlexJustify);
	void setAlignItems(FlexAlign);
	void setAlignContent(FlexAlign);
	void setGap(float row, float column);
	void setPadding(Padding);

	// helpers to read / assign per-item parameters via the component system
	static const FlexItemInfo *getItem(NotNull<Node>);
	static void setItem(NotNull<Node>, const FlexItemInfo &);

protected:
	// apply a mutation to the owner's FlexLayoutInfo component, creating it if needed
	void updateInfo(const Callback<bool(FlexLayoutInfo &)> &);

	FlexLayoutInfo _initialInfo;
};

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEFLEXLAYOUT_H_ */
