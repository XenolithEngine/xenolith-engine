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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUITABLEVIEW_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUITABLEVIEW_H_

#include "SPDataModel.h"
#include "XLUiPanel.h"
#include "XLUiLayoutSystem.h"
#include "XLSubscriptionListener.h"
#include "XL2dIconSprite.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"
#include "XLUiRowGeometry.h"
#include "XL2dLayer.h"
#include "XLDragSource.h"
#include "XLDropTarget.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TableView;

/* A scrolled, virtualized table over a data::Model.

The model is a FLAT list of rows: the children of the model's root, in order. A child is one row —
except a `Kind::Span` child, which stands for N rows nobody stores and is read the way a database
cursor is read, in slices, with a row that has no payload yet drawn as `loading` and refreshed when
the answer lands. So a table of fifty thousand database rows is one span, and costs fifty thousand
small structs and as many nodes as fit on screen; a table of a hundred editable records is a hundred
explicit nodes that can be reordered, removed and pointed at their originals.

The two mix freely in one table, because they are just children of the same category.

Columns are the part a stylesheet cannot fully express, so they are split in two:
- WHAT a column is - which key of the row's Value it shows, what its header says - lives in
  `Column`, set from code;
- HOW WIDE it is comes from CSS, as `grid-template-columns` on the table-view node (a table's
  columns ARE a track list, which is why the grid property is reused rather than duplicated).
  `Column::track` is the fallback for columns the sheet does not mention.

Row geometry is resolved ONCE per width change and imposed on every row: `resolveColumns()` produces
a single `TableColumnsComponent` and stamps that same value on the header and on every row node. The
rows are laid out by `LayoutSystem` in `LayoutMode::TableRow`, exactly as the rows of a static
`display: table` are - the only difference is who wrote the component. That is also why the sticky
header cannot drift out of alignment with the rows: there is one source of column geometry and no
second layout path.

The header is a sibling of the ScrollView rather than a row inside it, which is the whole of
"sticky" - there is no scroll offset to compensate.

A table does not have to be a scroller. setAutoHeight() makes it report the height of its whole
model instead (getIntrinsicHeight()) and stop scrolling, so it can sit as one BLOCK inside a larger
scroll view - a page of headings and explanatory text with a table in the middle. That trades away
virtualization for that table, which is the right trade for tens of rows and the wrong one for
thousands; see setAutoHeight().

Node reuse follows TreeView: a row whose `RowKey` is unchanged keeps the node it already has. Note
that the key carries `columnsRevision` (the SET of columns) and NOT the `generation` of the resolved
geometry (their pixel widths): a window resize re-lays-out every row but rebuilds none of them.

CONSTRAINT: a row's height is resolved before its node exists, because that is the only moment a
ScrollController can be told a size. So a cell must not be fit-content on the height axis - its
height is the row's, and a cell that re-measured itself taller would disagree with the size the
controller already committed to. Use setRowHeightCallback() for variable heights.

CSS: the widget is type "table-view", the header "table-header", a row "table-row" and a cell
"table-cell" (all Panels, so all take background-color / outline / border-radius - and note that a
Panel with no fill declared is an opaque WHITE surface, so a row meant to show the view's own
background must say so). A row carries `even`/`odd`, `selected` and `loading` style classes; a
header cell carries `header-cell` plus the column's own `Column::styleClass`. Each row publishes the
height it was laid out with as `--table-row-h`, and each cell its column index as
`--table-col-index`.

  table-view   { display: table; grid-template-columns: 2fr 1fr 120px;
                 border-collapse: collapse; background-color: #1e1e1e; }
  table-header { background-color: #252526; }
  table-row    { background-color: transparent; height: var(--table-row-h); }
  table-row.odd      { background-color: #212121; }
  table-row.selected { background-color: #094771; }
  table-cell   { display: flex; align-items: center; padding-left: 8px; padding-right: 8px;
                 border-bottom: 1px solid #333; }
  .header-cell > label { font-weight: bold; }
  .table-label { flex-grow: 1; white-space: nowrap; }
  .table-icon  { width: 16px; height: 16px; }

The `table-icon` above is what CellBuilder::setIcon adds, before the cell's label.

Declare the horizontal rules with `border-bottom` rather than `border-top`: a virtualized row
collapses only the borders it can see - the vertical lines between its own cells, and its own top
and bottom - so a line declared on both sides of a row boundary is drawn twice. */
class SP_PUBLIC TableView : public Panel {
public:
	using Model = data::Model;
	using ModelNode = data::Model::Node;
	using ItemId = data::Model::ItemId;

