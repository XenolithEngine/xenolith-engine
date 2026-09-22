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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEBROWSERVIEW_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEBROWSERVIEW_H_

#include "fileexplorer/IconGridView.h"
#include "XLUiTableView.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The right pane: one directory, shown either as tiles or as a table.

One ui::FilesystemModel per navigated directory, and a fresh one each time. FilesystemModel
installs its lazy-listing callback on the ROOT, and this pane reads exactly one level, so a model
rooted at the directory being shown is the whole of what it needs - and dropping the old one drops
the listing, the stats and the nodes of the directory being left.

Both views are built once and kept; only the active one is given the model. Switching modes
therefore costs a rebuild of the nodes and never a second walk of the directory. */
class FileBrowserView : public ui::Panel {
public:
	enum class Mode {
		Icons,
		Table,
	};

	using PathFunction = Function<void(StringView)>;

	virtual ~FileBrowserView() = default;

	virtual bool init(ThumbnailCache *);

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

	/* Show `path`. A directory that cannot be read leaves the pane where it was and answers
	false, so a click on something that has gone away does not empty the window. */
	bool navigate(StringView path);

	// One level up, unless the pane is already at the root of the filesystem.
	bool navigateUp();

	StringView getPath() const { return _path; }

	void setMode(Mode);
	Mode getMode() const { return _mode; }

	// The side of a tile's picture in Icons mode.
	void setIconSize(float);
	float getIconSize() const { return _iconSize; }

	// The height of a table row, and the source of the table's font metrics.
	void setRowHeight(float);
	float getRowHeight() const { return _rowHeight; }

	void setShowHidden(bool);
	bool isShowHidden() const { return _showHidden; }

	// Re-walk the current directory. Surviving entries keep their nodes, and with them the
	// selection and the thumbnails already decoded for them.
	void refresh();

	// What the MODEL holds, and what the active view has derived from it. They differ for a frame:
	// a listing lands on the app thread, and the view picks it up in the components phase after.
	size_t getEntryCount() const;
	size_t getViewRowCount() const;
	uint32_t getColumnCount() const { return _grid ? _grid->getColumnCount() : 0; }

	// What the pane is showing, for the inspector and the self-check.
	Value encodeEntries() const;

	size_t getSelectedIndex() const;
	StringView getSelectedPath() const;
	bool selectPath(StringView);

	// Told after every successful navigation, with the new path.
	void setNavigateCallback(PathFunction &&);

	ui::TableView *getTable() const { return _table; }
	IconGridView *getGrid() const { return _grid; }

protected:
	static constexpr float HeaderHeight = 36.0f;

	// Beyond this many components the leading ones are replaced by a single ellipsis crumb: the
	// bar does not scroll, and a path deep enough to fill it is exactly when the trailing
	// components are the ones worth seeing.
	static constexpr size_t MaxCrumbs = 4;

	void buildHeader();

	// Only the visible view is a candidate for the scene's selection, and only once it is in a
	// scene at all: opting in earlier asks for a SelectionSystem that does not exist yet.
	void applySelectionOwnership();
	void rebuildBreadcrumbs();
	void applyModel();
	void handleActivate(ui::FilesystemModel::Node *);

	// "12.4 MiB" and a local date, for the two columns that are not the name.
	static String formatSize(int64_t bytes);
	static String formatTime(int64_t micros);

	ThumbnailCache *_cache = nullptr;

	Node *_header = nullptr;
	ui::Button *_upButton = nullptr;
	Node *_breadcrumbs = nullptr;

	IconGridView *_grid = nullptr;
	ui::TableView *_table = nullptr;

	Rc<ui::FilesystemModel> _model;
	PathFunction _navigateCallback;

	String _path;
	Mode _mode = Mode::Icons;
	float _iconSize = 96.0f;
	float _rowHeight = 26.0f;
	bool _showHidden = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEBROWSERVIEW_H_
