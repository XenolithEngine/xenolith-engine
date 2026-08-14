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

#ifndef TESTS_WINDOW_SRC_APP_TESTTREELAYOUT_H_
#define TESTS_WINDOW_SRC_APP_TESTTREELAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTreeView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The whole test menu, as one tree.
//
// The registry is a tree - `src/css/NthChildLayout.cpp` is `css/nth` - and this shows it as one
// instead of as a stack of pushed levels: a group is an expandable row, a test is a leaf, and a
// tap on a leaf opens it. The depth a reader has to hold in their head is what is on screen, not
// how many times they pressed "Go back".
//
// The model is a data::Model built straight from the registry: a group becomes a Category node, a
// test becomes an Item node, and the subgroups are added before the tests because that is the order
// the menu wants - not, as with a data::Source, because it was the only order available. The
// registry is a static table of a few dozen entries, so the whole tree is built up front; the
// lazy-children hook exists for a model that has to go and look, and this one does not.
//
// It is a TestLayout for the caption and the stylesheet: ui::TreeView takes its row geometry from
// CSS, and this app has no global sheet, so the layout carries its own.
class TestTreeLayout : public TestLayout {
public:
	virtual ~TestTreeLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

protected:
	// A tap on a group opens or closes it; a tap on a test pushes that test over this menu.
	void handleRowSelected(size_t index, const ui::TreeView::Row &);

	// Drives the menu from the inspector socket, the way `layouts`/`layout` drive the registry:
	// menu.list reads the visible rows, menu.toggle opens or closes one.
	void addInspectorCommands(Scene *);

	ui::TreeView *_tree = nullptr;
	ui::Button *_back = nullptr;
	Vector<String> _inspectorCommands;
	Scene *_inspectorScene = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_APP_TESTTREELAYOUT_H_
