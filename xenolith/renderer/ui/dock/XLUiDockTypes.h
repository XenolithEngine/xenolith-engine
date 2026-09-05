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

A PANEL is what an application actually wants on screen - an explorer, an editor, a console. It is
identified by a string id, described once by a DockPanelDescriptor, and its node is built lazily on
first show. A panel is never positioned by anyone: it lives inside whatever frame it is parked in.

A FRAME is a parking place. It holds any number of panels as tabs, shows one of them at a time, and
carries the constraints the application declared for that place.

Frames are divided by SPLITS, and split + frame together form a binary tree. That tree is a pure
data structure inside DockSystem: every frame node and every splitter node is a FLAT direct child
of the dock root, and the system writes their geometry itself from the tree. Nothing here nests. */

// Orientation of a split. Horizontal puts its two children side by side (the divider between them
// is a vertical bar); Vertical stacks them, `first` on TOP - the scene's Y axis points up, so the
// first child of a vertical split starts at the higher Y.
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

	Default = AllowSplit | AllowDrop | AllowClose | AllowResize,
};

SP_DEFINE_ENUM_AS_MASK(DockFrameFlags)

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

// Everything the dock knows about a panel, registered before the panel is ever shown.
//
// `minSize` is the whole point of the registry: it is what strengthens the constraints of every
// frame the panel is parked in, and through that of every split above it. `builder` is called at
// most once, on first show; the node it returns is kept alive by the system across moves between
// frames, so a panel does not lose its state when it is dragged somewhere else.
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
// `minSize` is the frame's OWN floor; the effective minimum is the maximum of it, of the minimums
// of the panels parked here, and of the intrinsic size of the tab strip.
struct SP_PUBLIC DockFrameParams {
	String name; // optional stable name; also written onto the frame node as its CSS #id
	Size2 minSize;
	DockFrameFlags flags = DockFrameFlags::Default;
	DockTabBarSide tabBarSide = DockTabBarSide::Top;

	bool operator==(const DockFrameParams &) const = default;
};

// A reference to one node of the split tree.
//
// Generational on purpose: a splitter node holds the handle of its split across frames, and a drop
// can merge that split away. Without the generation the freed slot would be silently reused and
// the stale handle would retarget to an unrelated node; with it the handle simply stops resolving.
struct SP_PUBLIC DockNodeHandle {
	static constexpr uint32_t InvalidIndex = maxOf<uint32_t>();

	uint32_t index = InvalidIndex;
	uint32_t generation = 0;

	bool empty() const { return index == InvalidIndex; }
	explicit operator bool() const { return index != InvalidIndex; }

	bool operator==(const DockNodeHandle &) const = default;
};

// What a dragged panel handle carries as the drag's in-process payload.
//
// A Ref rather than a bare string, because DragData's fast path hands over a live object - and
// because WHERE THE PANEL CAME FROM has to travel with it: a drop needs that to recognise the cases
// that are no-ops, such as dropping a frame's only panel back into that same frame.
//
// The origin is recorded twice over, at two granularities, because a panel can be dragged between
// containers of different kinds: `host` says which container, `source`/`sourceIndex` say where
// inside it. A target compares the host first - a drag that arrived from somewhere else has no
// no-op case to check, and its `source` handle means nothing in this container's tree.
struct SP_PUBLIC DockPanelPayload : public Ref {
	// The drag's local type tag. A target checks this before touching anything else, and a drag
	// carrying anything else is simply not ours
	static constexpr auto TypeName = StringView("xl/dock-panel");

	String panelId;

	// Two fields for one thing, because PanelHost is deliberately not a Ref (see XLUiPanelHost.h):
	// `host` is identity and dispatch, `hostRef` is the only thing keeping it alive for the drag.
	PanelHost *host = nullptr;
	Rc<Ref> hostRef;

	DockNodeHandle source; // the frame within a dock host; empty for any other kind
	size_t sourceIndex = maxOf<size_t>(); // the position within a linear host; unset for a dock
};

// Marker on every frame node: which slot of the tree it materializes.
//
// Written once, when the node is created. It is what lets the system map a node back to the tree
// without a dynamic_cast, and what a hit test or an inspector dump identifies a frame by - the
// node's position among the root's children means nothing (sortAllChildren is unstable).
struct SP_PUBLIC DockFrameComponent {
	static ComponentId Id;

	DockNodeHandle handle;

	bool operator==(const DockFrameComponent &) const = default;
};

// Where a dragged panel would land if it were dropped right now.
//
// The zones are ordered by how specific they are, and that is also the order the hit test tries
// them in: the tab strip wins over the body, an edge band of the body wins over its middle.
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
	// shrink every minimum on the offending axis proportionally: everything stays visible and
	// inside the root, nothing overlaps, and the layout snaps back exactly once the root grows
	Scale,

	// honour the minimums and let the tail run outside the root
	Clip,
};

// One node of the split tree, as the application describes it to setLayout().
//
// A plain aggregate mirroring the tree it describes: `isSplit` nodes carry exactly two children,
// leaves carry the parking place. Built with the three static helpers, which read like the tree:
//
//   Spec::hsplit(0.22f,
//       Spec::leaf({"explorer"}, {.name = "sidebar"}),
//       Spec::vsplit(0.72f, Spec::leaf({"editor"}), Spec::leaf({"console", "problems"})));
struct SP_PUBLIC DockLayoutSpec {
	bool isSplit = false;

	// --- split -------------------------------------------------------------
	DockAxis axis = DockAxis::Horizontal;

	// share of `first` in the space left AFTER both children got their minimums; see DockTree
	float ratio = 0.5f;

	Vector<DockLayoutSpec> children; // exactly two when isSplit

	// --- leaf --------------------------------------------------------------
	DockFrameParams params;
	Vector<String> panels; // in tab order
	size_t active = 0; // index into `panels`

	static DockLayoutSpec leaf(Vector<String> &&panels, DockFrameParams && = DockFrameParams());
	static DockLayoutSpec hsplit(float ratio, DockLayoutSpec &&left, DockLayoutSpec &&right);
	static DockLayoutSpec vsplit(float ratio, DockLayoutSpec &&top, DockLayoutSpec &&bottom);
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTYPES_H_