	class RowBuilder;
	class CellBuilder;
	class RowNode;
	class HeaderNode;

	// One column. Everything here is what a stylesheet cannot know; the width comes from CSS.
	struct SP_PUBLIC Column {
		String key; // key into the row's Value; empty -> only the cell callback fills it
		String title; // header text
		String styleClass; // extra class on every cell of this column and on its header cell
		GridTrack track; // fallback width when the CSS track list is shorter than the columns

		bool operator==(const Column &) const = default;
	};

	/* One row of the model. `node` + `offset` is its identity and survives a rebuild, which is what
	lets a payload that arrives late find its row again without carrying an index — and, because an
	ItemId is never reused, what makes that identity survive an insertion or a removal too.

	`offset` is meaningful only when `node` is a Span; an explicit node leaves it at zero and keeps
	its payload in the model, so there is nothing to fetch for it. */
	struct SP_PUBLIC Row {
		Rc<ModelNode> node;
		uint64_t offset = 0;
		uint32_t revision = 0; // the node's revision when this row was derived
		float height = nan(); // resolved in rebuildRows(), before any node exists
		bool dataLoaded = false; // true once an answer arrived, even an empty one
		Value spanData; // payload of a span row; unused by every other kind

		bool isSpanItem() const { return node && node->isSpan(); }
		ItemId getId() const { return node ? node->getId() : ItemId(0); }

		const Value &getData() const {
			return (node && !node->isSpan()) ? node->getData() : spanData;
		}
	};

	/* What a standard row node was built from. Two rows with the same key show the same thing, so
	the node made for one can be handed to the other instead of being rebuilt.

	`columnsRevision` is the revision of the column SET, not the generation of the resolved column
	GEOMETRY. Adding a column changes what a row node looks like; making one 3px wider does not -
	the row re-lays-out from the re-stamped component and keeps every node. Folding the two into one
	counter would rebuild the whole visible table on every window resize. */
	struct SP_PUBLIC RowKey {
		Rc<ModelNode> node;
		uint64_t offset = 0;
		// The node's own revision, so editing one row's payload rebuilds one row's node.
		uint32_t revision = 0;
		uint64_t columnsRevision = 0;
		float height = 0.0f;
		bool dataLoaded = false;

		bool operator==(const RowKey &other) const {
			return node == other.node && offset == other.offset && revision == other.revision
					&& columnsRevision == other.columnsRevision && height == other.height
					&& dataLoaded == other.dataLoaded;
		}
	};

	using RowFunction = Function<void(RowBuilder &)>;
	using CellFunction = Function<void(CellBuilder &)>;
	using RowHeightFunction = Function<float(const Row &)>;
	using RowEventFunction = Function<void(size_t index, const Row &)>;

	virtual ~TableView();

	virtual bool init() override;
	virtual bool init(Model *);

	virtual void handleContentSizeDirty() override;

	virtual void setSource(Model *);
	Model *getSource() const;

	// Replacing the column set bumps the revision, so every row node is rebuilt.
	virtual void setColumns(Vector<Column> &&);
	virtual void addColumn(Column &&);
	virtual void clearColumns();
	SpanView<Column> getColumns() const { return _columns; }

	SpanView<Row> getRows() const { return _rows; }
	size_t getRowCount() const { return _rows.size(); }
	const Row *getRow(size_t) const;

	// Decorate a whole row (classes, a replacement node). Cells still come from the cell callback.
	virtual void setRowCallback(RowFunction &&);
	// Decorate one body cell. Called once per column per materialized row.
	virtual void setCellCallback(CellFunction &&);
	// Decorate one header cell.
	virtual void setHeaderCellCallback(CellFunction &&);

	// A row's height is consumed one pass earlier than its node is built (see the class doc), so it
	// cannot be measured from the node and cannot come from the builder. This callback is the
	// channel. It runs for every row on every rebuild, so keep it cheap, and answer for a row whose
	// payload has not arrived yet (`dataLoaded == false`) rather than assume one.
	virtual void setRowHeightCallback(RowHeightFunction &&);

	virtual void setRowHeight(float);
	float getRowHeight() const { return _rowHeight; }
	float getRowHeight(const Row &) const;

