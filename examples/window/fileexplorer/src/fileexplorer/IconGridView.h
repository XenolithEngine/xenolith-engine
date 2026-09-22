/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_ICONGRIDVIEW_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_ICONGRIDVIEW_H_

#include "fileexplorer/FileTile.h"
#include "XLUiPanel.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"
#include "XLSelectionSystem.h"
#include "XLUiRowGeometry.h"
#include "XLSubscriptionListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* A scrolled, virtualized grid of tiles over a ui::FilesystemModel.

The engine virtualizes a TreeView and a TableView with one basic2d::ScrollController item per row,
and there is no grid over data::Model yet. This is that grid, and it is built the same way for a
reason: the controller's culling is ONE-DIMENSIONAL - it projects an item's position and size onto
the scroll axis and never looks at the other one - so an item per file would cost N times the
per-scroll bookkeeping for exactly the same set of materialized nodes. An item per grid ROW also
keeps ui::RowGeometrySource usable, and with it scrollRowIntoView and getEnteringRow, which is what
makes keyboard navigation between this pane and the tree cheap.

Two rebuild paths, because there are two unrelated causes:

  * the width changed, so the column count may have - ScrollController::setRebuildCallback, which
    the controller invokes from its own content-size handling and which is the only path that
    keeps the user's relative scroll position across the reflow;
  * the model or a setting changed - markComponentsDirty(), coalesced into the components phase,
    exactly as TableView::requestRebuildNodes does it.

Row nodes are pooled and re-pointed rather than rebuilt: a tile knows how to change the entry it
shows, so scrolling costs a few assignments per row and no allocation. */
class IconGridView : public ui::Panel, public SelectionOwner {
public:
	using Model = ui::FilesystemModel;
	using ModelNode = Model::Node;

	class TileRowNode;

	// What a row node was built from. Everything that changes a row's shape is here, so a change
	// of tile size or of column count invalidates every pooled row by itself.
	struct RowKey {
		size_t firstIndex = 0;
		size_t count = 0;
		uint32_t columns = 0;
		float cellWidth = 0.0f;
		float cellHeight = 0.0f;
		float iconSize = 0.0f;

		bool operator==(const RowKey &) const = default;
	};

	using IndexFunction = Function<void(size_t index, ModelNode *)>;

	virtual ~IconGridView() = default;

	virtual bool init(ThumbnailCache *);

	virtual void handleContentSizeDirty() override;

	// Replaces the listing. The grid reads one level - the root's children - so a navigation is a
	// new model rather than a new root.
	void setSource(Model *);
	Model *getSource() const { return _source; }

	// The side of a tile's picture. The tile box is derived from it, so this is the one number
	// the icon-size setting writes.
	void setIconSize(float);
	float getIconSize() const { return _iconSize; }

	// The band under the picture. It follows the text size, or a larger font would write its
	// second line outside the tile.
	void setLabelHeight(float);
	float getLabelHeight() const { return _labelHeight; }

	uint32_t getColumnCount() const { return _columns; }
	size_t getEntryCount() const { return _entries.size(); }
	ModelNode *getEntry(size_t) const;
	size_t getIndexForPath(StringView) const;

	void setSelectCallback(IndexFunction &&);
	void setActivateCallback(IndexFunction &&);

	void setSelectedIndex(size_t); // maxOf<size_t>() clears
	size_t getSelectedIndex() const { return _selectedIndex; }

	// Join the scene-wide selection, as TableView::setSelectionOwned does.
	void setSelectionOwned(bool);
	bool isSelectionOwned() const { return _selectionOwned; }

	// --- SelectionOwner ----------------------------------------------------

	virtual Node *getSelectionOwnerNode() override { return this; }
	virtual Node *resolveSelectionNode(const SelectionItem &) const override;
	virtual void handleSelectionChanged(SpanView<SelectionItem>) override;

	// Left and Right step by one, Up and Down by a whole row.
	virtual bool moveSelection(SelectionDirection) override;
	virtual bool enterSelection(SelectionDirection, const Rect &fromWorld) override;

	// Rebuild the tiles at the start of the next visit; coalesced.
	void requestRebuildNodes(bool force = false);

	// Called by a tile when it is tapped; `count` is 1 for a select and 2 for an activate.
	void handleTileTap(size_t index, uint32_t count);

protected:
	static constexpr float CellPadding = 10.0f; // around the picture, inside the tile
	static constexpr float CellGap = 6.0f; // between tiles
	static constexpr float DefaultLabelHeight = 34.0f;

	void handleSourceDirty(SubscriptionFlags);
	void rebuildModel();
	bool rebuildTiles();
	void updateMetrics();

	RowKey makeRowKey(size_t rowIndex) const;
	Rc<Node> makeRow(size_t rowIndex);
	Rc<TileRowNode> takeReusableRow(const RowKey &);

	ui::RowGeometrySource makeGeometrySource() const;
	void applySelectionToTiles();
	void publishSelection();
	SelectionItem makeSelectionItem(size_t index) const;
	void selectFromKeyboard(size_t index);

	Rc<Model> _source;
	ThumbnailCache *_cache = nullptr;

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	DataListener<Model> *_sourceListener = nullptr;

	Vector<Rc<ModelNode>> _entries;
	Vector<Rc<TileRowNode>> _reusableRows;

	IndexFunction _selectCallback;
	IndexFunction _activateCallback;

	size_t _selectedIndex = maxOf<size_t>();
	size_t _rowCount = 0;
	uint32_t _columns = 1;

	float _iconSize = 96.0f;
	float _labelHeight = DefaultLabelHeight;
	float _cellWidth = 0.0f;
	float _cellHeight = 0.0f;

	bool _rebuildPending = false;
	bool _forceRebuild = false;
	bool _selectionOwned = false;
	bool _applyingSelection = false;
};

/* One row of the grid: the node a ScrollController item builds, and the owner of its tiles.

It places its own tiles, so it carries SystemManagedLayout and no LayoutSystem - a uniform grid
needs no track resolution, and a stylesheet must not become a second writer of the tiles' boxes. */
class IconGridView::TileRowNode : public ui::Panel {
public:
	virtual ~TileRowNode() = default;

	virtual bool init(IconGridView *, ThumbnailCache *, size_t tileCount);

	virtual void handleContentSizeDirty() override;

	// Re-point the tiles at the entries of `rowIndex`. The row node itself is never rebuilt for
	// this, which is what makes scrolling allocation-free.
	void update(size_t rowIndex, const RowKey &);

	const RowKey &getRowKey() const { return _key; }
	SpanView<Rc<FileTile>> getTiles() const { return _tiles; }

protected:
	IconGridView *_view = nullptr;
	Vector<Rc<FileTile>> _tiles;
	RowKey _key;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_ICONGRIDVIEW_H_
