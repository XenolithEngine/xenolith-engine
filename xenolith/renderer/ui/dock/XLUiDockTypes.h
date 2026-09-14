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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTYPES_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTYPES_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "XL2dIconSprite.h" // IconName of DockPanelDescriptor below

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class PanelHost;

/** The parking system: panels, frames and the tree that divides them.

A panel is identified by a string id, described by a DockPanelDescriptor and built lazily on first
show; it is placed only by the frame it is parked in. A frame holds panels as tabs, shows one at a
time and carries its declared constraints. Splits and frames form a binary tree kept as data in
DockSystem; all frame and splitter nodes are flat children of the dock root. */

// Orientation of a split. Horizontal puts the children side by side; Vertical stacks them with
// `first` on top (the scene's Y points up, so `first` has the higher Y).
enum class DockAxis : uint8_t {
	Horizontal,
	Vertical,
};

// Which edge of a frame carries its tab strip. Top/Bottom make a horizontal strip and eat height;
// Left/Right make a vertical one and eat width.
enum class DockTabBarSide : uint8_t {
	Top,
	Bottom,
	Left,
	Right,
};

// What a frame permits. Declared by the application for the places it creates, and inherited by
// every frame a later split produces from the frame it was split off.
enum class DockFrameFlags : uint32_t {
	None = 0,
	AllowSplit = 1 << 0, // may be subdivided (by splitFrame or by an edge drop)
	AllowDrop = 1 << 1, // may receive panels dragged from elsewhere
	AllowClose = 1 << 2, // its panels may be closed by the user
	AllowResize = 1 << 3, // the splitters bounding it are draggable
	Permanent = 1 << 4, // never collapsed, even when it holds no panel at all

	/* Axes along which a drop may subdivide the frame; neither bit set means both. Edge bands on a
	disallowed axis fall through to the center zone. They only narrow AllowSplit, and only apply
	to drops: `splitFrame` is never refused on an axis. */
	AllowSplitVertical = 1 << 5, // stacked: the SplitTop / SplitBottom zones
	AllowSplitHorizontal = 1 << 6, // side by side: the SplitLeft / SplitRight zones

	Default = AllowSplit | AllowDrop | AllowClose | AllowResize,
};

SP_DEFINE_ENUM_AS_MASK(DockFrameFlags)

// Whether a drop may subdivide a frame with these flags along this axis (neither bit means both).
constexpr bool allowsSplitAxis(DockFrameFlags flags, DockAxis axis) {
	if (!hasFlag(flags, DockFrameFlags::AllowSplit)) {
		return false;
	}
	const auto axes =
			flags & (DockFrameFlags::AllowSplitVertical | DockFrameFlags::AllowSplitHorizontal);
	if (axes == DockFrameFlags::None) {
		return true; // nothing narrowed: both axes
	}
	return hasFlag(flags,
			axis == DockAxis::Horizontal ? DockFrameFlags::AllowSplitHorizontal
										 : DockFrameFlags::AllowSplitVertical);
}

// What a panel permits.
enum class DockPanelFlags : uint32_t {
	None = 0,
	Closable = 1 << 0, // a close affordance is offered on its tab
	Movable = 1 << 1, // may be dragged out of the frame it sits in
	OpenByDefault = 1 << 2, // restore() opens it even when the saved layout has no record of it
	Singleton = 1 << 3, // at most one instance: openPanel() on an open panel only activates it

	Default = Closable | Movable,
};

SP_DEFINE_ENUM_AS_MASK(DockPanelFlags)

// Everything the dock knows about a panel, registered before it is shown.
//
// `minSize` raises the minimum of the frame holding the panel and of every split above it.
// `builder` is called at most once, on first show; the node is kept across moves.
struct SP_PUBLIC DockPanelDescriptor {
	String id; // stable, unique, and the key the layout is serialized with
	String title;
	IconName icon = IconName::None;
	Size2 minSize;
	DockPanelFlags flags = DockPanelFlags::Default;

	// name of the frame this panel prefers when it is opened without an explicit target
	String defaultFrame;

	Function<Rc<Node>()> builder;
};

// The constraints of one parking place, as declared by the application.
//
// `minSize` is the frame's own floor; the effective minimum is the maximum of it, of the minimums
// of the panels parked here, and of the intrinsic size of the tab strip.
struct SP_PUBLIC DockFrameParams {
	String name; // optional stable name; also written onto the frame node as its CSS #id
	Size2 minSize;
	DockFrameFlags flags = DockFrameFlags::Default;
	DockTabBarSide tabBarSide = DockTabBarSide::Top;

