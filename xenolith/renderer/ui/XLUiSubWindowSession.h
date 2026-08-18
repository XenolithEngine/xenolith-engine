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

#ifndef XENOLITH_RENDERER_UI_XLUISUBWINDOWSESSION_H_
#define XENOLITH_RENDERER_UI_XLUISUBWINDOWSESSION_H_

#include "XLUiSubWindow.h"
#include "XLSystem.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One tip slot and one popup chain PER WINDOW: showing a tip replaces that window's previous one,
// and opening a popup drops it.
//
// It is a System on the window's own SceneContent, not a process singleton — two windows must be
// able to show a hint each, and a session that outlives its window is a bug waiting to happen.
// get() attaches one lazily, so nothing has to be registered up front.
//
// App-thread only.
class SP_PUBLIC SubWindowSession : public System {
public:
	static constexpr TimeInterval DefaultHideDelay = TimeInterval::milliseconds(800);

	// The session of `window`, creating it on first use. Null only when the window has no
	// SceneContent yet (before its scene is presented).
	static SubWindowSession *get(NotNull<AppWindow> window);

	virtual ~SubWindowSession();

	virtual bool init() override;

	virtual void handleExit() override;

	// Replace this window's hint with `text`, anchored at a scene-space (Y-up) point. Re-showing
	// the same text just refreshes the hide timer instead of flapping the tip.
	void showTip(StringView text, Vec2 anchorSceneYUp, float sceneHeight,
			TimeInterval hideDelay = DefaultHideDelay);

	// Replace this window's hint with a surface of the caller's own making — this is what a hint
	// richer than a line of text goes through, and what ui::TooltipSystem uses.
	//
	// `key` identifies what is being shown, so that re-showing the same thing refreshes the hide
	// timer instead of a dismiss/recreate flap. It is compared, never parsed: the text overload
	// passes the text, TooltipSystem passes its target's identity. An empty key never matches, so
	// an unkeyed tip is always rebuilt.
	//
	// `config.type` is forced to Tooltip and `config.onClose` is chained, so the session's own slot
	// bookkeeping cannot be lost by a caller that wants a close callback of its own.
	//
	// A zero `hideDelay` means "no hide timer": the tip stays until something takes it down.
	Rc<SubWindow> showTip(SubWindow::Config &&, StringView key,
			TimeInterval hideDelay = DefaultHideDelay);

	// Restart the live tip's hide timer. For an owner that knows the user is still engaged with a
	// hint it did not just rebuild. A no-op when nothing is up.
	void refreshTip(TimeInterval hideDelay = DefaultHideDelay);

	void dismissTip();

	bool hasTip() const { return _tip && _tip->isOpen(); }

	// What the live tip is keyed on. For a text tip that IS the text.
	StringView getTipKey() const { return _tipKey; }
	StringView getTipText() const { return _tipKey; }

	// Open a popup for this window, dropping any live tip first (the tip is an overlay on the same
	// scene, and a menu taking over from a hint is what a user expects).
	Rc<SubWindow> openPopup(SubWindow::Config &&);

protected:
	// Weak back-link for the hide timer: the timer's completion may fire after the session is
	// gone (its window closed while a tip was up).
	struct Lifetime : public Ref {
		SubWindowSession *session = nullptr;
	};

	AppWindow *getWindow() const;

	void clearTip();
	void armHideTimer(TimeInterval);
	void cancelHideTimer();

	Rc<SubWindow> _tip;
	String _tipKey;
	TimeInterval _hideDelay = DefaultHideDelay;

	Rc<Lifetime> _life;
	Rc<sprt::dispatch::TimerHandle> _hideTimer;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUISUBWINDOWSESSION_H_
