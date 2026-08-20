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

#ifndef TESTS_WINDOW_SRC_WIDGETS_TEXTVIEWLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_TEXTVIEWLAYOUT_H_

#include "app/TestLayout.h"
#include "XLClipboard.h"
#include "XLUiTextInput.h"
#include "XLUiTextView.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::TextHistory: undo over the one point where text actually changes.
//
// TWO WIDGETS, because the interesting half of the decision is the one that says NO. The view has
// a history and answers Ctrl+Z; the field beside it has the same machinery and it is turned off,
// so the chord goes straight past it to whoever is below. A field in a property panel commits its
// value into somebody's document, and a field that swallowed Ctrl+Z would undo the typing instead
// of the document edit - which is the arbitration question this stand exists to pin down.
//
// TYPE THROUGH THE PLATFORM, not through a command. The runtime's text-input processor owns
// printable keys, so a typed character never reaches insertText at all: it arrives as an ECHO and
// is recovered by diffing. Driving the stand with `input ... native=true` is what exercises that
// path; an `insert` command would prove only that the command works.
//
// COALESCING IS ASSERTED WITHOUT SLEEPING. A run of keystrokes is one undo entry until its idle
// window passes, and `history-break` ends a run on demand - which is what an owner does when it
// knows the thought is over, and what keeps the check deterministic. The window itself is checked
// once, with a short window and a real pause, because that is the one claim a counter cannot make.
class TextViewLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	// The widget an argument names: "view" (default) or "field".
	ui::TextInput *widget(const Value &args) const;

	Value encodeState() const;
	Value encodeField() const;

	ui::TextView *_view = nullptr;
	ui::TextInput *_field = nullptr;
	basic2d::Label *_caption = nullptr;

	// So a paste has something to paste. The session is the E7 seam; what matters here is only
	// that the text arrives through the real asynchronous path, because that is what decides
	// whether a paste is one undo entry or one per character.
	Rc<ClipboardSession> _session;

	/* Counts Ctrl+Z chords that got PAST the focused widget. The layout's own listener sits above
	the widgets in the scene graph and is therefore visited after them, so this only moves when the
	focused widget declined - which is the whole claim behind "a field with no history must not
	swallow the chord". Without something below to catch it, "not swallowed" is unfalsifiable. */
	uint32_t _undoFellThrough = 0;
	uint32_t _redoFellThrough = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_TEXTVIEWLAYOUT_H_
