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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUITREEVIEW_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUITREEVIEW_H_

#include "SPDataModel.h"
#include "XLUiPanel.h"
#include "XLSubscriptionListener.h"
#include "XL2dIconSprite.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"
#include "XLUiRowGeometry.h"
#include "XLUiRowSelection.h"
#include "XLUiMarquee.h"
#include "XLDropTarget.h"
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TreeView;

/* A scrolled view over a data::Model tree.

The view shows a flat list of visible rows (an open category's children follow it, in model order)
and materializes only rows in the viewport. The view owns expansion state; the model owns data.
A `Kind::Span` child stands for N rows read in slices, drawn as `loading` until they arrive.
A category with a childs callback is populated on first expand. Everything runs on the app thread.

The row list is re-derived on every change, never spliced, so factory indices stay valid. Row nodes
are reused by RowKey, and a selection change only updates classes. Rebuilds run at the start of the
visit, so new rows are laid out on the frame they appear (Node::runPendingPhases).

CSS: types "tree-view" and "tree-row" (Panels; an unstyled Panel is opaque white). A row publishes
`--tree-depth` and `--tree-row-h`, and the sheet computes the indent:

  tree-view { background-color:#1e1e1e; }
  tree-row  { background-color: transparent;
              display:flex; flex-direction:row; align-items:center;
              height: var(--tree-row-h);
              padding-inline-start: calc(8px + var(--tree-depth, 0) * var(--tree-indent, 16px));
              padding-inline-end:8px; column-gap:6px; }
  tree-row.selected { background-color:#094771; }
  .tree-toggle { flex:0 0 18px; height:18px; border-radius:9px; }
  .tree-toggle:hover { background-color:#2a2a2a; }
  .tree-toggle > icon { width:16px; height:16px; }
  .tree-icon  { flex:0 0 16px; width:16px; height:16px; }
  .tree-label { flex-grow:1; font-size:13px; white-space:nowrap;
                text-align:start; unicode-bidi:normal; }

Use logical properties (`padding-inline-start`, `text-align:start`) so the tree mirrors in
right-to-left windows; `unicode-bidi:normal` keeps a sheet-wide `plaintext` from deriving the
label's direction from its script.

A row also carries `expanded` / `collapsed` / `leaf`, `loading` and `selected` style classes. The
rubber band is type `marquee` (see setMarqueeEnabled). */
/* A tree view can hold the scene's selection, opt-in per instance; see setSelectionOwned(). */
class SP_PUBLIC TreeView : public Panel, public SelectionOwner {
public:
	using Model = data::Model;
	using ModelNode = data::Model::Node;
	using ItemId = data::Model::ItemId;

	class RowBuilder;
	class RowNode;

	/* One visible row. `node` + `offset` is its stable identity (ItemIds are never reused), which
	carries payloads and expansion across rebuilds. `offset` is used only for a Span. */
	struct SP_PUBLIC Row {
		Rc<ModelNode> node; // the element itself, or the SPAN a span row belongs to
		uint64_t offset = 0; // index within the span
		uint32_t depth = 0;
		uint32_t revision = 0; // the node's revision when this row was derived
		bool expanded = false;
		bool dataLoaded = false; // spans only — an explicit node's payload is always in hand
		float height = nan(); // resolved in rebuildRows(), before any node exists
		Value spanData; // payload of a span row; unused by every other kind

		bool isCategory() const { return node && node->isCategory(); }
		bool isSpanItem() const { return node && node->isSpan(); }

		ItemId getId() const { return node ? node->getId() : ItemId(0); }

		// A span row shows its slice data; other rows read the model directly.
		const Value &getData() const {
			return (node && !node->isSpan()) ? node->getData() : spanData;
		}
	};

	/* Everything a standard row node was built from; rows with equal keys can share a node, so an
	expand or collapse keeps untouched rows' nodes. The selection is not in the key; it is applied
	by updateRowNode. */
	struct SP_PUBLIC RowKey {
		Rc<ModelNode> node;
		uint64_t offset = 0;
		uint32_t depth = 0;
		// So editing one row's payload rebuilds only that row's node.
		uint32_t revision = 0;
		float height = 0.0f;
		bool expanded = false;
		bool dataLoaded = false;

		bool operator==(const RowKey &other) const {
			return node == other.node && offset == other.offset && depth == other.depth
					&& revision == other.revision && height == other.height
					&& expanded == other.expanded && dataLoaded == other.dataLoaded;
		}
	};

