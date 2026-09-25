/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_
#define XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_

#include "XLRemoteAddress.h"
#include "XLResourceCache.h"
#include "XLAppThread.h"
#include "XLInput.h" // IWYU pragma: keep
#include "XLCoreRenderSession.h"
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Scene;
class Scheduler;
class InputDispatcher;
class TextInputManager;
class ActionManager;
class DirectorWindow;

class SP_PUBLIC Director : public core::RenderClientChannel {
public:
	using FrameRequest = core::FrameRequest;

	virtual ~Director();

	Director();

	bool init(NotNull<AppThread>, const core::FrameConstraints &,
			NotNull<core::RenderServerChannel>);

	void setFrameConstraints(const core::FrameConstraints &);

	void runScene(Rc<Scene> &&);

	// For server, compile and share Director's windo with a new queue;
	// For client - returns null;
	Rc<core::Queue> shareQueue(core::Queue::Builder &&, StringView addr, BytesView key,
			BytesView dict = BytesView());

	/* The same, for a window joining a session that is already running. Credentials and the listen
	address are set once by the session opener (setBearerKey fails while the listener is up). A
	connected client is re-announced, so the window appears without reconnecting. */
	Rc<core::Queue> shareQueue(core::Queue::Builder &&);

	// core::RenderClientChannel (server -> client). The server's PresentationEngine pulls a
	// command batch via acquireFrame(); other entries deliver platform events / contract changes.
	virtual void acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy>,
			Function<void(bool)> &&) override;
	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override;
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override;
	// windowId is ignored: a Director drives exactly one window. The id exists for the remote
	// channel, which drives many.
	virtual void handleWindowGeometryChanged(uint64_t windowId,
			const sprt::window::WindowGeometry &) override;
	virtual void handleInputEvents(uint64_t windowId, Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(uint64_t windowId, const core::TextInputState &) override;
	// Routed to the DragSystem of the scene content; refused without a scene
	virtual void handleDropEvent(uint64_t windowId, core::DropEvent &&) override;
	virtual void handleFramePresented(uint64_t frameOrder) override;
	virtual void handleFrameDeclined(uint64_t windowId) override;

	/* The scene changed (Scene::markChanged). The first change after a frame schedules one check at
	the end of the current app-thread task, so a change from any code path gets its frame; changes
	while a frame is being built are what that frame shows. */
	void handleSceneChanged();

	/* The app thread's tick for the window. A wakeup always asks for a frame: whatever woke the
	thread may change what is drawn. The heartbeat asks only for a scene that changed or moves. */
	void handleAppUpdate(bool wakeup);

	void update(uint64_t t);

	// Can be nullptr to disconnect director from window
	void setServer(core::RenderServerChannel *);

	void end();

	AppThread *getApplication() const { return _application; }

	// Server-side endpoint of the render-session boundary; client-side code (e.g. FrameContext)
	// issues render-graph / resource / material compilation through it.
	core::RenderServerChannel *getRenderServer() const { return _server; }

	// The 2D renderer still reaches the gapi loop directly to schedule frame-input attachment on
	// the render-loop thread.
	core::Loop *getGlLoop() const;

	// Run `cb` on the thread that consumes per-frame input: the gapi loop thread on a server/local
	// director, or the (GL-loop-less) client app thread on a remote client. Used by frame-input
	// submission so it resolves correctly on both sides.
	void performOnRenderThread(Function<void()> &&cb, Ref *ref = nullptr);

	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	InputDispatcher *getInputDispatcher() const { return _inputDispatcher; }
	TextInputManager *getTextInputManager() const;

	Scene *getScene() const { return _scene; }
	ResourceCache *getResourceCache() const;

	const Mat4 &getGeneralProjection() const { return _generalProjection; }

	const core::FrameConstraints &getFrameConstraints() const { return _constraints; }

	virtual void pushDrawStat(uint64_t windowId, const DrawStat &) override;

	const UpdateTime &getUpdateTime() const { return _time; }
	const DrawStat &getDrawStat() const { return _drawStat; }

	float getFps() const;
	float getAvgFps() const;
	float getSpf() const; // in milliseconds
	float getFenceFrameTime() const;
	float getTimestampFrameTime() const;

	float getDirectorFrameTime() const { return _avgFrameTimeValue / 1000.0f; }

#if XL_FRAME_ACCOUNT
	/* The app half of the last frame, exact, in nanoseconds (not an average): `acquireFrame` whole,
	including the update and the scene visit, and handing tesselation to a worker but not waiting
	for it. */
	uint64_t getLastAppFrameTime() const { return _lastAppFrameTime; }

	// The render half of the last completed frame, exact, with its frame id: vertex plan, wait on
	// deferred work, buffer writes and device submission. Does not include the visit.
	core::FrameTimingInfo getFrameTiming() const;

	/* Deferred tasks started during that frame's visit (the producing side). VertexPlan reports
	what a frame waited for, which differs; a steady frame reports zero on both. */
	uint32_t getLastDeferredSpawned() const { return _lastDeferredSpawned; }
	void countDeferredSpawned() { ++_deferredSpawned; }
#endif

	void autorelease(Ref *);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	bool hasActiveInteractions();

	void updateFrameRequest(bool force);
	void requestFrame();

	Rc<AppThread> _application;

	Rc<Ref> _window;

	// Server-side endpoint of the render-session boundary (client -> server calls): the AppWindow
	// locally, a RemoteWindow on a remote client.
	core::RenderServerChannel *_server = nullptr;

	// Render queues the server has announced as available (via handleRenderQueueAttached), keyed
	// by name. The client selects one per frame through the FrameRequestProxy.
	Map<StringView, Rc<core::Queue>> _availableQueues;

	core::FrameConstraints _constraints;

	/* Asking for frames. A frame is in flight from the request to acquireFrame, and is being built
	from acquireFrame to the end of its visit. `_drawnChanges` is the scene's change count at the end
	of the last visit, `_declinedChanges` the count the last declined request was made at. */
	uint64_t _drawnChanges = 0;
	uint64_t _declinedChanges = maxOf<uint64_t>();
	uint32_t _frameBuildSerial = 0;
	bool _frameRequested = false;
	bool _frameBuilding = false;
	bool _checkScheduled = false;

	uint64_t _startTime = 0;
	UpdateTime _time;
	DrawStat _drawStat;

#if XL_FRAME_ACCOUNT
	uint64_t _pendingAppTime = 0; // the update half, until the visit closes the account
	uint64_t _lastAppFrameTime = 0;
	uint32_t _deferredSpawned = 0; // counted during the visit
	uint32_t _lastDeferredSpawned = 0; // what the visit that just ended counted
#endif

	Rc<Scene> _scene;
	Rc<Scene> _nextScene;

	Mat4 _generalProjection;

	Rc<sprt::AllocRef> _allocator;
	Rc<sprt::PoolRef> _pool;
	Rc<Scheduler> _scheduler;
	Rc<ActionManager> _actionManager;
	Rc<InputDispatcher> _inputDispatcher;
	Rc<TextInputManager> _textInput;

	Vector<Rc<Ref>> _autorelease;

	MovingAverage<20, uint64_t> _avgFrameTime;
	uint64_t _avgFrameTimeValue = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_DIRECTOR_XLDIRECTOR_H_ */
