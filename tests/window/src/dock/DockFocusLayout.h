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

#ifndef TESTS_WINDOW_SRC_DOCK_DOCKFOCUSLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_DOCKFOCUSLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"
#include "XLUiTreeView.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* The current frame: which dock frame the scene's selection runs through, and the outline drawn
over its content.

An outer dock with a tree (a selection owner) on the left and a nested dock on the right, whose top
frame holds a selectable canvas and whose bottom frame holds a plain panel. Every panel is opaque,
so an outline painted by the frame itself would be hidden. The plain panel holds a text input:
taking focus selects the frame it is in. Presses select
(SelectionSystem::setSelectOnPress); dock-focus-check.py drives it. */
class DockFocusLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleEnter(Scene *) override;

protected:
	virtual void registerCommands() override;

	Rc<data::Model> makeModel() const;

	Value encodeFrame(ui::DockSystem *, StringView name) const;
	Value encodeState() const;
	Value encodeProbe() const;

	Node *_root = nullptr;
	ui::DockSystem *_outer = nullptr;
	ui::DockSystem *_inner = nullptr;

	ui::TreeView *_tree = nullptr;
	Node *_canvas = nullptr;
	Node *_plain = nullptr;
	ui::TextInput *_input = nullptr;

	size_t _selects = 0;
	size_t _keyboardSelects = 0;
	size_t _activations = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_DOCKFOCUSLAYOUT_H_
