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
//
// The CUSTOM target is the factory path, and it was added because that path was WRONG. An editor
// built by InlineEditorFactory is a node this widget knows nothing about, so it cannot read the
// value out of it - and until setCollectCallback existed, it did not try: every factory-built editor
// committed Nil, silently, because `collect` is optional and its absence looks exactly like an empty
// value. Nothing in the tree noticed, because nothing in the tree used a factory. The editor here is
// a ui::Checkbox on purpose: a widget with no text at all, so a value arriving through the stock
// text path could not be mistaken for a value arriving through the factory's.
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
	bool beginCustomEdit();

	ui::InlineEditTarget *_labelTarget = nullptr;
	basic2d::Label *_label = nullptr;
	ui::InlineEditTarget *_customTarget = nullptr;
	basic2d::Label *_custom = nullptr;
	ui::TableView *_table = nullptr;
	ui::TextInput *_neighbour = nullptr;

	Rc<data::Model> _model;
	Rc<ui::InlineEditSession> _cellSession;
	size_t _editedRow = maxOf<size_t>();

	Vector<String> _values;
	String _labelText;

	// What the factory's checkbox holds, and what it committed. The VALUE rather than its text:
	// "the commit carried a bool and not Nil" is the whole claim, and a string would hide it.
	bool _customValue = true;
	Value _lastCommitValue;

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

	/* What TableView::requestRebuildNodes(cb) answered, and what the watched row MEASURED at the
	moment it answered.

	The claim under test is not that the callback arrives - it is WHEN. A row built by that rebuild
	was attached while the frame was in flight, so it caught up on the visit's phases as it was
	attached; if that holds, the width recorded inside the callback is the row's final one, and a
	caller has nothing left to wait for. A callback that landed before the layout would record a
	zero, and one that landed a hop early would record a width that then changed. */
	uint32_t _rebuildAnswers = 0;
	size_t _rebuildWatchRow = maxOf<size_t>();
	float _rebuildRowWidth = 0.0f;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_INLINEEDITORLAYOUT_H_
