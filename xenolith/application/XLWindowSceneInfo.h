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

#ifndef XENOLITH_APPLICATION_XLWINDOWSCENEINFO_H_
#define XENOLITH_APPLICATION_XLWINDOWSCENEINFO_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLCoreQueue.h"
#include "XLCoreInfo.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;
class AppWindow;
class Scene;

namespace core {
class RenderServerChannel;
}

// Everything the application wants to say about a window that the runtime must not know: which
// scene it runs, which already-compiled render queue that scene adopts, and what happens when the
// window goes away.
//
// It rides to the window backend as WindowInfo::appData and comes back off on the thread that
// owns the window's content, so a scene is named together with its window instead of being looked
// up afterwards in a table keyed by WindowInfo::id. That matters beyond tidiness: the runtime
// re-uniques a colliding id (ContextController::configureWindow), so the id the caller chose is
// not necessarily the id the factory later sees.
//
// This object IS the window handle — keep the Rc the opener returned. getWindow() is non-null
// from the moment the AppWindow reaches the app thread until the window is torn down.
//
// Thread contract: constructed, used and destroyed on the app thread. In a DEBUG build the
// destructor asserts that, because the one thing that would silently break it — leaving the
// payload on a WindowInfo that dies on the context thread — has no other symptom.
class SP_PUBLIC WindowSceneInfo : public Ref {
public:
	// Runs on the app thread when the window needs its scene. Returning null falls through to the
	// process-wide `makeScene` symbol, which is what unattached windows (above all the root one)
	// keep using.
	using SceneBuilder = Function<Rc<Scene>(NotNull<AppThread>,
			NotNull<core::RenderServerChannel>, const core::FrameConstraints &)>;

	// Fired exactly once, on the app thread, however the window went away — its own close, the
	// parent's teardown, a WM-side dismiss, or a creation that never succeeded.
	using CloseCallback = Function<void(NotNull<WindowSceneInfo>)>;

	virtual ~WindowSceneInfo();

	virtual bool init(SceneBuilder &&, CloseCallback && = nullptr);

	// Adopt this queue instead of having the scene build one. Null means "build your own".
	// The queue is expected to be compiled (see QueueCache); a scene adopting one does not own it.
	const Rc<core::Queue> &getQueue() const { return _queue; }
	void setQueue(Rc<core::Queue> &&q) { _queue = sp::move(q); }

	// The live window, or null before creation and after teardown.
	AppWindow *getWindow() const { return _window; }

	// The final, uniqued WindowInfo::id. Empty until the window exists.
	StringView getId() const;

	Rc<Scene> makeScene(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &);

	// Idempotent: the second and later calls do nothing.
	void fireClose();
	bool isCloseFired() const { return _closeFired; }

	Ref *getUserData() const { return _userData; }
	void setUserData(Rc<Ref> &&u) { _userData = sp::move(u); }

protected:
	friend class AppWindow;

	void setWindow(AppWindow *w) { _window = w; }

	SceneBuilder _builder;
	CloseCallback _onClose;
	Rc<core::Queue> _queue;
	Rc<Ref> _userData;

	// Non-owning back-reference: the window owns this object, not the other way round.
	AppWindow *_window = nullptr;

	bool _closeFired = false;

#if DEBUG
	// The looper this was constructed on, checked in the destructor. See the thread contract above.
	sprt::dispatch::Looper *_owner = nullptr;
#endif
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLWINDOWSCENEINFO_H_
