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


#ifndef TESTS_WINDOW_SRC_WIDGETS_MARQUEELAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_MARQUEELAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTableView.h"
#include "XLUiTreeView.h"
#include "XLUiMarquee.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A grid of tiles of its own over a ScrollView, swept by a ui::MarqueeSystem the way an application
// would compose one: hits by tile rectangles in content space, the preview through ui::ListSweep.
class MarqueeTileGrid : public Node {
public:
	static constexpr uint32_t Columns = 4;
	static constexpr size_t Count = 40;
	static constexpr float TileWidth = 60.0f;
	static constexpr float TileHeight = 40.0f;
	static constexpr float Gap = 8.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	basic2d::ScrollView *getScroll() const { return _scroll; }
	ui::MarqueeSystem *getMarquee() const { return _marquee; }

	const ui::ListSelectionState &getState() const { return _state; }
	const ui::ListSweep &getSweep() const { return _sweep; }
	uint32_t getCommits() const { return _commits; }

	// A tile's rectangle in the scroll root's space; false before the rows are placed.
	bool getTileRect(size_t, Rect &) const;

	// The tiles on screen, and whether each is drawn as selected.
	void foreachTile(const Callback<void(size_t, basic2d::Layer *)> &) const;

	void setSelected(SpanView<size_t>);

protected:
	Rc<Node> makeRow(size_t row);
	void syncTiles();

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;
	ui::MarqueeSystem *_marquee = nullptr;

	ui::ListSelectionState _state;
	ui::ListSweep _sweep;
	uint32_t _commits = 0;
};

/* A table (with a reorder grip and a header), a tree and a grid of tiles, each with a band; a panel
drawn over a corner of the table. What a band covers is lit before the release, which applies it
once: plain replaces, Ctrl toggles, Shift adds. */
class MarqueeLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;

	template <typename View>
	Value encodeView(View *) const;
	Value encodeGrid() const;

	Rc<data::Model> makeTableModel() const;
	Rc<data::Model> makeTreeModel() const;
	bool applyMove(size_t from, size_t to);

	Rc<data::Model> _tableModel;
	ui::TableView *_table = nullptr;
	ui::TreeView *_tree = nullptr;
	MarqueeTileGrid *_grid = nullptr;
	Node *_cover = nullptr;

	uint32_t _selects = 0;
	uint32_t _sweeps = 0;
	uint32_t _reorders = 0;
	uint32_t _activates = 0;
	uint32_t _treeSweeps = 0;
	uint32_t _coverTaps = 0;
	String _lastSweepOp;
	Vector<int64_t> _lastSweepRows;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_MARQUEELAYOUT_H_
