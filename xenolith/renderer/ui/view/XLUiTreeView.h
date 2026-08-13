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

#include "SPDataSource.h"
#include "XLUiPanel.h"
#include "XLSubscriptionListener.h"
#include "XL2dIconSprite.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TreeView;

/* A scrolled view over a data::Source tree.

What is on screen is a FLAT list of visible rows, not a tree of nodes: an open category's children
follow it in the list, and only the rows that fit the viewport are ever materialized. A directory
with ten thousand entries costs ten thousand small structs and as many nodes as fit on screen —
whereas a node per entry, built up front, would cost a full subtree walk before the first frame.

The model is a plain data::Source and stays one: this widget owns the expand/collapse state, the
Source owns the data. A Source is read one level at a time — a category's display children are its
subcategories (expandable rows) followed by its own items (leaf rows), which is exactly the order
Source's own flattening uses. The `lookupLevel`/`subcats` flattening is deliberately NOT used: it
opens every branch to the same depth, which is not what a tree does.

A category with a ChildsSourceCallback is populated on its first expand. A Source that can answer
inline (a filesystem walk) does so before the first frame; one that answers later has its rows
rendered as `loading` and refreshed when the payload lands. Nothing here runs on a worker thread.

The row list is re-derived on every change, never spliced: that is what keeps the index captured by
a row's factory valid, and it is why collapsing a category no longer forgets which of its
descendants were open.

Re-deriving the list does NOT mean re-building the nodes. A row node records the RowKey it was made
from and is handed to whichever index that row moved to, so opening a category rebuilds only the
rows that are genuinely new; and a change that alters no row at all - moving the selection - never
touches the controller, it flips a style class on the two nodes involved. And the rebuild itself runs
at the start of the visit that will draw its result, so a row joins the tree while the frame is in
flight and catches up on the visit's phases there and then (Node::runPendingPhases) - styled and
laid out on the frame it appears rather than the one after. Together that is why a click on a row
does not flicker.

CSS: the widget is type "tree-view" and a row is type "tree-row" (both Panels, so both take the
usual background-color / outline / border-radius - and note that a Panel with no fill declared is
an opaque WHITE surface, so a row meant to show the tree's own background must say so). A row
publishes its depth and the height the
controller laid it out with as node-local custom properties — a rule reaches a SET of nodes and so
cannot carry a per-row number — and the sheet does the arithmetic, so neither number is duplicated
in C++:

  tree-view { background-color:#1e1e1e; }
  tree-row  { background-color: transparent;
              display:flex; flex-direction:row; align-items:center;
              height: var(--tree-row-h);
              padding-left: calc(8px + var(--tree-depth, 0) * var(--tree-indent, 16px));
              padding-right:8px; column-gap:6px; }
  tree-row.selected { background-color:#094771; }
  .tree-toggle { flex:0 0 18px; height:18px; border-radius:9px; }
  .tree-toggle:hover { background-color:#2a2a2a; }
  .tree-toggle > icon { width:16px; height:16px; }
  .tree-icon  { flex:0 0 16px; width:16px; height:16px; }
  .tree-label { flex-grow:1; font-size:13px; white-space:nowrap; }

A row also carries `expanded` / `collapsed` / `leaf`, `loading` and `selected` style classes. */
class SP_PUBLIC TreeView : public Panel {
public:
	using Source = data::Source;
	using SourceId = data::Source::Id;

	class RowBuilder;
	class RowNode;

	// One visible row. `source` + `itemId` is the row's IDENTITY: it survives a model rebuild, which
	// is what lets a payload that arrives late find its row again without carrying an index.
	struct SP_PUBLIC Row {
		Rc<Source> source; // a category row's own Source; an item row's OWNING category
		SourceId itemId = Source::Self; // Self for a category row, the item index otherwise
		uint32_t depth = 0;
		bool expanded = false;
		bool dataLoaded = false; // true once an answer arrived, even an empty one
		float height = nan(); // resolved in rebuildRows(), before any node exists
		Value data;

		bool isCategory() const { return itemId == Source::Self; }
	};

	/* Everything a standard row node was built from.

	Two rows with the same key are the same row showing the same thing, so the node made for one
	can be handed to the other instead of being destroyed and built again. That is what keeps an
	expand or a collapse from redrawing the rows it did not touch: they keep their nodes, and only
	their index moves. Presentation that can change WITHOUT changing the row - the selection - is
	deliberately not part of the key; it is re-applied on the node it moves between (see
	updateRowNode). */
	struct SP_PUBLIC RowKey {
		Rc<Source> source;
		SourceId itemId = Source::Self;
		uint32_t depth = 0;
		float height = 0.0f;
		bool expanded = false;
		bool dataLoaded = false;

