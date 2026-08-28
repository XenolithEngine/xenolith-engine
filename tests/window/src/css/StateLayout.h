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

#ifndef TESTS_WINDOW_SRC_CSS_STATELAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_STATELAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiPanel.h"
#include "XLUiTextInput.h"
#include "XLUiCheckbox.h"
#include "XLUiProgressBar.h"
#include "XLUiFormSystem.h"
#include "XLUiFormAdapters.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* Control states as CSS: `:invalid`/`:valid`, `:read-only`/`:read-write`, `:indeterminate`,
`:required`/`:optional`, `:default`, and the two focus states `:focus-visible`/`:focus-within`.
Driven headless by style-check.py - the first headless check of the CSS engine in this app; the
other eleven css stands are read by looking at them.

EVERY STATE HERE IS PUT THERE BY ITS REAL PRODUCER. The form rejects an empty required field, an
edit lock takes a control away, a ui::TextInput is switched to read-only, a progress bar is given no
total, a submit button becomes the form's default, the tab ring is walked. Nothing assigns a bit
directly - which is what css/hover does, and rightly, because it checks the PARSER. Checking the
parser twice would leave the interesting half unchecked: that a widget's own state reaches a
selector at all.

WHAT THE SCRIPT READS is the RESOLVED style (StyleResolver::resolveStyleForNode), not the colour on
screen: the claim is that a rule matched, and a widget that happens not to paint its background
would otherwise fail a check about the cascade.

THE CLASS AND THE PSEUDO-CLASS ARE BOTH WATCHED, on purpose and on DIFFERENT properties: the old
`invalid` style class paints `color`, `:invalid` paints `background-color`. A form that publishes
one and not the other is exactly the regression this stand exists to catch, in either direction. */
class StateLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	// Everything the script addresses by name. `panel` and `nested` are the focus-within pair
	struct Sample {
		String name;
		Node *node = nullptr;
	};

	Value encodeSample(const Sample &) const;

	Node *getTarget(const Value &args) const;

	void addSample(StringView name, Node *);

	Vector<Sample> _samples;

	ui::FormSystem *_form = nullptr;

	ui::TextInput *_req = nullptr; // required, and empty until the script types into it
	ui::TextInput *_opt = nullptr; // the same widget without the flag: `:optional`
	ui::TextInput *_ro = nullptr; // read-only by its own mode
	ui::TextInput *_rw = nullptr; // the control case for the pair
	ui::TextInput *_locked = nullptr; // read-only because a lock owns it
	ui::Checkbox *_check = nullptr; // a NON-text field: the only one whose focus can be invisible
	ui::Button *_submit = nullptr; // the form's default button
	ui::ProgressBar *_bar = nullptr;

	ui::Panel *_outer = nullptr; // both must take `:focus-within` when the field below has focus
	ui::Panel *_inner = nullptr;
	ui::TextInput *_nested = nullptr;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_STATELAYOUT_H_
