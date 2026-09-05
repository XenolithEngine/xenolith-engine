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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSYSTEM_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSYSTEM_H_

#include "XLUiDockTree.h"
#include "XLUiDockFrame.h"
#include "XLUiDockDragVisuals.h"
#include "XLUiPanelHost.h"
#include "XLDropTarget.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** The parking system: one system on the node a dock is rooted at.

THE ONE THING TO KNOW. Every frame node is a FLAT direct child of this system's owner. The split
tree is a data structure in here; nothing in the scene graph nests. `handleLayoutChildren` walks
the tree, computes a rect per slot and writes `setContentSize` + `setPosition` on those flat
children. Two consequences that are easy to get wrong:

 - the owner MUST NOT also carry a LayoutSystem. Two systems with HandleLayoutChildren both write
   every child's geometry and neither wins. handleAdded asserts this, and the owner must never be
   `display: flex` in a stylesheet - which is what the SystemManagedLayout marker it sets prevents;

 - a frame's index among the owner's children means NOTHING. Node::sortAllChildren is not a stable
   sort and frames share a ZOrder, so their order permutes. Identify a frame by its DockNodeHandle
   or its DockFrameComponent, never by position.

WHAT THE APPLICATION DECLARES, AND WHAT THE SYSTEM THEN OWNS. The application registers a
descriptor per panel (id, title, icon, minimum size, a lazy builder) and one initial structure of
frames with their constraints. From there the system runs itself: it splits and merges frames,
moves panels between them, and recomputes the effective minimum of every frame and every split
from the panels actually parked in them. A constraint is only ever strengthened by that pass -
a frame can never end up smaller than the largest panel it holds.

THE THREE PASSES, kept separate on purpose. `updateMinimums` is pure and may run inside
handleMeasure; `distribute` writes only into the tree; `commitGeometry` is the only thing that
touches a node. That separation is what makes the measurement protocol safe to answer.

WHERE THE PANELS THEMSELVES LIVE. Not here: in a ui::PanelRegistry, which this system creates for
itself unless one is handed to it. Sharing one with another container - a ui::AccordionView - is what
lets a panel be dragged from this dock into that one and arrive with its node intact. Every panel
method below therefore reads as "my structure, plus whatever the registry says", and the registry is
what enforces that a panel is parked in exactly one place at a time. */
class SP_PUBLIC DockSystem : public System, public PanelHost {
public:
	// same band as LayoutSystem: after styling has resolved, before anything user-level
	static constexpr uint32_t DockDefaultPriority = System::DefaultPriority - 100;

	static constexpr float DefaultSplitterThickness = 6.0f;

	// how far a pointer has to travel on a tab before it counts as pulling the panel out rather
	// than as a tap; must stay above the tap tolerance or a click would start a drag
	static constexpr float DefaultDragThreshold = 8.0f;

	// how far the "drop here to split" bands reach into a frame from its edges; capped at a
	// quarter of the frame so a small frame still has a middle to drop into
	static constexpr float DefaultEdgeDropBand = 48.0f;

	// ZOrder bands. They must stay DISTINCT: sortAllChildren is unstable, so equal-ZOrder siblings
	// permute between frames and a splitter could end up drawn and hit-tested under a frame.
	//
	// There is no band for the drag ghost: it is the general drag system's decorator now, parked
	// at DragSystem::DecoratorZOrder - inside this owner, but above every band here.
	static constexpr ZOrder FrameZOrder = ZOrder(0);
	static constexpr ZOrder SplitterZOrder = ZOrder(64);
	static constexpr ZOrder IndicatorZOrder = ZOrder(128);

	// published on the frame stack, so a docked panel's subtree can find its dock during a visit
	static uint64_t SystemFrameTag;

	using LayoutChangedCallback = Function<void()>;
	using PanelCallback = Function<void(StringView id)>;

	virtual ~DockSystem() = default;

	virtual bool init() override;

	// Run against a registry somebody else owns, so this dock and another container share one set of
	// panels: dragging one across then moves the node rather than rebuilding it.
	virtual bool init(Rc<PanelRegistry> &&);

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleLayoutChildren() override;
	virtual bool handleMeasure(const MeasureConstraints &, Size2 &) override;
	virtual void handleChildContentSizeDirty(Node *) override;

	// The nearest DockSystem at or above `node`. Everything public here happens OUTSIDE a visit
	// (a button in a panel body asking to close itself), which is why this walks the parent chain
	// rather than the frame stack - that one is only alive while a node is being visited.
	static DockSystem *findForNode(Node *);

	// --- panel registry ----------------------------------------------------
	//
	// Convenience forwards: the registry is where a panel actually is described and built. Use them
	// for a dock that owns its registry; reach for the registry itself when two containers share it.

	// Register what a panel is. The content node is not built here: `builder` runs at most once,
	// on first show, and the node is then kept across moves so a panel keeps its state when it is
	// dragged into another frame - or into another container entirely.
	void registerPanel(DockPanelDescriptor &&);
	void unregisterPanel(StringView id);

