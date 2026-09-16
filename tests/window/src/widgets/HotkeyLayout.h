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

#ifndef TESTS_WINDOW_SRC_WIDGETS_HOTKEYLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_HOTKEYLAYOUT_H_

#include "app/TestLayout.h"
#include "XLHotkey.h"
#include "XLFocusGroup.h"
#include "XLInputListener.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* Verification layout for the global hotkey controller (XLHotkey.h + InputDispatcher::handleHotkey).
 *
 * The scene carries four subscribers on purpose, so the delivery order is observable:
 *
 *   `global`    — on this layout, no focus group, plain subscription
 *   `focused`   — inside a SingleFocus group, FocusedOnly
 *   `declining` — sits ahead of `global` in the walk and always returns false
 *   `exclusive` — inside an Exclusive group that can be switched on at runtime
 *
 * Every delivery appends to a log, which is what the checks read: who was called, in what order,
 * and who was never called at all because someone earlier consumed the key.
 */
class HotkeyLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	struct Subscriber {
		StringView name;
		InputListener *listener = nullptr;
		bool consume = true;
		bool enabled = true;
	};

	virtual void registerCommands() override;

	// Registers the stand's own hotkeys, separate from the engine's so a test can rebind them
	void setupHotkeys();

	Subscriber *addSubscriber(StringView name, Node *owner, HotkeyFlags, bool consume);

	Value encodeLog() const;
	Value encodeRegistry() const;

	void note(StringView subscriber, HotkeyId);

	// Reserved in init(): the delivery callbacks index into it, so it must never reallocate
	Vector<Subscriber> _subscribers;
	Vector<Value> _log;

	HotkeyId _action; // Ctrl+K — the ordinary case
	HotkeyId _shared; // F7 — bound by several subscribers at once
	HotkeyId _bypass; // Ctrl+Q — carries BypassExclusive
	HotkeyId _sided; // CtrlL+K — the same key as _action, but only from the left Ctrl
	HotkeyId _reserved; // Alt+F — carries a keychar, so only the reserved-key filter saves it

	// A focused text field, so that the reserved-key filter can be tested where it matters: with
	// the IME holding the keyboard
	ui::TextInput *_field = nullptr;

	FocusGroup *_focusGroup = nullptr;
	FocusGroup *_exclusiveGroup = nullptr;
	Node *_exclusiveNode = nullptr;

	// Counts plain key events that reached an ordinary recognizer, so a test can tell
	// "no hotkey matched" from "a hotkey matched and was swallowed"
	uint32_t _fallthroughCount = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_HOTKEYLAYOUT_H_
