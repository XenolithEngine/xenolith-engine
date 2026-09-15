/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SELECTIONNAVLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SELECTIONNAVLAYOUT_H_

#include "app/TestLayout.h"
#include "XLSelectionSystem.h"
#include "XLInputListener.h"
#include "XLUiTreeView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* Verification layout for arrow navigation of the scene's selection (SelectionSystem::moveSelection).

A 3x3 grid of plain selectable nodes, a hidden one under it, and two trees opted in as owners to
the right: `tree` left-to-right, `tree-rtl` below it right-to-left. An arrow listener that can be
told to take the keys, and an Exclusive focus group that can be shown, cover the two ways the
arrows must not reach the selection. The key-driven cases live in selection-nav-check.py. */
class SelectionNavLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView what);
	void runChecks();

	Rc<data::Model> makeModel(StringView prefix, size_t items) const;

	ui::TreeView *findTree(StringView) const;
	Value encodeTree(ui::TreeView *) const;
	Value encodeState() const;

	Node *_grid[3][3] = {};
	Node *_hidden = nullptr;

	ui::TreeView *_tree = nullptr;
	ui::TreeView *_treeRtl = nullptr;

	Node *_eater = nullptr;
	bool _eat = false;
	size_t _eaten = 0;

	Node *_modal = nullptr;

	// An empty container a selected cell can be moved into, for the chain to follow
	Node *_box = nullptr;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SELECTIONNAVLAYOUT_H_