	virtual void setHeaderVisible(bool);
	bool isHeaderVisible() const { return _headerVisible; }
	virtual void setHeaderHeight(float);
	float getHeaderHeight() const { return _headerVisible ? _headerHeight : 0.0f; }
	HeaderNode *getHeader() const { return _header; }

	/* The height the widget needs in order to show EVERY row: the header plus the sum of the rows'
	heights.

	Resolved from the MODEL, so not one node has to exist - which is the whole point. An outer
	ScrollController must be told an item's size BEFORE it will ever call the factory that builds
	it, exactly the constraint the rows themselves are under, so a table embedded in one cannot be
	measured from its nodes either. Costs one setRowHeightCallback() call per row, so it is as cheap
	as that callback is. */
	float getIntrinsicHeight() const;

	/* Size the widget to its whole model instead of scrolling inside a fixed box.

	This is what lets a table be a BLOCK inside a larger scroll view - a page of headings and
	explanatory text with a table in the middle of it - rather than a scroller of its own. Two
	things follow: the inner ScrollView is disabled, because nested scrollers would otherwise fight
	over the same gesture, and the widget starts answering the content measurement protocol, so
	`flex-basis: fit-content` on it resolves to getIntrinsicHeight() and a fit-content column
	containing it composes recursively.

	The owner still has to APPLY that size - through a fit-content layout, or by passing
	getIntrinsicHeight() to ScrollController::addItem. Auto-height only reports and stops
	scrolling; it never writes its own contentSize.

	The cost is virtualization: the viewport becomes the whole content, so every row is
	materialized. That is the right trade for tens of rows and the wrong one for thousands, which
	should keep their own scroll in a fixed frame. */
	virtual void setAutoHeight(bool);
	bool isAutoHeight() const { return _autoHeight; }

	// Fires when the model changed the answer getIntrinsicHeight() gives. An outer ScrollController
	// caches the size it was handed and cannot notice on its own; a fit-content layout re-measures
	// by itself and can ignore this.
	virtual void setIntrinsicHeightCallback(Function<void(float)> &&);

	// Selection is off until one of these is set: only then does a row get an input listener at
	// all, and only then can `table-row:hover` / `table-row.selected` match.
	virtual void setSelectCallback(RowEventFunction &&);
	virtual void setActivateCallback(RowEventFunction &&);
	virtual void setSelectionEnabled(bool);
	bool isSelectionEnabled() const { return _selectionEnabled; }

	virtual void setSelectedRow(size_t); // maxOf<size_t>() clears
	size_t getSelectedRow() const { return _selectedRow; }

	// Re-derive the rows and re-request their data.
	virtual void invalidateSource();

	// Rebuild the row NODES at the start of the next visit. Coalesced, and deferred on purpose: the
	// rebuild can destroy the node it is reached from, and a node attached while a frame is in
	// flight is styled and laid out on that frame rather than the next.
	virtual void requestRebuildNodes(bool force = false);

	basic2d::ScrollView *getScroll() const { return _scroll; }
	basic2d::ScrollController *getController() const { return _controller; }

	/* Where a row and a cell LIE, in this node's coordinate space.

	Answers for a row that has no node: only the nodes are virtualized, while rebuildRows() commits
	one controller item per row with the height it resolved before any node existed. Reproducing
	that outside the widget means copying arithmetic that lives in here and will change, which is
	the whole reason these are public.

	False before the first layout pass - there is nothing to report yet, and a zero rectangle is a
	worse answer than an admitted absence. */
	bool getRowRect(size_t index, Rect &out) const;
	bool getCellRect(size_t row, size_t column, Rect &out) const;

	/* Which row lies at a point, in CONTENT space rather than in the visible box: a point above or
	below the viewport names the row that would be there, the same way getRowRect describes a row
	that scrolled out of sight. maxOf<size_t>() only for a point outside the content entirely. */
	size_t getRowIndexAt(const Vec2 &nodeLocation) const;

	// The boundary an insertion would snap to: 0..getRowCount(), never a row index.
	size_t getRowBoundaryAt(const Vec2 &nodeLocation, Rect *boundaryRect = nullptr) const;

