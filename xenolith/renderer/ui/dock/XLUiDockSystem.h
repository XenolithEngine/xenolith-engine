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

Every frame node is a flat direct child of the owner; the split tree exists only as data here, and
`handleLayoutChildren` writes each child's size and position from it. The owner must not also
carry a LayoutSystem (asserted in handleAdded). Frames share a ZOrder and sortAllChildren is not
stable, so identify a frame by its DockNodeHandle or DockFrameComponent, never by child index.

Effective minimums of frames and splits are recomputed from the parked panels and only ever
strengthen the declared ones. `updateMinimums` is pure (safe inside handleMeasure), `distribute`
writes only into the tree, `commitGeometry` is the only pass that touches nodes.

Panels live in a ui::PanelRegistry, created here unless one is passed in; sharing it with another
container lets a panel move between them with its node intact. */
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

	// ZOrder bands must stay distinct: sortAllChildren is unstable, so equal bands would let a
	// splitter draw and hit-test under a frame. The drag ghost uses DragSystem::DecoratorZOrder.
	static constexpr ZOrder FrameZOrder = ZOrder(0);
	static constexpr ZOrder SplitterZOrder = ZOrder(64);
	static constexpr ZOrder IndicatorZOrder = ZOrder(128);

	// published on the frame stack, so a docked panel's subtree can find its dock during a visit
	static uint64_t SystemFrameTag;

	using LayoutChangedCallback = Function<void()>;
	using PanelCallback = Function<void(StringView id)>;

	virtual ~DockSystem() = default;

	virtual bool init() override;

	// Run against a registry somebody else owns, so this dock and another container share one set
	// of panels: dragging one across then moves the node rather than rebuilding it.
	virtual bool init(Rc<PanelRegistry> &&);

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleLayoutChildren() override;
	virtual bool handleMeasure(const MeasureConstraints &, Size2 &) override;
	virtual void handleChildContentSizeDirty(Node *) override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags) override;

	// The nearest DockSystem at or above `node`. Walks the parent chain, not the frame stack, so it
	// works outside a visit.
	static DockSystem *findForNode(Node *);

	// --- panel registry ----------------------------------------------------
	//
	// Forwards to the registry; use the registry directly when it is shared between containers.

	// `builder` runs at most once, on first show; the node is then kept across moves, including
	// moves into another container.
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
	// the panel was last in, then to the largest one. A panel held by another container is taken
	// from it by the registry.
	bool openPanel(StringView id, DockNodeHandle target = DockNodeHandle(),
			size_t index = maxOf<size_t>());
	virtual bool closePanel(StringView id) override;
	virtual bool activatePanel(StringView id) override;
	virtual void handlePanelTapped(StringView id) override;
	bool movePanel(StringView id, DockNodeHandle target, size_t index = maxOf<size_t>());
	virtual bool isPanelOpen(StringView id) const override;

	// --- PanelHost ---------------------------------------------------------

	virtual Ref *getPanelHostRef() override { return this; }

	// Take the panel out of the tree without reporting it closed (it is moving elsewhere). The node
	// is left to the registry; an emptied frame folds away as on close.
	virtual void releasePanel(StringView id) override;

	// The dock root: styled and never clipped, so a ghost here gets its `dock-drag-ghost` rule and
	// stays visible across frame boundaries.
	virtual Node *getPanelDecoratorParent() const override { return _owner; }

	// --- frames ------------------------------------------------------------

	// Subdivide a frame. `firstIsNew` puts the new frame on the low side of the axis - left for
	// Horizontal, top for Vertical (the scene's Y points up).
	DockNodeHandle splitFrame(DockNodeHandle frame, DockAxis, bool firstIsNew,
			const DockFrameParams & = DockFrameParams(), float ratio = 0.5f);
	DockNodeHandle splitFrameWithPanel(DockNodeHandle frame, DockAxis, bool firstIsNew,
			StringView panelId, float ratio = 0.5f);
	bool closeFrame(DockNodeHandle);

	/* Re-declare a frame's name (its CSS #id), floor, flags and tab strip side. Written to the
	tree, which is the source of truth for nodes built later, and to the node. Changing `tabBarSide`
	re-measures the floor from the strip's new orientation. */
	bool setFrameParams(DockNodeHandle, const DockFrameParams &);

	/* Collapse a frame to its tab strip, or expand it. A collapsed leaf's minimum is only its strip
	(panel and frame floors are dropped), so a divider can then move down to the strip; this call
	does not move dividers itself. The flag is persisted by save()/restore(). */
	bool setFrameCollapsed(DockNodeHandle, bool);
	bool isFrameCollapsed(DockNodeHandle) const;

	// --- parameters --------------------------------------------------------

	// Divider thickness in points. A layout input, so not settable from CSS; style the divider's
	// appearance with `dock-splitter { ... }`.
	void setSplitterThickness(float);
	float getSplitterThickness() const { return _splitterThickness; }

	void setOverflowPolicy(DockOverflowPolicy);
	DockOverflowPolicy getOverflowPolicy() const { return _overflowPolicy; }

	/* Register every frame of this dock with setNodeSelectable, so arrow navigation and
	SelectionSystem::setSelectOnPress can make a frame the selection. Off by default: a dock whose
	frames only hold other docks should leave the choice to those. */
	void setFramesSelectable(bool);
	bool isFramesSelectable() const { return _framesSelectable; }

	/* The frame of this dock that is the deepest frame on the scene's selection chain, or null.
	It carries DockFrame::setCurrent; a nested dock's frame outranks the frame that holds it. */
	DockFrame *getCurrentFrame() const { return _currentFrame; }

	// --- persistence -------------------------------------------------------

	// Saves shape and membership only; titles, icons and minimums come from the descriptors.
	Value save() const;

	// Restore a saved layout. Unknown panel ids are dropped with a warning and emptied frames fold
	// away. A registered panel absent from the file stays closed, unless it has
	// DockPanelFlags::OpenByDefault.
	bool restore(const Value &);

	// --- callbacks ---------------------------------------------------------

	void setLayoutChangedCallback(LayoutChangedCallback &&);
	void setPanelOpenedCallback(PanelCallback &&);
	void setPanelClosedCallback(PanelCallback &&);
	void setPanelActivatedCallback(PanelCallback &&);

	/* A tab was pressed by the user. Fires on every tap, including one that changed nothing, and
	never on programmatic activation; a collapsed frame uses it to react to taps on its strip. */
	void setPanelTapCallback(PanelCallback &&);

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
	// One drop target on the owner for the whole dock; DragSystem runs the drag. A drop reads
	// everything first and then changes structure in one step, since it can destroy the frame and
	// tab the drag started from.

	// Where a panel dropped at `rootLocal` (dock root space) would land. Walks the split tree,
	// O(depth), without a scene hit test.
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

	// Detach panel nodes inside `roots` without cleanup before those subtrees are destroyed, so the
	// registry keeps them. Only nodes under `roots`: the registry may be shared.
	void detachPanelsUnder(const Set<Node *> &roots);

	void commitGeometry();

	// follow the selection chain onto one frame; see getCurrentFrame
	void updateCurrentFrame();

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

	// possibly shared with another container; never null after init()
	Rc<PanelRegistry> _registry;

	// the highlight, alive only between a drag entering this dock and leaving it
	Rc<DockDropIndicator> _indicator;

	float _splitterThickness = DefaultSplitterThickness;
	float _dragThreshold = DefaultDragThreshold;
	float _edgeDropBand = DefaultEdgeDropBand;
	DockOverflowPolicy _overflowPolicy = DockOverflowPolicy::Scale;

	// guards the placement pass against the ContentSize notifications it causes itself
	bool _inPlacement = false;
	bool _framesSelectable = false;

	// Rc: a closed frame is removed before the next visit can clear its state
	Rc<DockFrame> _currentFrame;

	LayoutChangedCallback _layoutChangedCallback;
	PanelCallback _panelOpenedCallback;
	PanelCallback _panelClosedCallback;
	PanelCallback _panelActivatedCallback;
	PanelCallback _panelTapCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKSYSTEM_H_