	/* Where a drop into this tree would land. A leaf row splits in halves (before/after). A
	category row has a CategoryDropBand at each end for before/after and a wide middle for into.

	`parent` and `index` are in model terms for data::Model::moveNode or emplaceItem(); `index` is
	maxOf<size_t>() for an append. `row` is the row under the pointer, or maxOf<size_t>() for the
	empty space below the last row, which targets the root. Depends only on geometry and the model,
	so it works without a drag in flight. */
	struct SP_PUBLIC DropPosition {
		enum class Kind {
			None, // nowhere: there is no model at all
			Into, // append to `parent`
			Before, // insert at `index`, above the row
			After, // insert at `index`, below the row
		};

		Kind kind = Kind::None;
		size_t row = maxOf<size_t>();

		// Rc: a position can outlive its event and a rebuild, while the category may be removed.
		Rc<ModelNode> parent;
		size_t index = maxOf<size_t>();

		bool valid() const { return kind != Kind::None && parent; }

		bool operator==(const DropPosition &other) const {
			return kind == other.kind && row == other.row && parent == other.parent
					&& index == other.index;
		}
	};

	/* Caller hooks for dropping. The view resolves positions, draws feedback and opens categories
	under a dwell; the caller decides acceptance and applies the drop.

	`accept` must be pure: it runs during hit testing, possibly several times a frame. Return the
	subset of `event.allowed` acceptable at `pos`, or DragActions::None to let the drag fall
	through to what is under this view. */
	struct SP_PUBLIC DropSlots {
		Function<DragActions(const DragEvent &, const DropPosition &)> accept;

		// Apply the drop. `action` is a single resolved bit. False means nothing was actually done,
		// and the source's completion is told DragActions::None
		Function<bool(const DragEvent &, const DropPosition &, DragActions)> drop;
	};

	using RowFunction = Function<void(RowBuilder &)>;
	using RowHeightFunction = Function<float(const Row &)>;
	using RowEventFunction = Function<void(size_t index, const Row &)>;
	using RowFilterFunction = Function<bool(size_t index, const Row &)>;
	using MarqueeFunction = Function<void(ListSelectionOp)>;

	virtual ~TreeView();

	virtual bool init() override;
	virtual bool init(Model *);

	virtual void handleContentSizeDirty() override;

	virtual void setSource(Model *);
	Model *getSource() const;

	// Show the model's root as row 0 (depth 0, expandable) instead of starting with its children.
	virtual void setRootVisible(bool);
	bool isRootVisible() const { return _rootVisible; }

	SpanView<Row> getRows() const { return _rows; }
	size_t getRowCount() const { return _rows.size(); }
	const Row *getRow(size_t) const;

	// Expansion is keyed by ItemId, so re-expanding or reloading a category restores its open
	// subtree. Rows update immediately; the nodes follow on the next frame.
	virtual bool expandRow(size_t);
	virtual bool collapseRow(size_t);
	virtual bool toggleRow(size_t);

	bool isRowExpanded(size_t) const;

	// Move the element a row stands for, via Model::moveNode (the model may refuse). Refused for
	// a span row.
	virtual bool moveRow(size_t index, ModelNode *dstParent, size_t childIndex);

	// false: collapsing forgets the subtree's expansion and drops its lazily loaded children.
	virtual void setKeepExpandedState(bool);
	bool isKeepExpandedState() const { return _keepExpanded; }

	virtual void setRowCallback(RowFunction &&);

	// Row height, needed by the ScrollController before the node exists. Runs for every row on
	// every rebuild: keep it cheap and side-effect free, and handle rows whose payload has not
	// arrived (`dataLoaded == false`).
	virtual void setRowHeightCallback(RowHeightFunction &&);

	// The height of every row the callback does not resize, and the value it falls back to.
	virtual void setRowHeight(float);
	float getRowHeight() const { return _rowHeight; }
	float getRowHeight(const Row &) const;

	// Key of the string in a row's Value that the standard label shows. Default "name".
	virtual void setLabelKey(StringView);
	StringView getLabelKey() const { return _labelKey; }

	// Selection is off until one of these is set; only then do rows get an input listener and can
	// match `.tree-row:hover` / `.tree-row.selected`.
	virtual void setSelectCallback(RowEventFunction &&);
	virtual void setActivateCallback(RowEventFunction &&);
	virtual void setSelectionEnabled(bool);
	bool isSelectionEnabled() const { return _selectionEnabled; }