		bool operator==(const RowKey &other) const {
			return source == other.source && itemId == other.itemId && depth == other.depth
					&& height == other.height && expanded == other.expanded
					&& dataLoaded == other.dataLoaded;
		}
	};

	using RowFunction = Function<void(RowBuilder &)>;
	using RowHeightFunction = Function<float(const Row &)>;
	using RowEventFunction = Function<void(size_t index, const Row &)>;

	virtual ~TreeView();

	virtual bool init() override;
	virtual bool init(Source *);

	virtual void handleContentSizeDirty() override;

	virtual void setSource(Source *);
	Source *getSource() const;

	// Show the root Source as row 0 (depth 0, expandable) instead of starting with its children.
	virtual void setRootVisible(bool);
	bool isRootVisible() const { return _rootVisible; }

	SpanView<Row> getRows() const { return _rows; }
	size_t getRowCount() const { return _rows.size(); }
	const Row *getRow(size_t) const;

	// Expansion is keyed by Source, so collapsing and re-expanding a category restores the subtree
	// that was open under it. The model is updated immediately; the nodes follow on the next frame.
	virtual bool expandRow(size_t);
	virtual bool collapseRow(size_t);
	virtual bool toggleRow(size_t);

	bool isRowExpanded(size_t) const;

	// false: collapsing forgets the subtree's expansion AND drops its lazily loaded children.
	virtual void setKeepExpandedState(bool);
	bool isKeepExpandedState() const { return _keepExpanded; }

	virtual void setRowCallback(RowFunction &&);

	// A row's height is resolved during the geometry pass, BEFORE the row node exists — that is the
	// only moment the ScrollController can be told a size, and it is what lets the controller place
	// the scrollbar and build only the rows in view. So the height cannot be measured from the node
	// and cannot come from the RowBuilder; this callback is the channel. It runs for every row on
	// every rebuild, so keep it cheap and free of side effects, and answer for a row whose payload
	// has not arrived yet (`dataLoaded == false`) rather than assume one.
	virtual void setRowHeightCallback(RowHeightFunction &&);

	// The height of every row the callback does not resize, and the value it falls back to.
	virtual void setRowHeight(float);
	float getRowHeight() const { return _rowHeight; }
	float getRowHeight(const Row &) const;

	// Key of the string in a row's Value that the standard label shows. Default "name".
	virtual void setLabelKey(StringView);
	StringView getLabelKey() const { return _labelKey; }

	// Selection is off until one of these is set: only then does a row get an input listener at all,
	// and only then can `.tree-row:hover` / `.tree-row.selected` match.
	virtual void setSelectCallback(RowEventFunction &&);
	virtual void setActivateCallback(RowEventFunction &&);
	virtual void setSelectionEnabled(bool);
	bool isSelectionEnabled() const { return _selectionEnabled; }

	virtual void setSelectedRow(size_t); // maxOf<size_t>() clears
	size_t getSelectedRow() const { return _selectedRow; }

	// Re-derive the rows and re-request their data. Only the ROOT Source is watched through a
	// DataListener — Source has no parent links, so a subcategory's setDirty() does not reach it —
	// so call this after mutating a subcategory from outside.
	virtual void invalidateSource();

	// Rebuild the row NODES at the start of this widget's next visit. Coalesced, and deferred on
	// purpose: the rebuild can destroy the row node it is reached from - a tap on an expander
	// living in one of them - and a node attached while a frame is in flight is styled and laid
	// out on that frame rather than the next (see Node::runPendingPhases).
	//
	// A row whose RowKey is unchanged keeps the node it already has, so the rebuild only builds
	// what is genuinely new. Pass `force` when something OUTSIDE the row decided how the row looks
	// and changed - a new row callback, a new label key - because the key cannot see that.
	virtual void requestRebuildNodes(bool force = false);

	basic2d::ScrollView *getScroll() const { return _scroll; }
	basic2d::ScrollController *getController() const { return _controller; }

protected:
	using Panel::init;

	virtual void handleSourceDirty();

	// Re-derive the model, ask for what it still needs, and schedule the nodes. The data request
	// runs BEFORE any node exists, so a source that answers inline has every payload in place by
	// the time the first row is built and no placeholder frame is ever drawn.
	virtual void refresh();

	// Model passes. Both are synchronous and touch no scene node, so they are safe to run from
	// inside a row's own callback.
	virtual void rebuildModel();
	virtual void appendChildRows(Source *, uint32_t depth, Map<Source *, Map<SourceId, Value>> &);
	virtual void requestRowData();

