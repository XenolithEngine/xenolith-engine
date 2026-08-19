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

#ifndef TESTS_WINDOW_SRC_WIDGETS_VECTORFIELDLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_VECTORFIELDLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiVectorField.h"
#include "XLUiTextInput.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::VectorField: several ui::NumberFields that are ONE value.
//
// The claims worth checking are all about the SEAM between the row and everything around it, and
// none of them are visible in a screenshot:
//
//   * one form field, not N - what is collected is one array under one name;
//   * Tab walks the components and leaves the row at its ends, and Shift+Tab ENTERING the row
//     lands on its last component (which is what FormFieldSlots::setFocused's `backwards` is for);
//   * a refusal in one component marks the ROW and names the component, and leaves the other
//     components alone;
//   * changing the arity keeps the values that still have an index.
//
// THE FORM IS ON A CHILD NODE, not on this layout. A ui::FormSystem is the focus group for every
// listener beneath it, and once it has focused a field it filters keys to that field's subtree -
// so the two standalone rows are deliberately kept OUT of the form's subtree, and what happens to
// them stays a statement about the widget rather than about the form above it.
class VectorFieldLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeField(ui::VectorField *) const;
	Value encodeState() const;

	ui::VectorField *getTarget(const Value &args) const;

	ui::VectorField *_real = nullptr; // Vec3, fractional, standalone
	ui::VectorField *_ranged = nullptr; // Vec2, whole, 0..999, standalone
	ui::VectorField *_formField = nullptr; // Vec4, inside the form

	// The field after the row in the tab ring: where Tab out of the last component must land, and
	// whose caret must not move while the row holds the keyboard.
	ui::TextInput *_neighbour = nullptr;

	Node *_formPanel = nullptr;
	ui::FormSystem *_form = nullptr;

	// Per row, so that "the callback did not fire" is a statement about the row it is made about.
	Map<String, uint32_t> _callbacks;
	Map<String, Value> _lastValue;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_VECTORFIELDLAYOUT_H_
