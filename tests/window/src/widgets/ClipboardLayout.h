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

#ifndef TESTS_WINDOW_SRC_WIDGETS_CLIPBOARDLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_CLIPBOARDLAYOUT_H_

#include "app/TestLayout.h"
#include "XLClipboard.h"
#include "XLUiTextInput.h"
#include "XLUiTextView.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// xenolith::ClipboardSession: one typed exchange with the system clipboard.
//
// The stand carries a session of its own AND two widgets, because the seam has two kinds of claim
// and they are checked differently.
//
// The SESSION is the typed half, and it is what no widget exercises: a payload with two
// representations, a preference list that decides which one comes back, and a preference list that
// matches nothing. That last one is the whole reason the seam exists - wayland answers a type it
// did not offer with SILENCE, and the base controller answers twice - so "exactly one answer"
// is counted here rather than asserted in prose.
//
// The WIDGETS are the parity half. copy/cut/paste live in ui::TextInput now and ui::TextView only
// overrides where its cursor comes from, so what one copies the other must paste. The password
// field is here for the asymmetry that survived the merge: TextInput refuses to copy a masked
// selection, TextView never masks and always copies, and both halves of that are asserted so a
// later tidy-up cannot quietly unify them.
//
// RUN THIS ONE HEADLESS, like the drag payload stand: the headless controller keeps the clipboard
// in process, so a round trip is deterministic. On a real window system taking the selection needs
// an input serial from a focused window, and a test window in the background silently fails to
// become the owner - after which the read returns whatever another application put there.
class ClipboardLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;
	Value encodeLastRead() const;

	ui::TextInput *widget(StringView name) const;

	Rc<ClipboardSession> _session;

	ui::TextInput *_field = nullptr;
	ui::TextInput *_secret = nullptr;
	ui::TextView *_view = nullptr;

	basic2d::Label *_caption = nullptr;

	// Every answer bumps this, and that is the point: "exactly once" is a number, not a feeling.
	// A read that matches nothing must still land here exactly once, and a cancelled one must not
	// land at all.
	uint32_t _deliveries = 0;

	// The last answer, kept whole so the script can read the type that ARRIVED and the types that
	// WERE there - the second is what lets a consumer say why a paste was refused.
	uint64_t _lastSerial = 0;
	Status _lastStatus = Status::Declined;
	String _lastType;
	String _lastText;
	Vector<String> _lastAvailable;
	bool _everAnswered = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_CLIPBOARDLAYOUT_H_
