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


#ifndef TESTS_WINDOW_SRC_WIDGETS_TABLEVIEWLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_TABLEVIEWLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTableView.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::TableView: row geometry, and reordering rows by grip and by keyboard.
//
// Two claims are being checked here and neither is visible in a frame.
//
// GEOMETRY answers for a row that has NO NODE. Only the nodes of a TableView are virtualized;
// rebuildRows() commits one controller item per row with the height it resolved beforehand. So a
// row scrolled far out of sight still has a rectangle, and that is precisely the case the insertion
// line and the drop index are derived from - which is why this lives in the engine rather than in
// whichever application wanted to drag a row.
//
// REORDER is a QUESTION the view asks and the owner answers: the callback returns false to refuse,
// and nothing about the order or the selection moves. When it accepts, the selection has to follow
// the ROW rather than stay on the index it used to occupy - a distinction that looks identical on
// screen for one frame and then diverges forever.
//
// The neighbouring text field is here to prove that Alt+Up in a table with nothing selected is not
// swallowed: it goes to whoever is below.
class TableViewLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;
	Value encodeRect(const Rect &) const;

	void rebuildModel();
	bool applyMove(size_t from, size_t to);

	ui::TableView *_table = nullptr;
	ui::TextInput *_neighbour = nullptr;

	Rc<data::Model> _model;
	Vector<String> _values;

	uint32_t _moves = 0;
	uint32_t _refusals = 0;
	String _lastMove;

	// While set, every reorder is refused - the "a refusal changes nothing" case.
	bool _refuse = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_TABLEVIEWLAYOUT_H_
