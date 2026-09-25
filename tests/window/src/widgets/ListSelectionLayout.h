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


#ifndef TESTS_WINDOW_SRC_WIDGETS_LISTSELECTIONLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_LISTSELECTIONLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTableView.h"
#include "XLUiTreeView.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A table and a tree in the multiple selection mode, a table in the single mode beside them, and a
// text field: presses with Shift and Ctrl, Shift+Up/Down and Ctrl+A, and what the scene's selection
// makes of a set of rows.
class ListSelectionLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;

	template <typename View>
	Value encodeView(View *) const;

	Rc<data::Model> makeTableModel(StringView prefix) const;
	Rc<data::Model> makeTreeModel() const;

	ui::TableView *_table = nullptr;
	ui::TreeView *_tree = nullptr;
	ui::TableView *_single = nullptr;
	ui::TextInput *_field = nullptr;

	uint32_t _selects = 0;
	uint32_t _activates = 0;
	String _lastOp;
	bool _lastKeyboard = false;
	Vector<int64_t> _lastRows;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_LISTSELECTIONLAYOUT_H_