	/* One row, or a set. In the multiple mode a press with Shift or the toggle modifier works on the
	set (see ListSelectionOp) over the visible rows, and while the view owns the scene's selection
	Shift+Up/Down extend it and Ctrl+A takes every visible row. A selected row inside a collapsed
	branch stays selected; an additive pick keeps it, any other drops it. */
	virtual void setSelectionMode(ListSelectionMode);
	ListSelectionMode getSelectionMode() const { return _selection.getMode(); }

	virtual void setSelectedRow(size_t); // maxOf<size_t>() clears

	// The current row: the one picked last, which the keyboard and an activation act on.
	size_t getSelectedRow() const { return _selection.getCurrent(); }

	// Every selected visible row, ascending. After a toggle the current row may be outside it.
	SpanView<size_t> getSelectedRows() const { return _selection.getRows(); }
	bool isRowSelected(size_t index) const { return _selection.isSelected(index); }

	// Exactly these rows; `current` falls back to the first of them. Like setSelectedRow, no callback.
	virtual void setSelectedRows(SpanView<size_t>, size_t current);

	/* Follow a selection made elsewhere (a canvas, a document): moves the row, and hands it to the
	scene's SelectionSystem only while this view already holds the selection, so a mirror never
	takes the keyboard from the surface the author is working in. */
	virtual void showSelectedRow(size_t);
	virtual void showSelectedRows(SpanView<size_t>, size_t current);

	// What the pick being reported did, inside the select callback; Replace for a single step.
	ListSelectionOp getLastSelectionOp() const { return _lastSelectionOp; }

	/* True inside the select callback when the pick came from an arrow key rather than a tap: a
	callback that opens what was picked should wait for the activation (Enter or a double tap). */
	bool isSelectingFromKeyboard() const { return _keyboardSelect; }

	/* The rubber band, as TableView::setMarqueeEnabled: a mouse drag sweeps the visible rows between
	the band's top and bottom, shown by the `selected` class until the release applies it. A row in
	a collapsed branch keeps its selection through a band that toggles or adds, and loses it to one
	that replaces. A row that is a drag source starts no band; the space below the rows does. */
	virtual void setMarqueeEnabled(bool);
	bool isMarqueeEnabled() const { return _marqueeEnabled; }

	virtual void setMarqueeFilter(RowFilterFunction &&);
	virtual void setMarqueeCallback(MarqueeFunction &&);

	bool isMarqueeActive() const { return _sweep.active; }
	SpanView<size_t> getMarqueeHits() const { return _sweep.hits; }
	MarqueeSystem *getMarquee() const { return _marquee; }

	bool isRowShownSelected(size_t index) const {
		return _sweep.isShown(index, _selection.isSelected(index));
	}

	/* Join the scene-wide selection (SelectionSystem): rows match `:selected`, the view matches
	`:selection-within`, and hotkeys go to the row, then this view, first. Opt-in, because popup
	lists (ui::SearchPicker, ui::Select) must not take the scene's selection. */
	virtual void setSelectionOwned(bool);
	bool isSelectionOwned() const { return _selectionOwned; }

	// --- SelectionOwner ----------------------------------------------------

	virtual Node *getSelectionOwnerNode() override { return this; }

	// The row node showing this item, or null while it is scrolled out or inside a collapsed branch
	virtual Node *resolveSelectionNode(const SelectionItem &) const override;

	virtual void handleSelectionChanged(SpanView<SelectionItem>) override;

	/* Up and Down step through the visible rows. The inline-start arrow collapses an open category
	or goes to the parent row, the inline-end arrow expands a closed one or enters its first child;
	both follow the view's direction. */
	virtual bool moveSelection(SelectionDirection) override;

	// The closest visible row to `fromWorld`
	virtual bool enterSelection(SelectionDirection, const Rect &fromWorld) override;

	// Re-derive the rows and re-request their data.
	virtual void invalidateSource();

	// Rebuild the row nodes at the start of the next visit. Coalesced and deferred: the rebuild can
	// destroy the node it is reached from, and nodes attached mid-frame are laid out on that frame.
	// Rows with an unchanged RowKey keep their nodes; pass `force` when something outside the key
	// (row callback, label key) changed.
	virtual void requestRebuildNodes(bool force = false);

