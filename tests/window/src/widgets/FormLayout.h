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

#ifndef TESTS_WINDOW_SRC_WIDGETS_FORMLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_FORMLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::FormSystem: collection, Tab traversal, submit/reset and validation, all driven from the
// inspector socket so the whole thing runs headless.
//
// The fields are laid out in a fixed order, and `form.state` reports the tab ring by name - which
// is the assertion surface for everything about traversal: that the ring is in document order,
// that a display:none field is not in it, that a disabled one is not either, and that Tab wraps.
//
// The `hidden` field lives inside a container the test can collapse with display:none, because
// that is the case the ring gets right for free (a collapsed subtree is never visited, so its
// listeners never register) and would get wrong under a hand-rolled tree walk.
class FormLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	ui::FormInputListener *getField(const Value &args) const;
	ui::TextInput *getTextInput(StringView name) const;
	Value encodeState() const;

	// The text-input details a form check needs: what the widget holds, where its caret is and
	// what CSS painted it. FormLayout has to report these itself - `text-input.*` belongs to the
	// TextInput stand and is not registered here
	Value encodeTextInput(ui::TextInput *) const;

	ui::FormSystem *_form = nullptr;

	ui::TextInput *_name = nullptr;
	ui::TextInput *_email = nullptr;
	ui::Checkbox *_subscribe = nullptr;
	ui::TextInput *_notes = nullptr;
	Node *_hiddenBox = nullptr;
	ui::TextInput *_hidden = nullptr;
	ui::Button *_submit = nullptr;
	ui::Button *_reset = nullptr;

	uint32_t _submitCallbacks = 0;
	uint32_t _resetCallbacks = 0;
	uint32_t _invalidCallbacks = 0;
	Value _lastSubmit;
	Vector<String> _lastInvalid;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_FORMLAYOUT_H_
