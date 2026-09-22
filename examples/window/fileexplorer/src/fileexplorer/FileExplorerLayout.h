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

#ifndef EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLAYOUT_H_
#define EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLAYOUT_H_

#include "fileexplorer/FileBrowserView.h"
#include "fileexplorer/FilePlaces.h"
#include "XL2dSceneLayout.h"
#include "XLUiDockSystem.h"
#include "XLUiTreeView.h"
#include "XLUiSlider.h"
#include "XLUiCheckbox.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* A two-pane file navigator, and the first consumer of ui::FilesystemModel.

What it is for, and how to read it:

  * ONE ui::DockSystem with a single horizontal split: the places tree on the left, the browser on
    the right. The dock is here because it is the honest way to get two panes the user can resize
    by dragging the divider between them; neither pane knows it exists. Both panels are permanent
    and neither closable nor movable - the example is not about the dock.
  * TWO ui::FilesystemModel instances with different jobs. The left one has several roots, lists
    directories only and takes no stat(); the right one is rebuilt per directory and carries
    everything a listing shows. One class, two configurations, which is the point.
  * The right pane switches between a table and a grid of tiles. The table is ui::TableView; the
    grid is IconGridView, which this example adds because the engine virtualizes rows and not
    cells.
  * Two settings drive the look: the picture size, which is a number the grid lays out with, and
    the text size, which is a CSS custom property this layout declares and the sheet reads. They
    live for the session only - nothing here writes to disk.

The stylesheet is installed on the scene CONTENT by main.cpp rather than here, because a context
menu opens beside the layout rather than under it. */
// The sheet, handed to main.cpp so it can be installed on the scene content rather than here.
StringView getFileExplorerStylesheet();

class FileExplorerLayout : public basic2d::SceneLayout2d {
public:
	virtual ~FileExplorerLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

protected:
	static constexpr float BarHeight = 46.0f;
	static constexpr float MinIconSize = 48.0f;
	static constexpr float MaxIconSize = 192.0f;
	static constexpr float IconSizeStep = 16.0f;
	static constexpr float MinFontSize = 10.0f;
	static constexpr float MaxFontSize = 20.0f;

	// What the two sliders and the two toggles are worth. Session-only by design: an example that
	// remembered its state would start in a state the reader did not choose.
	struct Settings {
		FileBrowserView::Mode mode = FileBrowserView::Mode::Icons;
		float iconSize = 96.0f;
		float fontSize = 13.0f;
		bool showHidden = false;
	};

	void buildDock();
	void buildToolbar();
	ui::Button *makeToolButton(StringView label, int16_t order, Function<void()> &&);

	Rc<ui::TreeView> makePlacesTree();

	void setMode(FileBrowserView::Mode);
	void setIconSize(float);
	void setFontSize(float);
	void setShowHidden(bool);

	// The text size reaches the widgets as an inherited custom property rather than as a call on
	// every label: a virtualized row is built and destroyed as the user scrolls, and only the
	// cascade is there for all of them.
	void applyFontSize();

	// Where the pane opens: the home directory, or the root of the filesystem when there is none.
	static String getStartPath();

	void navigate(StringView path);

	// --- the inspector ---------------------------------------------------
	void addInspectorCommands(Scene *);
	void removeInspectorCommands();
	Value encodeState() const;
	Value encodePlaces() const;

	// --- the self-check --------------------------------------------------
	// What can be asserted without pixels: the places, the listing and its order, and that each
	// setting moves what it claims to move. Prints "N checks, M failures".
	void runSelfCheck();
	void performChecks();
	void expect(bool, StringView);

	Settings _settings;

	Node *_background = nullptr;
	Node *_toolbar = nullptr;
	Node *_dockRoot = nullptr;

	ui::DockSystem *_dock = nullptr;
	ui::Button *_iconsButton = nullptr;
	ui::Button *_tableButton = nullptr;
	ui::Button *_localeButton = nullptr;
	ui::Slider *_iconSlider = nullptr;
	ui::Slider *_fontSlider = nullptr;
	ui::Checkbox *_hiddenCheck = nullptr;

	Rc<ThumbnailCache> _thumbnails;
	Rc<ui::FilesystemModel> _placesModel;

	// Raw: the dock owns both panel nodes and outlives these pointers.
	ui::TreeView *_places = nullptr;
	FileBrowserView *_browser = nullptr;

	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_FILEEXPLORER_SRC_FILEEXPLORER_FILEEXPLORERLAYOUT_H_