	/* The same, with `cb` run once at the end of that rebuild, inside the visit, when new rows are
	already styled and laid out (Node::runPendingPhases). Callbacks coalesce and run in order; one
	that asks again is served by the next rebuild. Only rows in the scroll window get nodes. */
	virtual void requestRebuildNodes(Function<void()> &&cb, bool force = false);

	basic2d::ScrollView *getScroll() const { return _scroll; }
	basic2d::ScrollController *getController() const { return _controller; }

	// A row's rectangle in this node's space (ui::RowGeometrySource), also for rows without a node.
	bool getRowRect(size_t index, Rect &out) const;

	/* The row rectangle with its left edge at the content node (after indent, expander and icon),
	e.g. for an inline editor. Needs a materialized row, since the indent comes from the sheet;
	otherwise falls back to getRowRect. */
	bool getRowContentRect(size_t index, Rect &out) const;

	size_t getRowIndexAt(const Vec2 &nodeLocation) const;

	/* --- dropping into the tree ---------------------------------------------------------------

	One drop target on the view, not per row: rows are virtualized and the empty space below the
	last row (the root) has no row. The row is resolved from the pointer (getDropPositionAt). */

	// The share of a category's row, at each end, that means "beside it" rather than "into it".
	static constexpr float CategoryDropBand = 0.2f;

	virtual void setDropSlots(DropSlots &&); // also enables dropping
	const DropSlots &getDropSlots() const { return _dropSlots; }

	virtual void setDropEnabled(bool);
	bool isDropEnabled() const { return _dropEnabled; }

	/* How long a drag has to rest on a collapsed category before the view opens it; zero disables.
	Movement within the category does not restart the dwell; leaving it cancels. Runs as an
	Action, which keeps the frame loop awake in on-demand rendering. */
	virtual void setDropExpandDelay(TimeInterval);
	TimeInterval getDropExpandDelay() const { return _dropExpandDelay; }

	// Where a drop would land for a point in this node's space.
	DropPosition getDropPositionAt(const Vec2 &nodeLocation) const;

	/* The position for row `index`, with `offset` 0 at the row's top edge and 1 at its bottom.
	maxOf<size_t>() asks for the empty space below the last row. See DropPosition. */
	DropPosition getDropPositionForRow(size_t index, float offset) const;

	/* The feedback rectangle for `pos` in this node's space: the row box for Into, a thin bar for
	Before/After, both starting at the anchor row's indent so nesting levels are distinguishable.
	False when there is nothing to draw. */
	bool getDropPositionRect(const DropPosition &, Rect &out) const;

	/* Where row `index` begins its content (its indent), read from the laid-out row since the sheet
	computes the padding. nan() for a row without a node. */
	float getRowIndentX(size_t index) const;

	// What the view is showing feedback for right now; Kind::None while no drag is over it.
	const DropPosition &getDropPosition() const { return _dropPosition; }

protected:
	using Panel::init;

	virtual void handleSourceDirty(SubscriptionFlags);

	// Mark every span row's payload stale, so the next model pass re-asks for it. Explicit nodes
	// read their payload from the model.
	void dropSpanData();

	// Re-derive the model, request missing data, and schedule the nodes. Data is requested before
	// any node exists, so an inline model needs no placeholder frame.
	virtual void refresh();

	// Model passes. Both are synchronous and touch no scene node, so they are safe to run from
	// inside a row's own callback.
	virtual void rebuildModel();
	virtual void appendChildRows(ModelNode *, uint32_t depth, Map<Model::Position, Value> &);
	virtual void requestRowData();

	virtual void handleSliceData(ModelNode *span, uint64_t first, size_t count,
			Map<uint64_t, Value> &);

	bool isExpanded(const ModelNode *) const;

	// Node pass. Re-derives the controller's item list, so it only ever runs through
	// requestRebuildNodes(). Rows whose RowKey survived keep their nodes.
	virtual void rebuildRows();

	/* Recompute the selected indices from the identities after _rows is re-derived, since indices
	shift on expand/collapse. An identity with no row (collapsed parent) is kept, so re-expanding
	restores it. */
	void remapSelection();

	RowIdentity getRowIdentity(size_t) const;

	// After the selection changed: restyles the rows on screen and hands it to the scene.
	void applySelection(bool changed);
	void updateRowNodes();

	// Shift+Up/Down and Ctrl+A, bound while the view owns the scene's selection in the multiple mode.
	void bindSelectionHotkeys();
	bool extendSelectionFromKeyboard(bool down);
	bool selectAllFromKeyboard();

