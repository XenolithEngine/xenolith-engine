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

#ifndef TESTS_WINDOW_SRC_WIDGETS_COLORFIELDLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_COLORFIELDLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiColorField.h"
#include "XLUiTextInput.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::ColorField: a swatch, its hex, and two pickers.
//
// Everything worth checking here is a difference that a screenshot cannot show:
//
//   * WHICH picker a tap opens. Headless advertises no colour dialog at all, so `Auto` must resolve
//     to the built-in surface - and `System`, asked for explicitly, must fail in a way that says so
//     instead of opening nothing and going quiet;
//   * that the hex line reads what sprt::geom::readColor reads (#f0a, #ff00aa and rgb() are one
//     colour) rather than a second parser written for this widget;
//   * that Enter and blur mean different things about a refusal;
//   * that the field is ONE form field whose value is text.
//
// THE FORM IS ON A CHILD NODE, not on this layout: a ui::FormSystem filters keys to its focused
// field's subtree, so the two standalone fields are deliberately kept out of it.
class ColorFieldLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeField(ui::ColorField *) const;
	Value encodeState() const;

	ui::ColorField *getTarget(const Value &args) const;

	ui::ColorField *_plain = nullptr; // rgb, standalone
	ui::ColorField *_alpha = nullptr; // rgba, its own palette, standalone
	ui::ColorField *_formField = nullptr; // inside the form

	// The field after the colour in the tab ring: where Tab out of the hex line must land, and
	// whose caret must not move while the colour holds the keyboard.
	ui::TextInput *_neighbour = nullptr;

	Node *_formPanel = nullptr;
	ui::FormSystem *_form = nullptr;

	Map<String, uint32_t> _callbacks;
	Map<String, String> _lastValue;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_COLORFIELDLAYOUT_H_
