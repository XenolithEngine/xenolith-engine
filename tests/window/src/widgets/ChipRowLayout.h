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

#ifndef TESTS_WINDOW_SRC_WIDGETS_CHIPROWLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_CHIPROWLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiChipRow.h"
#include "XLUiTextInput.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::ChipRow: several values that are one value.
//
// What is worth checking here is what a screenshot cannot show:
//
//   * that the ORDER survives everything - removing the middle chip, assigning from the form,
//     wrapping onto three lines;
//   * that the declared limits are declared: at the maximum the "+" is dead and opens nothing, and
//     with unique ids the options already in the row come up disabled IN THE MENU;
//   * that removal always has a visible target - Backspace with nothing selected selects rather
//     than deletes;
//   * that the row is ONE form field: one array under one name, one refusal when Required and
//     empty, and Tab that leaves it rather than walking its chips;
//   * that the height the row REPORTS is the height it draws at, at any width.
//
// THE FORM IS ON A CHILD NODE, not on this layout: a ui::FormSystem filters keys to its focused
// field's subtree, so the three standalone rows are deliberately kept out of it.
class ChipRowLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeChip(ui::Chip *, bool selected) const;
	Value encodeRow(ui::ChipRow *) const;
	Value encodeState() const;

	ui::ChipRow *getTarget(const Value &args) const;

	ui::ChipRow *_free = nullptr; // duplicates allowed, no limit
	ui::ChipRow *_limited = nullptr; // unique ids, at most three
	ui::ChipRow *_narrow = nullptr; // deliberately too narrow: this one wraps
	ui::ChipRow *_formRow = nullptr; // inside the form, Required

	// The field after the row in the tab ring: where Tab out of the row must land, and whose caret
	// must not move while the row holds the keyboard.
	ui::TextInput *_neighbour = nullptr;

	Node *_formPanel = nullptr;
	ui::FormSystem *_form = nullptr;

	Map<String, uint32_t> _callbacks;
	Map<String, uint32_t> _heightReports;
	Map<String, String> _lastValue;
	Value _lastSubmit;
	uint32_t _submitCount = 0;
	uint32_t _invalidCount = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_CHIPROWLAYOUT_H_