	// The select callback, with the operation and the keyboard flag it is asked about.
	void notifySelect(size_t index, ListSelectionOp, bool keyboard);

	// Hand the current selection to the scene's SelectionSystem. No-op unless owned
	void publishSelection();

	// The opaque identity of row `index`, as SelectionSystem stores it
	SelectionItem makeSelectionItem(size_t index) const;

	// A keyboard pick: selects, scrolls the row into view and reports it like a tap
	void selectRowFromKeyboard(size_t index);

	// Enter on the selected row, while this view owns the scene's selection
	void bindActivateHotkeys();
	bool activateSelectedRow();

	virtual Rc<Node> makeRow(size_t index);
	virtual Rc<Node> buildRowNode(RowBuilder &);

	RowGeometrySource makeGeometrySource() const;

	// The live node of a materialized row; null when the row is outside the scroll window.
	RowNode *getRowNode(size_t index) const;

	// Re-apply presentation outside the RowKey (the selection) on an existing row node.
	virtual void updateRowNode(RowNode *, size_t index);

	// Claim a node carried over the current rebuild for row `index`, or null when none matches.
	Rc<RowNode> takeReusableRow(size_t index);

	static RowKey makeRowKey(const Row &);

	virtual void handleRowTap(size_t index, uint32_t count, InputModifier = InputModifier::None);

	virtual bool handleMarqueeBegin(const MarqueeEvent &);
	virtual void handleMarqueeUpdate(const MarqueeEvent &);
	virtual void handleMarqueeEnd(const MarqueeEvent &, bool commit);

	// Drop the expansion of everything under `cat` and release the children it loaded lazily.
	void forgetSubtree(ModelNode *cat);

	// Action tag of the dwell that opens a collapsed category under a drag.
	static constexpr uint32_t DropExpandActionTag = "XLUiTreeDropExpand"_tag;
	static constexpr float InsertionLineThickness = 2.0f;

	// The upright at the left end of the insertion line, marking the indent it sits at.
	static constexpr float InsertionStemHeight = 10.0f;

	virtual void updateDropSystems();

	// enter / over: re-resolve, move the feedback, and restart the dwell when the category changed
	virtual void updateDropPosition(const DragEvent &);
	void clearDropPosition();

	void showDropFeedback();
	void hideDropFeedback();

	void armDropExpand();
	void cancelDropExpand();
	void fireDropExpand();

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	DataListener<Model> *_sourceListener = nullptr;

	Vector<Row> _rows;

	// Held by identity; the indices are derived from it on every rebuild. The offset of an
	// identity tells apart the rows of one span, which share an ItemId.
	RowSelection _selection;
	ListSelectionOp _lastSelectionOp = ListSelectionOp::Replace;

	bool _selectionOwned = false;
	bool _keyboardSelect = false;
	InputListener *_activateKeys = nullptr;

	// Set while applying a change that came from the system, so publishSelection() does not echo it
	bool _applyingSelection = false;

	// Set while handing the selection to the system, which hands it straight back
	bool _publishing = false;

	MarqueeSystem *_marquee = nullptr;
	ListSweep _sweep;
	RowFilterFunction _marqueeFilter;
	MarqueeFunction _marqueeCallback;
	bool _marqueeEnabled = false;

	// By id, so a reloaded category keeps its open subtree.
	Set<ItemId> _expanded;

	// Row nodes carried across the running rebuild for makeRow() to claim; empty otherwise.
	Vector<Rc<RowNode>> _reusableRows;

	RowFunction _rowCallback;
	RowHeightFunction _rowHeightCallback;
	RowEventFunction _selectCallback;
	RowEventFunction _activateCallback;

	String _labelKey = String("name");
	float _rowHeight = 26.0f;

	// Non-zero while expandRow() requests children; an inline answer's refresh is left to
	// expandRow().
	uint32_t _deferRefresh = 0;

	bool _rootVisible = false;
	bool _keepExpanded = true;
	bool _selectionEnabled = false;
	bool _rebuildPending = false;
	bool _inDataRequest = false;

	// The pending rebuild ignores RowKey reuse (something outside the key changed).
	bool _forceRebuild = false;

	// Callbacks for the next rebuild; taken off the list before they run.
	Vector<Function<void()>> _rebuildCallbacks;

