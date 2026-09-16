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
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TableView;

/* A scrolled, virtualized table over a data::Model.

Rows are the children of the model's root. A `Kind::Span` child stands for N rows read in slices;
a row without its payload yet has the `loading` class. Spans and explicit nodes can be mixed.

A `Column` (set from code) says which Value key a column shows and its title; widths come from CSS
`grid-template-columns` on the table-view, with `Column::track` as the fallback.
`resolveColumns()` builds one `TableColumnsComponent` per width change and stamps it on the header
and every row, laid out by `LayoutSystem` in `LayoutMode::TableRow`, so header and rows align.
The header is a sibling of the ScrollView, which makes it sticky.

setAutoHeight() reports the whole model's height and disables scrolling (no virtualization).

A row whose `RowKey` is unchanged keeps its node; the key has `columnsRevision` (the column set),
not the geometry generation, so a resize rebuilds no rows.

A row's height is resolved before its node exists, so cells must not be fit-content in height;
use setRowHeightCallback() for variable heights.

CSS: types "table-view", "table-header", "table-row" and "table-cell" (all Panels; an unstyled
Panel is opaque white). Cells are painted transparent by the view; a sheet can override that.
Rows get `even`/`odd`, `selected` and `loading` classes; header cells get `header-cell` plus
`Column::styleClass`. Rows publish `--table-row-h`, cells `--table-col-index`.

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

Declare horizontal rules with `border-bottom` only: a virtualized row collapses only its own
borders, so a line declared on both sides of a row boundary is drawn twice. */
/* A table view can hold the scene's selection, opt-in per instance; see setSelectionOwned(). */
class SP_PUBLIC TableView : public Panel, public SelectionOwner {
public:
	using Model = data::Model;
	using ModelNode = data::Model::Node;
	using ItemId = data::Model::ItemId;

	class RowBuilder;
	class CellBuilder;
	class RowNode;
	class HeaderNode;

	// One column; the width comes from CSS.
	struct SP_PUBLIC Column {
		String key; // key into the row's Value; empty -> only the cell callback fills it
		String title; // header text
		String styleClass; // extra class on every cell of this column and on its header cell
		GridTrack track; // fallback width when the CSS track list is shorter than the columns

		bool operator==(const Column &) const = default;
	};

	/* One row of the model. `node` + `offset` is its identity across rebuilds, insertions and
	removals (ItemIds are never reused), so a late payload finds its row. `offset` is used only
	for a Span; an explicit node keeps its payload in the model. */
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

	/* What a standard row node was built from; rows with equal keys can share a node.
	`columnsRevision` is the column set revision, not the geometry generation, so a width change
	re-lays-out rows without rebuilding them. */
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

	// Row height, needed before the node is built. Runs for every row on every rebuild, so keep it
	// cheap, and handle rows whose payload has not arrived (`dataLoaded == false`).
	virtual void setRowHeightCallback(RowHeightFunction &&);

	virtual void setRowHeight(float);
	float getRowHeight() const { return _rowHeight; }
	float getRowHeight(const Row &) const;

	virtual void setHeaderVisible(bool);
	bool isHeaderVisible() const { return _headerVisible; }
	virtual void setHeaderHeight(float);
	float getHeaderHeight() const { return _headerVisible ? _headerHeight : 0.0f; }
	HeaderNode *getHeader() const { return _header; }

	/* The header plus the sum of all row heights, computed from the model without nodes (one
	setRowHeightCallback() call per row). */
	float getIntrinsicHeight() const;

	/* Size to the whole model instead of scrolling. Disables the inner ScrollView and answers the
	measurement protocol with getIntrinsicHeight(), so `flex-basis: fit-content` works. The owner
	applies the size; this never writes its own contentSize. Every row is materialized, so use
	it for tens of rows, not thousands. */
	virtual void setAutoHeight(bool);
	bool isAutoHeight() const { return _autoHeight; }

	// Fires when getIntrinsicHeight() changes, for an outer ScrollController that caches sizes.
	virtual void setIntrinsicHeightCallback(Function<void(float)> &&);

	// Selection is off until one of these is set; only then do rows get an input listener and can
	// match `table-row:hover` / `table-row.selected`.
	virtual void setSelectCallback(RowEventFunction &&);
	virtual void setActivateCallback(RowEventFunction &&);
	virtual void setSelectionEnabled(bool);
	bool isSelectionEnabled() const { return _selectionEnabled; }

	virtual void setSelectedRow(size_t); // maxOf<size_t>() clears
	size_t getSelectedRow() const { return _selectedRow; }

	/* Join the scene-wide selection, as TreeView::setSelectionOwned. It also scopes the reorder
	keys to the table that owns the selection. */
	virtual void setSelectionOwned(bool);
	bool isSelectionOwned() const { return _selectionOwned; }

	// --- SelectionOwner ----------------------------------------------------

	virtual Node *getSelectionOwnerNode() override { return this; }
	virtual Node *resolveSelectionNode(const SelectionItem &) const override;
	virtual void handleSelectionChanged(SpanView<SelectionItem>) override;

	// Up and Down step through the rows; Left and Right leave the table
	virtual bool moveSelection(SelectionDirection) override;

	// The closest visible row to `fromWorld`
	virtual bool enterSelection(SelectionDirection, const Rect &fromWorld) override;

	// Re-derive the rows and re-request their data.
	virtual void invalidateSource();