	/* Reordering rows by dragging a grip, and by Alt+Up / Alt+Down.

	THE GRIP IS A COLUMN THE CALLER DECLARES, under this key, wherever it wants it and with whatever
	track CSS gives it. The view fills that cell in - an icon and a DragSource - but does not insert
	the column itself: doing that would renumber every other column behind the caller's back, and
	the `grid-template-columns` list they already wrote would line up against the wrong cells.

	`to` IS THE ROW'S FINAL INDEX, counted after it has been taken out of its old place. That is the
	only reading under which "move this one down" is expressible, and it is the same convention
	data::Model::moveNode states.

	The callback returns FALSE to refuse: the order does not change and neither does the selection.
	Nothing is moved by the view itself - the model belongs to the caller, and only the caller knows
	whether the move is legal. On acceptance the view re-points the selection so that it follows the
	ROW, not the index it used to sit at. */
	static constexpr StringView ReorderColumnKey = StringView("__reorder");

	virtual void setReorderEnabled(bool);
	bool isReorderEnabled() const { return _reorderEnabled; }

	virtual void setReorderCallback(Function<bool(size_t from, size_t to)> &&);

	// Ask for a move as if the user had done it. What the keyboard path calls, and what a test
	// drives the widget with. False when it was refused or was a no-op.
	virtual bool reorderRow(size_t from, size_t to);

protected:
	using Panel::init;

	virtual void handleSourceDirty(SubscriptionFlags);
	virtual void refresh();

	// Mark every SPAN row's payload stale. Explicit nodes are not touched: their payload lives in
	// the model and is read through, so it cannot be out of date with it.
	void dropSpanData();

	// Resolve the column geometry for the current width and stamp it on the header and every live
	// row. One computation, one component, every consumer - that is what keeps the sticky header
	// aligned with the rows without either measuring the other.
	virtual void resolveColumns();
	virtual void restampColumns();

	virtual void rebuildModel();
	virtual void requestRowData();
	virtual void handleSliceData(ModelNode *span, uint64_t first, size_t count,
			Map<uint64_t, Value> &);

	virtual void rebuildRows();
	virtual void rebuildHeader();
	virtual Rc<Node> makeRow(size_t index);
	virtual Rc<Node> buildRowNode(RowBuilder &);
	// build the cells of `node` for `row`; also used for the header, with `header` set
	virtual void buildCells(Node *node, const Row *row, size_t index, bool header);

	RowGeometrySource makeGeometrySource() const;

	// Fills the caller's `__reorder` cell: the grip icon plus the DragSource that starts the move.
	Rc<Node> makeReorderCell(size_t index);

	void updateReorderSystems();
	void showInsertionLine(size_t boundary);
	void hideInsertionLine();

	bool handleReorderDrop(size_t from, const Vec2 &nodeLocation);
	bool handleReorderHotkey(bool down);


	RowNode *getRowNode(size_t index) const;
	virtual void updateRowNode(RowNode *, size_t index);
	Rc<RowNode> takeReusableRow(size_t index);
	RowKey makeRowKey(const Row &) const;

	virtual void handleRowTap(size_t index, uint32_t count);

	// give a node the systems and components that make it lay its children out as a table row
	void makeTableRow(Node *);

	// Re-derive the intrinsic height and report it if it moved. No-op unless auto-height is on -
	// a scrolling table has no intrinsic height to report and nobody is listening for one.
	virtual void updateIntrinsicHeight();

	HeaderNode *_header = nullptr; // OUTSIDE the ScrollView - that is the whole of "sticky"
	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	DataListener<Model> *_sourceListener = nullptr;

	Vector<Column> _columns;
	bool _reorderEnabled = false;
	Function<bool(size_t from, size_t to)> _reorderCallback;
	InputListener *_reorderKeys = nullptr;
	// The target is a component on this node now, so there is nothing to hold - only whether it is
	// currently declared
	bool _hasDropTarget = false;
	basic2d::Layer *_insertionLine = nullptr;

	TableColumnsComponent _geometry; // the one copy every row and the header is stamped from
	uint64_t _columnsRevision = 0;

	Vector<Row> _rows;
	Vector<Rc<RowNode>> _reusableRows;

	RowFunction _rowCallback;
	CellFunction _cellCallback;
	CellFunction _headerCellCallback;
	RowHeightFunction _rowHeightCallback;
	RowEventFunction _selectCallback;
	RowEventFunction _activateCallback;
	Function<void(float)> _intrinsicHeightCallback;