	bool operator==(const DockFrameParams &) const = default;
};

// A reference to one node of the split tree. Generational: a handle to a released slot stops
// resolving instead of retargeting to whatever reuses the slot.
struct SP_PUBLIC DockNodeHandle {
	static constexpr uint32_t InvalidIndex = maxOf<uint32_t>();

	uint32_t index = InvalidIndex;
	uint32_t generation = 0;

	bool empty() const { return index == InvalidIndex; }
	explicit operator bool() const { return index != InvalidIndex; }

	bool operator==(const DockNodeHandle &) const = default;
};

// In-process payload of a dragged panel: the id plus its origin, which a drop uses to detect
// no-op moves. `host` names the container; `source`/`sourceIndex` are only meaningful inside that
// host, so a target compares the host first.
struct SP_PUBLIC DockPanelPayload : public Ref {
	// the drag's local type tag; a target checks it before anything else
	static constexpr auto TypeName = StringView("xl/dock-panel");

	String panelId;

	// PanelHost is not a Ref: `host` is used for identity and calls, `hostRef` keeps it alive.
	PanelHost *host = nullptr;
	Rc<Ref> hostRef;

	DockNodeHandle source; // the frame within a dock host; empty for any other kind
	size_t sourceIndex = maxOf<size_t>(); // the position within a linear host; unset for a dock
};

// Marker on every frame node: which tree slot it materializes. Written once on creation; use it
// to identify a frame, since child order is unstable.
struct SP_PUBLIC DockFrameComponent {
	static ComponentId Id;

	DockNodeHandle handle;

	bool operator==(const DockFrameComponent &) const = default;
};

// Where a dragged panel would land if dropped now. The hit test prefers the tab strip over the
// body, and an edge band over the body's middle.
struct SP_PUBLIC DockDropTarget {
	enum class Kind : uint8_t {
		None, // nowhere: outside the dock, or over a frame that refuses drops
		Center, // append to the frame's tabs
		TabStrip, // insert into the strip at `tabIndex`
		SplitLeft, // subdivide the frame and take the named side
		SplitRight,
		SplitTop,
		SplitBottom,
	};

	Kind kind = Kind::None;
	DockNodeHandle frame;
	size_t tabIndex = 0; // TabStrip only
	Rect highlight; // root-local; what the indicator shows

	bool isSplit() const { return kind >= Kind::SplitLeft; }

	// axis and side of a split zone; meaningless for the others
	DockAxis getAxis() const {
		return (kind == Kind::SplitLeft || kind == Kind::SplitRight) ? DockAxis::Horizontal
																	 : DockAxis::Vertical;
	}
	bool isFirst() const { return kind == Kind::SplitLeft || kind == Kind::SplitTop; }
};

// What to do when the root is smaller than the tree's propagated minimum.
enum class DockOverflowPolicy : uint8_t {
	// shrink every minimum on the offending axis proportionally, keeping everything inside the root
	Scale,

	// honour the minimums and let the tail run outside the root
	Clip,
};

// One node of the split tree as passed to setLayout(). Split nodes have exactly two children,
// leaves carry the frame. Built with the static helpers:
//
//   Spec::hsplit(0.22f,
//       Spec::leaf({"explorer"}, {.name = "sidebar"}),
//       Spec::vsplit(0.72f, Spec::leaf({"editor"}), Spec::leaf({"console", "problems"})));
struct SP_PUBLIC DockLayoutSpec {
	bool isSplit = false;

	// --- split -------------------------------------------------------------
	DockAxis axis = DockAxis::Horizontal;

	// share of `first` in the space left after both children got their minimums; see DockTree
	float ratio = 0.5f;

	Vector<DockLayoutSpec> children; // exactly two when isSplit

	// --- leaf --------------------------------------------------------------
	DockFrameParams params;
	Vector<String> panels; // in tab order
	size_t active = 0; // index into `panels`

	// collapsed to its tab strip; see DockTreeNode::collapsed and DockSystem::setFrameCollapsed
	bool collapsed = false;

	static DockLayoutSpec leaf(Vector<String> &&panels, DockFrameParams && = DockFrameParams());
	static DockLayoutSpec hsplit(float ratio, DockLayoutSpec &&left, DockLayoutSpec &&right);
	static DockLayoutSpec vsplit(float ratio, DockLayoutSpec &&top, DockLayoutSpec &&bottom);
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTYPES_H_