	virtual PanelRegistry *getPanelRegistry() const override { return _registry; }

	// --- structure ---------------------------------------------------------

	// Replace the whole layout. Panel ids the registry does not know are dropped with a warning.
	bool setLayout(const DockLayoutSpec &);

	const DockTree &getTree() const { return _tree; }

	DockNodeHandle getRootNode() const { return _tree.getRoot(); }
	DockNodeHandle findFrameByName(StringView) const;
	DockNodeHandle findFrameForPanel(StringView panelId) const;
	DockFrame *getFrameNode(DockNodeHandle) const;
	SpanView<String> getPanelsInFrame(DockNodeHandle) const;

	// --- panels ------------------------------------------------------------

	// Show a panel. An empty `target` resolves to the descriptor's defaultFrame, then to the frame
	// the panel was last in, then to the largest one.
	//
	// A panel another container is holding is taken from it: the registry evicts the previous host
	// as part of handing the node over, so nothing here has to ask first.
	bool openPanel(StringView id, DockNodeHandle target = DockNodeHandle(),
			size_t index = maxOf<size_t>());
	virtual bool closePanel(StringView id) override;
	virtual bool activatePanel(StringView id) override;
	bool movePanel(StringView id, DockNodeHandle target, size_t index = maxOf<size_t>());
	virtual bool isPanelOpen(StringView id) const override;

	// --- PanelHost ---------------------------------------------------------

	virtual Ref *getPanelHostRef() override { return this; }

	// Take the panel out of the tree without reporting it closed: it is moving elsewhere. The node
	// is untouched - the registry hands it to the new host - and an emptied frame folds away exactly
	// as it does for a close.
	virtual void releasePanel(StringView id) override;

	// The dock root. It is inside the StyleResolver's subtree and it is never clipped, so a ghost
	// parked here takes its `dock-drag-ghost` rule and survives crossing every frame boundary.
	virtual Node *getPanelDecoratorParent() const override { return _owner; }

	// --- frames ------------------------------------------------------------

	// Subdivide a frame. `firstIsNew` puts the new frame on the low side of the axis - left for
	// Horizontal, TOP for Vertical (the scene's Y points up).
	DockNodeHandle splitFrame(DockNodeHandle frame, DockAxis, bool firstIsNew,
			const DockFrameParams & = DockFrameParams(), float ratio = 0.5f);
	DockNodeHandle splitFrameWithPanel(DockNodeHandle frame, DockAxis, bool firstIsNew,
			StringView panelId, float ratio = 0.5f);
	bool closeFrame(DockNodeHandle);

	/* Re-declare a parking place: its name (and with it its CSS #id), its floor, its flags and
	which edge carries its tab strip.

	The TREE is the source of truth for all four - a frame node the system builds later, after a
	restore or a collapse, reads them from there and not from the node - so both are written here.
	Flipping `tabBarSide` is the interesting one: the strip changes axis, its tabs change kind, and
	the frame's floor is re-measured from the strip's new intrinsic size. */
	bool setFrameParams(DockNodeHandle, const DockFrameParams &);

	/* SHUT A PARKING PLACE TO ITS TAB STRIP, or open it again.

	What it costs the layout is the point: a collapsed leaf reports only its strip as its minimum -
	the panels' declared floors AND the frame's own are both dropped, because a place that is showing
	nothing has no business reserving the room its content would need. The divider above it can then
	travel down to the strip, which is what turns a side pane into an icon rail. Nothing here moves a
	divider: shut the frames, then set the ratio, and the minimums stop it in the right place.

	The flag lives in the TREE and is written by save()/restore(), so a rail somebody shut stays shut
	across a restart. */
	bool setFrameCollapsed(DockNodeHandle, bool);
	bool isFrameCollapsed(DockNodeHandle) const;

	// --- parameters --------------------------------------------------------

	// Thickness of the divider between two frames, in points. It is a system parameter and not a
	// CSS one: it is an input of the layout, and the CSS subset has no way to feed one. Style the
	// divider's appearance with `dock-splitter { ... }`.
	void setSplitterThickness(float);
	float getSplitterThickness() const { return _splitterThickness; }

	void setOverflowPolicy(DockOverflowPolicy);
	DockOverflowPolicy getOverflowPolicy() const { return _overflowPolicy; }

	// --- persistence -------------------------------------------------------

	// The shape and the membership; never a panel's title, icon or minimum, which belong to the
	// descriptor registry and would go stale the moment the application is updated.
	Value save() const;

	// Restore a saved layout. Panels the registry no longer knows are dropped with a warning, and
	// a frame left empty by that folds away - a downgraded application must not be fatal.
	//
	// A panel that IS registered but absent from the file stays CLOSED: a panel the user closed
	// has to stay closed across a restart, which is the whole reason the layout is persisted. The
	// one exception is DockPanelFlags::OpenByDefault, which is how a panel added by a new version
	// of the application still appears.
	bool restore(const Value &);

	// --- callbacks ---------------------------------------------------------