	float _rowHeight = 28.0f;
	float _headerHeight = 32.0f;
	// The last height reported through _intrinsicHeightCallback; nan() until one has been reported,
	// so the first answer is never swallowed as "unchanged".
	float _reportedHeight = nan();
	size_t _selectedRow = maxOf<size_t>();

	bool _autoHeight = false;
	bool _headerVisible = true;
	bool _selectionEnabled = false;
	bool _rebuildPending = false;
	bool _forceRebuild = false;
	bool _inDataRequest = false;
};

// Chooses what a row looks like. Every setter is optional: a builder the callback never touches
// yields the standard row - one cell per column, each showing data[column.key].
//
// There is deliberately no height setter: the height is consumed one pass earlier (see TableView).
class SP_PUBLIC TableView::RowBuilder {
public:
	TableView *getView() const { return _view; }
	const Row &getRow() const { return *_row; }
	size_t getIndex() const { return _index; }

	const Value &getData() const { return _row->getData(); }
	bool isLoaded() const { return _row->dataLoaded; } // false: the payload has not arrived yet
	bool isSelected() const;

	// The element behind the row, and the external object it stands for. Null for a table with no
	// model; a span row answers with the span node, which is the honest answer — the row is an
	// offset into a length, and there is no element there to point at.
	ModelNode *getNode() const { return _row->node; }
	Ref *getObject() const { return _row->node ? _row->node->getObject() : nullptr; }

	// Take the row over completely. Nothing below has any effect afterwards, and TableView builds
	// no cells for it - a full row is responsible for its own content.
	void setNode(Rc<Node> &&);

	void addStyleClass(StringView); // on the row node
	void setName(StringView); // node name - also the CSS id, and how the inspector finds the row

protected:
	friend class TableView;

	TableView *_view = nullptr;
	const Row *_row = nullptr;
	size_t _index = 0;

	Rc<Node> _node;
	Vector<String> _classes;
	String _name;
};

// Chooses what one cell looks like. Untouched, it yields a label showing data[column.key].
class SP_PUBLIC TableView::CellBuilder {
public:
	TableView *getView() const { return _view; }
	const Column &getColumn() const { return *_column; }
	size_t getColumnIndex() const { return _columnIndex; }
	size_t getRowIndex() const { return _rowIndex; }

	// null for a header cell
	const Row *getRow() const { return _row; }
	bool isHeader() const { return _row == nullptr; }

	// The cell's own value: data[column.key], or a null Value when there is no key, no payload yet
	// or this is a header cell.
	const Value &getValue() const;

	void setNode(Rc<Node> &&); // your node instead of the standard label cell
	void setLabel(StringView); // instead of the value / the column title
	void setIcon(IconName); // an icon before the label
	void setColumnSpan(uint32_t); // this cell covers N columns
	void addStyleClass(StringView);
	void setName(StringView);

protected:
	friend class TableView;

	TableView *_view = nullptr;
	const Column *_column = nullptr;
	const Row *_row = nullptr;
	size_t _columnIndex = 0;
	size_t _rowIndex = 0;

	Rc<Node> _node;
	Vector<String> _classes;
	String _label;
	String _name;
	IconName _icon = IconName::None;
	uint32_t _columnSpan = 1;
	bool _hasLabel = false;
};

// The standard row container: a Panel, so a row can be painted and hovered by CSS like any other
// atom. It knows its index only to route taps back to the view.
class SP_PUBLIC TableView::RowNode : public Panel {
public:
	virtual ~RowNode();

	virtual bool init(TableView *, size_t index, bool interactive);

	size_t getRowIndex() const { return _index; }
	// a rebuild moves a surviving row to a new index, so the index is stored rather than captured
	void setRowIndex(size_t index) { _index = index; }

	const RowKey &getRowKey() const { return _key; }
	void setRowKey(RowKey &&key) { _key = sp::move(key); }

protected:
	using Panel::init;

	TableView *_view = nullptr; // the view owns the controller that owns this node's factory
	size_t _index = 0;
	RowKey _key;
	InputListener *_listener = nullptr;
};

// The header: the same kind of node as a row, laid out by the same component, just not scrolled.
class SP_PUBLIC TableView::HeaderNode : public Panel {
public:
	virtual ~HeaderNode() = default;

	virtual bool init(TableView *);

protected:
	using Panel::init;

	TableView *_view = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUITABLEVIEW_H_
