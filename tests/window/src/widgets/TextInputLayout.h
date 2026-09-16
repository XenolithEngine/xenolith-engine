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

#ifndef TESTS_WINDOW_SRC_WIDGETS_TEXTINPUTLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_TEXTINPUTLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::TextInput: caret, selection, placeholder, password masking, read-only and horizontal
// overflow, all driven from the inspector socket so the whole thing runs headless.
//
// The commands are the assertion surface: `state` reports everything a check needs (text, cursor,
// marked range, caret geometry, label offset, and the InteractiveComponent flags that prove CSS
// `:focus` finally has a producer), so a run does not have to diff screenshots to know whether the
// widget behaved.
class TextInputLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	// name -> widget, for the "widget" argument every command takes
	ui::TextInput *getWidget(const Value &args) const;

	Value encodeState(ui::TextInput *) const;

	ui::TextInput *_plain = nullptr;
	ui::TextInput *_password = nullptr;
	ui::TextInput *_readOnly = nullptr;
	ui::TextInput *_long = nullptr;

	uint32_t _changeCallbacks = 0;
	uint32_t _enterCallbacks = 0;
	String _lastChange;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_TEXTINPUTLAYOUT_H_