	void setLayoutChangedCallback(LayoutChangedCallback &&);
	void setPanelOpenedCallback(PanelCallback &&);
	void setPanelClosedCallback(PanelCallback &&);
	void setPanelActivatedCallback(PanelCallback &&);

	// --- resizing ----------------------------------------------------------

	// May the divider of this split be dragged? False when either side forbids resizing.
	bool canResize(DockNodeHandle split) const;

	// Move a divider by `delta` points and re-derive the split's ratio from where it landed. The
	// travel is clamped so neither child goes below its propagated minimum. Called by DockSplitter
	// while it is being dragged, and directly by whoever wants to move one programmatically.
	void updateSplitterDrag(DockNodeHandle split, const Vec2 &delta);

	// Assign a split's ratio outright, clamped to what the minimums permit.
	bool setSplitRatio(DockNodeHandle split, float ratio);

	// --- receiving a dragged panel -----------------------------------------
	//
	// The dock is a DROP TARGET on its own owner, and nothing more: the general DragSystem runs
	// the drag, and this system only answers "would this land here, and where exactly".
	//
	// One target for the whole dock rather than one per frame, because a frame is found by walking
	// the split tree, not by hit-testing the scene - see hitTest below. The invariant that made the
	// dock's own session safe is unchanged and now belongs to the drop slot: everything is READ
	// first, and every structural change happens in one shot afterwards, because the drop can
	// collapse the frame the drag started in and destroy the tab that was delivering it.

	// Where a panel dropped at `rootLocal` (the dock root's own coordinate space) would land.
	//
	// Pure arithmetic over the tree, not a scene hit test: it costs O(tree depth) instead of a
	// traversal, it is not confused by whatever the panels' own input listeners are doing, and it
	// answers with a handle - which is what a commit needs anyway.
	DockDropTarget hitTest(const Vec2 &rootLocal, StringView draggedPanelId = StringView()) const;

	// how far into a frame the edge bands that mean "split here" reach
	void setEdgeDropBand(float);
	float getEdgeDropBand() const { return _edgeDropBand; }

	void setDragThreshold(float value) { _dragThreshold = value; }
	float getDragThreshold() const { return _dragThreshold; }

protected:
	// recompute minimums, distribute rects, write them onto the flat children
	void apply();

	// content minimum of one leaf: what its panels and its tab strip need, before the frame's own
	// declared floor is applied by the tree
	Size2 measureLeaf(const DockTreeNode &) const;

	// create the scene node for every slot that has none, drop the ones whose slot is gone
	void syncNodes();

	// Take every panel node parked inside `roots` out of the scene WITHOUT cleanup, before those
	// subtrees are destroyed. The registry outlives them and may be shared, so this is scoped: a
	// panel belonging to another container must not be touched. See the body for the full reason.
	void detachPanelsUnder(const Set<Node *> &roots);

	void commitGeometry();

	// re-parent the active panel's content into a frame's body, building it on first show, and
	// bring the tab strip in line with the frame's panel list
	void updateFrameContent(DockTreeNode &);

	// tabs of one frame, reusing the nodes that are already there
	void updateFrameTabs(DockTreeNode &);

	Node *acquireContent(StringView panelId);

	// The body of both closePanel and releasePanel: the panel leaves the tree and an emptied frame
	// folds away. `notify` is the only difference - a release is a move, not a close.
	bool takePanelOut(StringView id, bool notify);

	// coalesce: mutations only mark the owner dirty, so many of them cost one placement per frame
	void invalidateLayout();

	// --- drop-target slots -------------------------------------------------
	// The payload of a drag, if it is one of ours; null for anything else
	static DockPanelPayload *payloadOf(const DragEvent &);

	// Pure: called during hit testing, possibly several times a frame, for candidates that may
	// never become current. Resolves the zone and answers whether it would take the panel
	DragResponse handleDragAccept(const DragEvent &);

	void handleDragEnter(const DragEvent &);
	void handleDragOver(const DragEvent &);
	void handleDragLeave(const DragEvent &);
	bool handleDragDrop(const DragEvent &, DragActions);

	DockTree _tree;

	// What a panel is and what its node is, possibly shared with another container. Never null after
	// init(): a dock with no registry handed to it makes its own.
	Rc<PanelRegistry> _registry;

	// how the dock receives drags; installed on the owner in handleAdded

	// the highlight, alive only between a drag entering this dock and leaving it
	Rc<DockDropIndicator> _indicator;

	float _splitterThickness = DefaultSplitterThickness;
	float _dragThreshold = DefaultDragThreshold;
	float _edgeDropBand = DefaultEdgeDropBand;
	DockOverflowPolicy _overflowPolicy = DockOverflowPolicy::Scale;

	// guards the placement pass against the ContentSize notifications it causes itself
	bool _inPlacement = false;

	LayoutChangedCallback _layoutChangedCallback;
	PanelCallback _panelOpenedCallback;
	PanelCallback _panelClosedCallback;
	PanelCallback _panelActivatedCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSYSTEM_H_
