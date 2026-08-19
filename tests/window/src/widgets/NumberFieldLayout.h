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

#ifndef TESTS_WINDOW_SRC_WIDGETS_NUMBERFIELDLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_NUMBERFIELDLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiNumberField.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::NumberField: a text field that holds a number.
//
// Four fields, because the widget's claims are about the DIFFERENCES between them: a whole-number
// field that must refuse a fractional part, a real one that must not, a ranged one where typing
// past the end is refused and dragging past it is clamped, and one inside a ui::FormSystem where
// what is collected is a number rather than the text of one.
//
// Everything here is read back as numbers - the value, the text, the validity, the message and the
// callback count - because none of it is visible in a screenshot: a refused edit and an accepted
// one that happened to produce the same number look identical on screen.
class NumberFieldLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeField(ui::NumberField *) const;
	Value encodeState() const;

	ui::NumberField *getTarget(const Value &args) const;

	ui::NumberField *_integer = nullptr;
	ui::NumberField *_real = nullptr;
	ui::NumberField *_ranged = nullptr;
	ui::NumberField *_formField = nullptr;
	ui::FormSystem *_form = nullptr;

	// Per field, so that "the callback did not fire" is a statement about the field it is made
	// about rather than about the stand as a whole.
	Map<String, uint32_t> _callbacks;
	Map<String, double> _lastValue;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_NUMBERFIELDLAYOUT_H_