	// --- dropping into the tree ---
	// Whether the drop target component is currently declared on this node
	bool _hasDropTarget = false;
	DropSlots _dropSlots;
	DropPosition _dropPosition; // what the feedback on screen is showing
	basic2d::Layer *_insertionLine = nullptr;
	basic2d::Layer *_dropHighlight = nullptr;

	// The category the dwell is running for; by Rc, since rows can be re-derived meanwhile.
	Rc<ModelNode> _dropExpandCandidate;
	TimeInterval _dropExpandDelay = TimeInterval::milliseconds(500);
	bool _dropEnabled = false;
};

// Chooses what a row looks like. Every setter is optional: a builder the factory never touches
// yields the standard row — an expander when the row has children, no icon, and a label reading
// data[labelKey].
//
// No height setter: the height is resolved before the row is built. Use
// TreeView::setRowHeightCallback().
class SP_PUBLIC TreeView::RowBuilder {
public:
	TreeView *getView() const { return _view; }
	const Row &getRow() const { return *_row; }
	size_t getIndex() const { return _index; }

	const Value &getData() const { return _row->getData(); }
	uint32_t getDepth() const { return _row->depth; }
	bool isExpandable() const { return _row->isCategory(); }
	bool isExpanded() const { return _row->expanded; }
	bool isLoaded() const { return _row->dataLoaded; } // false: the payload has not arrived yet
	bool isSelected() const;

	// The element behind the row and its external object. Null for a view with no model.
	ModelNode *getNode() const { return _row->node; }
	Ref *getObject() const { return _row->node ? _row->node->getObject() : nullptr; }

	// Take the row over completely. Nothing below has any effect afterwards; TreeView still writes
	// --tree-depth and --tree-row-h onto the node, which a full row is free to ignore.
	void setNode(Rc<Node> &&);

	// --- the decorated path
	void setExpander(Rc<Node> &&); // your widget, and you wire the toggle yourself
	void setExpanderIcons(IconName collapsed, IconName expanded); // standard button, your icons
	void setExpanderVisible(bool); // false: not even the empty slot that keeps names aligned

	void setIcon(IconName); // IconName::None (the default) means no icon node at all
	void setIcon(Rc<Node> &&);

	void setLabel(StringView); // instead of data[labelKey]
	void setContent(Rc<Node> &&); // your node in the label's slot
	void addTrailing(Rc<Node> &&); // appended after the content, in call order

	void addStyleClass(StringView); // on the row node
	void setName(StringView); // node name — also the CSS id, and how the inspector finds the row

protected:
	friend class TreeView;

	TreeView *_view = nullptr;
	const Row *_row = nullptr;
	size_t _index = 0;

	Rc<Node> _node;
	Rc<Node> _expander;
	Rc<Node> _iconNode;
	Rc<Node> _content;
	Vector<Rc<Node>> _trailing;
	Vector<String> _classes;
	String _label;
	String _name;
	IconName _icon = IconName::None;
	IconName _iconCollapsed = IconName::Navigation_chevron_right_solid;
	IconName _iconExpanded = IconName::Navigation_expand_more_solid;
	bool _hasLabel = false;
	bool _expanderVisible = true;
};

// The standard row container: a Panel, so a row can be painted, rounded and hovered by CSS like any
// other atom. It knows its index only to route taps back to the view.
class SP_PUBLIC TreeView::RowNode : public Panel {
public:
	virtual ~RowNode();

	virtual bool init(TreeView *, size_t index, bool interactive);

	size_t getRowIndex() const { return _index; }

	// Stored rather than captured: a rebuild moves a reused row to a new index.
	void setRowIndex(size_t index) { _index = index; }

	const RowKey &getRowKey() const { return _key; }
	void setRowKey(RowKey &&key) { _key = sp::move(key); }

	// The node in the content slot (the label or a replacement); see
	// TreeView::getRowContentRect.
	Node *getContentNode() const { return _content; }
	void setContentNode(Node *node) { _content = node; }

	// The expander node, when it handles its own taps; the row does not select on a tap inside it.
	// Needed because ui::Button does not swallow the touch (it must not block scroll swipes).
	void setExpanderNode(Node *node) { _expander = node; }

protected:
	using Panel::init;

	TreeView *_view = nullptr; // the view owns the controller that owns this node's factory
	size_t _index = 0;
	RowKey _key;
	InputListener *_listener = nullptr;
	Node *_expander = nullptr; // a child of this node, so no ownership is needed
	Node *_content = nullptr; // likewise
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUITREEVIEW_H_