	virtual void handleItemData(Source *, SourceId, Value &&);
	virtual void handleSliceData(Source *, SourceId first, size_t count, Map<SourceId, Value> &);

	// Node pass. Re-derives the controller's item list, so it only ever runs through
	// requestRebuildNodes(). Rows whose RowKey survived keep their nodes.
	virtual void rebuildRows();

	virtual Rc<Node> makeRow(size_t index);
	virtual Rc<Node> buildRowNode(RowBuilder &);

	// The live node of a materialized row; null when the row is outside the scroll window - and
	// then there is nothing to update, because the row is built with the current state when it
	// scrolls in.
	RowNode *getRowNode(size_t index) const;

	// Re-apply the presentation a row node can change WITHOUT becoming a different row, on the node
	// it already has. Only the selection qualifies today: everything else is in the RowKey and
	// therefore rebuilds the node.
	virtual void updateRowNode(RowNode *, size_t index);

	// Claim a node carried over the current rebuild for row `index`, or null when none matches.
	Rc<RowNode> takeReusableRow(size_t index);

	static RowKey makeRowKey(const Row &);

	virtual void handleRowTap(size_t index, uint32_t count);

	// Drop the expansion of everything under `cat` and release the children it loaded lazily.
	void forgetSubtree(Source *cat);

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	DataListener<Source> *_sourceListener = nullptr;

	Vector<Row> _rows;
	Set<Rc<Source>> _expanded;

	// Row nodes carried across the rebuild that is running right now, waiting for makeRow() to
	// claim them. Empty at every other moment: whatever is left when the pass ends belonged to a
	// row that is gone or that now looks different, and is released with the vector.
	Vector<Rc<RowNode>> _reusableRows;

	RowFunction _rowCallback;
	RowHeightFunction _rowHeightCallback;
	RowEventFunction _selectCallback;
	RowEventFunction _activateCallback;

	String _labelKey = String("name");
	float _rowHeight = 26.0f;
	size_t _selectedRow = maxOf<size_t>();

	// Non-zero while expandRow() is asking a category for its children: a source that answers
	// inline completes before the call returns, and its refresh is covered by the one expandRow()
	// makes afterwards.
	uint32_t _deferRefresh = 0;

	bool _rootVisible = false;
	bool _keepExpanded = true;
	bool _selectionEnabled = false;
	bool _rebuildPending = false;
	bool _inDataRequest = false;

	// The pending rebuild must build every row from scratch: something the RowKey cannot see - the
	// row callback itself - decides how a row looks, and it changed.
	bool _forceRebuild = false;
};

// Chooses what a row looks like. Every setter is optional: a builder the factory never touches
// yields the standard row — an expander when the row has children, no icon, and a label reading
// data[labelKey].
//
// There is deliberately no height setter here. The height is consumed one pass earlier than this
// runs: rebuildRows() must hand ScrollController::addItem a size before it will ever call the
// factory. Use TreeView::setRowHeightCallback().
class SP_PUBLIC TreeView::RowBuilder {
public:
	TreeView *getView() const { return _view; }
	const Row &getRow() const { return *_row; }
	size_t getIndex() const { return _index; }

	const Value &getData() const { return _row->data; }
	uint32_t getDepth() const { return _row->depth; }
	bool isExpandable() const { return _row->isCategory(); }
	bool isExpanded() const { return _row->expanded; }
	bool isLoaded() const { return _row->dataLoaded; } // false: the payload has not arrived yet
	bool isSelected() const;

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

	// A rebuild moves a surviving row to a new index, and the index is what the expander and the
	// tap route through - so it is stored here rather than captured, and re-stamped on reuse.
	void setRowIndex(size_t index) { _index = index; }

	const RowKey &getRowKey() const { return _key; }
	void setRowKey(RowKey &&key) { _key = sp::move(key); }

	// The node occupying the expander slot, when it is one that handles its own taps. A tap inside
	// it is the expander's alone: the row does not also select on it. Nothing else could arbitrate
	// this - the row's listener and the expander's are two independent listeners over overlapping
	// areas, and a plain ui::Button does not swallow the touch (it must not: a button inside a
	// ScrollView would then eat the swipe that scrolls it).
	void setExpanderNode(Node *node) { _expander = node; }

protected:
	using Panel::init;

	TreeView *_view = nullptr; // the view owns the controller that owns this node's factory
	size_t _index = 0;
	RowKey _key;
	InputListener *_listener = nullptr;
	Node *_expander = nullptr; // a child of this node, so no ownership is needed
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUITREEVIEW_H_
