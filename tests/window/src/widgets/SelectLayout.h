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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SELECTLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SELECTLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiSelect.h"
#include "XLUiTextInput.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::Select: a closed control that opens a real menu surface.
//
// The stand carries three things because the widget has three separable claims to check. The first
// Select is the plain one - a list with a disabled option in the middle, so that stepping and the
// keyboard have something to skip over. The second lives inside a ui::FormSystem, which is where
// "the form collects the id, not the title" is checkable. The ui::TextInput beside them is the
// neighbour: while the list is open it must NOT receive the arrows, and that is the whole point of
// the menu's focus group.
class SelectLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeSelect(ui::Select *) const;
	Value encodeState() const;

	ui::Select *getTarget(const Value &args) const;

	ui::Select *_select = nullptr;
	ui::Select *_formSelect = nullptr;
	ui::TextInput *_neighbour = nullptr;
	ui::FormSystem *_form = nullptr;

	uint32_t _changes = 0;
	String _lastChange;
	Vector<String> _changeLog;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SELECTLAYOUT_H_