	// Rebuild the row nodes at the start of the next visit. Coalesced and deferred: the rebuild can
	// destroy the node it is reached from, and nodes attached mid-frame are laid out on that frame.
	virtual void requestRebuildNodes(bool force = false);

	/* The same, with `cb` run once at the end of that rebuild, inside the visit, when new rows are
	already styled and laid out (Node::runPendingPhases). Callbacks coalesce and run in order; one
	that asks again is served by the next rebuild. Only rows in the scroll window get nodes. */
	virtual void requestRebuildNodes(Function<void()> &&cb, bool force = false);

	basic2d::ScrollView *getScroll() const { return _scroll; }
	basic2d::ScrollController *getController() const { return _controller; }

	// Row and cell rectangles in this node's space, also for rows without a node. False before the
	// first layout pass.
	bool getRowRect(size_t index, Rect &out) const;
	bool getCellRect(size_t row, size_t column, Rect &out) const;

	// Which row lies at a point, in content space (see ui::getRowIndexAt); maxOf<size_t>() outside
	// the content.
	size_t getRowIndexAt(const Vec2 &nodeLocation) const;

	// The boundary an insertion would snap to: 0..getRowCount(), never a row index.
	size_t getRowBoundaryAt(const Vec2 &nodeLocation, Rect *boundaryRect = nullptr) const;

	/* Reordering rows by dragging a grip, and by Alt+Up / Alt+Down.

	The caller declares the grip column under ReorderColumnKey; the view fills that cell with an
	icon and a DragSource but does not insert the column (that would shift the CSS track list).

	`to` is the row's final index, counted after removal from its old place (as
	data::Model::moveNode). The callback performs the move and returns false to refuse; on
	acceptance the selection follows the row. */
	static constexpr StringView ReorderColumnKey = StringView("__reorder");

	virtual void setReorderEnabled(bool);
	bool isReorderEnabled() const { return _reorderEnabled; }

	virtual void setReorderCallback(Function<bool(size_t from, size_t to)> &&);

	// Request a move as if the user had done it (used by the keyboard path). False when refused or
	// a no-op.
	virtual bool reorderRow(size_t from, size_t to);

protected:
	using Panel::init;

	virtual void handleSourceDirty(SubscriptionFlags);
	virtual void refresh();

	// Mark every span row's payload stale; explicit nodes read their payload from the model.
	void dropSpanData();

	// Resolve the column geometry for the current width and stamp it on the header and every live
	// row.
	virtual void resolveColumns();
	virtual void restampColumns();

	virtual void rebuildModel();
	virtual void requestRowData();
	virtual void handleSliceData(ModelNode *span, uint64_t first, size_t count,
			Map<uint64_t, Value> &);

	virtual void rebuildRows();

	// Put _selectedRow back on its row by identity after _rows is re-derived (also after a
	// reorder), as TreeView::remapSelection.
	void remapSelection();

	// The single mutator both setSelectedRow() and handleSelectionChanged() go through; see
	// TreeView::setSelectedIdentity
	void setSelectedIdentity(ItemId);

	void publishSelection();

	// (Re-)register the reorder chords on _reorderKeys with the flags ownership currently implies
	void bindReorderHotkeys();

	SelectionItem makeSelectionItem(size_t index) const;

	// A keyboard pick: selects, scrolls the row into view and reports it like a tap
	void selectRowFromKeyboard(size_t index);
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

	// Re-derive the intrinsic height and report it if it changed. No-op unless auto-height is on.
	virtual void updateIntrinsicHeight();

	HeaderNode *_header = nullptr; // outside the ScrollView, so it is sticky
	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	DataListener<Model> *_sourceListener = nullptr;

	Vector<Column> _columns;
	bool _reorderEnabled = false;
	Function<bool(size_t from, size_t to)> _reorderCallback;
	InputListener *_reorderKeys = nullptr;
	// Whether the drop target component is currently declared on this node
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
	// The last height reported through _intrinsicHeightCallback; nan() until the first report.
	float _reportedHeight = nan();
	size_t _selectedRow = maxOf<size_t>();

	// The selected identity; _selectedRow is derived from it on every rebuild (remapSelection)
	ItemId _selectedId = ItemId(0);

	bool _selectionOwned = false;

	// Set while applying a change that came from the system; see TreeView
	bool _applyingSelection = false;

	bool _autoHeight = false;
	bool _headerVisible = true;
	bool _selectionEnabled = false;
	bool _rebuildPending = false;
	bool _forceRebuild = false;
	bool _inDataRequest = false;

	// Callbacks for the next rebuild; taken off the list before they run.
	Vector<Function<void()>> _rebuildCallbacks;
};

// Chooses what a row looks like. Every setter is optional: a builder the callback never touches
// yields the standard row - one cell per column, each showing data[column.key].
//
// No height setter: the height is resolved before the row is built (see setRowHeightCallback).
class SP_PUBLIC TableView::RowBuilder {
public:
	TableView *getView() const { return _view; }
	const Row &getRow() const { return *_row; }
	size_t getIndex() const { return _index; }

	const Value &getData() const { return _row->getData(); }
	bool isLoaded() const { return _row->dataLoaded; } // false: the payload has not arrived yet
	bool isSelected() const;

	// The element behind the row and its external object. Null with no model; a span row returns
	// the span node.
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
