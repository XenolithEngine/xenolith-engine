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


#ifndef TESTS_WINDOW_SRC_WIDGETS_SEARCHPICKERLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SEARCHPICKERLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiSearchPicker.h"
#include "XLUiSearchSystem.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::SearchPicker over a ui::SearchSystem.
//
// The surface is here TWICE on purpose. One SearchPickerContent is parented straight into the
// layout, which is what makes the widget checkable at all: a query can be typed, a selection walked
// and a highlight read back with no window in the picture. The SearchPicker beside it is the same
// surface behind a control that opens a real popup, so that the two paths cannot drift apart
// unnoticed.
//
// The item list carries the two strings that break naive implementations: a name led by two emoji,
// where a UTF-16 offset and a code point index disagree, and a name with the Turkish dotted capital
// I, whose lowercase form is LONGER in bytes than the original.
class SearchPickerLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeContent(ui::SearchPickerContent *) const;

	// The DISPLAY of the grouped surface: rows as they are shown, which is not the hit list -
	// a category is a row standing for no hit, and expanding one shifts every row after it.
	Value encodeRows(ui::SearchPickerContent *) const;

	Value encodeState() const;

	void rebuildSource();

	// A fresh grouped surface, opened on `highlight` (empty for none). See the definition.
	void buildGrouped(StringView highlight);

	ui::SearchSystem *_search = nullptr;

	// What every surface here is built from, kept so the grouped one can be rebuilt on a value.
	ui::SearchPickerConfig _baseConfig;
	ui::SearchPickerContent *_content = nullptr;

	// The same widget with `grouped` on: a tree of categories while the query is empty, a flat
	// ranked list as soon as it is not. Beside the flat one so the two cannot drift apart.
	ui::SearchPickerContent *_grouped = nullptr;
	ui::SearchPicker *_picker = nullptr;
	ui::TextInput *_neighbour = nullptr;

	ui::SearchMatchMode _mode = ui::SearchMatchMode::Subsequence;
	bool _typoTolerance = false;

	uint32_t _activations = 0;
	String _lastActivation;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SEARCHPICKERLAYOUT_H_
