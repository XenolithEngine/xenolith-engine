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

#ifndef XENOLITH_RENDERER_UI_XLUIAUXSESSION_H_
#define XENOLITH_RENDERER_UI_XLUIAUXSESSION_H_

#include "XLUiAuxWindow.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Single tip slot for the whole process: showing a tip replaces the previous one, and opening a
// popup drops it. Tips are in-scene overlays on the parent; popups stay native subwindows.
// App-thread only.
class SP_PUBLIC AuxSession {
public:
	static constexpr TimeInterval DefaultHideDelay = TimeInterval::milliseconds(800);

	enum class TipState {
		Idle,
		Ready
	};

	static AuxSession &instance();

	void showTip(NotNull<AppWindow> parent, StringView text, Vec2 anchorSceneYUp, float sceneHeight,
			TimeInterval hideDelay = DefaultHideDelay);

	void dismissTip();

	TipState getTipState() const { return _tipState; }
	StringView getTipId() const { return _tipId; }

	String openPopup(NotNull<AppWindow> parent, const sprt::window::WindowPlacement &, Extent2 size,
			AuxWindow::ContentBuilder &&, StringView title = StringView());

protected:
	// Weak back-link for the hide timer: the timer outlives nothing, but its completion may fire
	// after the session is gone (static teardown).
	struct Lifetime : public Ref {
		AuxSession *session = nullptr;
	};

	AuxSession();
	~AuxSession();

	AuxSession(const AuxSession &) = delete;
	AuxSession &operator=(const AuxSession &) = delete;

	void replaceOverlayTip(NotNull<AppWindow> parent, StringView text, Vec2 anchorSceneYUp,
			float sceneHeight, TimeInterval hideDelay);
	void clearTip();
	void armHideTimer(TimeInterval);
	void cancelHideTimer();
	bool isParentUsable(AppWindow *) const;
	sprt::window::WindowPlacement makePlacement(Vec2 anchorSceneYUp, float sceneHeight) const;

	TipState _tipState = TipState::Idle;
	String _tipId;
	String _tipText;
	AppWindow *_parent = nullptr;
	TimeInterval _hideDelay = DefaultHideDelay;

	Rc<Lifetime> _life;
	Rc<sprt::dispatch::TimerHandle> _hideTimer;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUIAUXSESSION_H_
