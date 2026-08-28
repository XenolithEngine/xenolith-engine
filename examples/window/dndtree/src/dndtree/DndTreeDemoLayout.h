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

#ifndef EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEDEMOLAYOUT_H_
#define EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEDEMOLAYOUT_H_

#include "dndtree/DndTreeView.h"
#include "XL2dSceneLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// Two trees, parked side by side, whose rows travel between them by drag & drop.
//
// What it shows, and how to read it:
//   * ONE ui::DockSystem with a single horizontal split - a "Library" frame on the left and a
//     "Project" frame on the right. The dock is here because it is the honest way to get two
//     parking places the user can resize by dragging the divider between them; nothing in the drag
//     & drop below knows it exists. The panels themselves are neither closable nor movable, so the
//     only drag in this app is the one it is about.
//   * TWO ui::TreeView widgets over TWO SEPARATE data::Model trees. Two models rather than one is
//     the whole point: an ItemId belongs to the model that allocated it, so a row that stays in
//     its own tree is MOVED (identity, expansion and selection survive) while a row that crosses
//     over has to be rebuilt on the other side and deleted here. Both cases are one drag.
//   * The drag itself is stock: a DragSource and a DropTarget per row, plus one DropTarget on each
//     view's background so an empty tree - "Scene B" starts out empty - is still somewhere to drop.
//     Ctrl asks for a Copy, Shift for a Move, and the target has the last word.
//
// The content is generated in code (makeLibraryModel / makeProjectModel), and the self-check drives
// the very same transfer path the pointer does, so what it proves is what a drop actually does.
class DndTreeDemoLayout : public basic2d::SceneLayout2d {
public:
	virtual ~DndTreeDemoLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

protected:
	// Build the two models, the dock and the frames. Called by init(), and again by the Reset
	// button - which is what makes a demo that has been dragged into a mess recoverable.
	void buildDock();

	// One configured tree over one model. The dock's panel builder calls this on first show and the
	// node it returns is kept across every move, so this runs exactly twice.
	Rc<DndTreeView> makeTree(data::Model *, StringView title);

	void makeControlBar();
	ui::Button *makeControl(StringView label, Function<void()> &&action);

	// An empty `lastAction` recomputes the counts and keeps whatever was last said - which is what
	// the source half of a drag needs, since it takes the original away one step AFTER the target
	// has already reported the drop.
	void refreshStatus(StringView lastAction = StringView());

	// The demo's starting state: fresh content in both trees, opened, with the frames and the dock
	// left as they are. Also what the caller of runSelfCheck() uses to undo what it did.
	void resetContent();

	DndTreeView *viewByName(StringView) const; // "left" / "right", for the inspector commands

	// --- self-check ------------------------------------------------------
	// Everything a drop does, without a pointer: where a row answers that a drop would land, which
	// drops are refused, and what each of the three transfers (move inside one model, move across
	// two, copy) leaves behind. Prints "N checks, M failures" on completion, and leaves the models
	// where its transfers put them - the caller restores with resetContent().
	void runSelfCheck();

	void addInspectorCommands(Scene *);

	Rc<data::Model> _libraryModel;
	Rc<data::Model> _projectModel;

	ui::DockSystem *_dock = nullptr;

	Node *_background = nullptr; // full-bleed backdrop behind everything
	Node *_dockRoot =
			nullptr; // flat owner: every DockFrame is its direct child, no layout of its own
	Node *_controlBarRow = nullptr;
	basic2d::Label *_statusLabel = nullptr;
	String _lastAction;

	DndTreeView *_left = nullptr; // raw: the dock owns both panel nodes and outlives this pointer
	DndTreeView *_right = nullptr;

	Scene *_inspectorScene = nullptr;
	Vector<String> _inspectorCommands;

	size_t _checks = 0;
	size_t _failures = 0;
	bool _selfCheckDone = false;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEDEMOLAYOUT_H_
