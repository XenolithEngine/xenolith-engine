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


#ifndef TESTS_WINDOW_SRC_WIDGETS_INLINEEDITORLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_INLINEEDITORLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiInlineEditor.h"
#include "XLUiTableView.h"
#include "XLUiTextInput.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::InlineEditor: an editor placed over a rectangle rather than inside the thing it edits.
//
// The stand carries three things because the widget has three separable claims.
//
// The LABEL is the plain case: an ui::InlineEditTarget on an ordinary node, double-clicked. It says
// nothing about lists and is where committing, cancelling and refusing are checked.
//
// The TABLE is the reason the widget exists. A ui::TableView virtualizes its rows: scrolling drops
// the node, and invalidateSource() rebuilds every one of them. An editor parented into a cell would
// be destroyed mid-edit, with the IME and the typed text going with it. Here the editor hangs off
// the TABLE and a rectangle, so a rebuild underneath must leave the edit untouched - and scrolling,
// which makes that rectangle point at a different row, must end it by KEEPING what was typed.
//
// The NEIGHBOUR is a text field outside the overlay: while an editor is open its exclusive focus
// group owns the keyboard, and this field must not see a single key.
class InlineEditorLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;

	void rebuildModel();

	// The cell's rectangle in the table's space. What ui::TableView::getCellRect will answer once
	// it exists; until then a stand has to derive it from the row node, which is exactly why it
	// only works for a row that is currently built.
	bool getCellRect(size_t row, Rect &out) const;

	bool beginLabelEdit();
	bool beginCellEdit(size_t row);

	ui::InlineEditTarget *_labelTarget = nullptr;
	basic2d::Label *_label = nullptr;
	ui::TableView *_table = nullptr;
	ui::TextInput *_neighbour = nullptr;

	Rc<data::Model> _model;
	Rc<ui::InlineEditSession> _cellSession;
	size_t _editedRow = maxOf<size_t>();

	Vector<String> _values;
	String _labelText;

	// Every ending is counted separately: "commit arrives exactly once" is a claim about numbers.
	uint32_t _commits = 0;
	uint32_t _cancels = 0;
	uint32_t _closes = 0;
	uint32_t _refusals = 0;
	String _lastCommit;

	// While set, every commit is refused with this reason - the "a refused edit stays open" case.
	bool _refuse = false;

	// What the cell editor is opened with, so that the option can be seen to do something.
	bool _closeOnScroll = true;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_INLINEEDITORLAYOUT_H_
