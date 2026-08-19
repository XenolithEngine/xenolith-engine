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

	// core::RenderClientChannel (server -> client). The server's PresentationEngine pulls a
	// command batch via acquireFrame(); other entries deliver platform events / contract changes.
	virtual void acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy>,
			Function<void(bool)> &&) override;
	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override;
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override;
	virtual void handleWindowGeometryChanged(const sprt::window::WindowGeometry &) override;
	virtual void handleInputEvents(Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(const core::TextInputState &) override;
	virtual void handleFramePresented(uint64_t frameOrder) override;

	void update(uint64_t t);

	// Can be nullptr to disconnect director from window
	void setServer(core::RenderServerChannel *);

	void end();

	AppThread *getApplication() const { return _application; }

	// Server-side endpoint of the render-session boundary; client-side code (e.g. FrameContext)
	// issues render-graph / resource / material compilation through it.
	core::RenderServerChannel *getRenderServer() const { return _server; }

	// REMAINING client->server coupling: the 2D renderer still reaches the gapi loop directly to
	// schedule frame-input attachment on the render-loop thread. To be folded into the render
	// session (as part of command-batch submission) in a later stage.
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

	virtual void pushDrawStat(const DrawStat &) override;

	const UpdateTime &getUpdateTime() const { return _time; }
	const DrawStat &getDrawStat() const { return _drawStat; }

	float getFps() const;
	float getAvgFps() const;
	float getSpf() const; // in milliseconds
	float getFenceFrameTime() const;
	float getTimestampFrameTime() const;

	float getDirectorFrameTime() const { return _avgFrameTimeValue / 1000.0f; }

	void autorelease(Ref *);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	bool hasActiveInteractions();

	Rc<AppThread> _application;

	Rc<Ref> _window;

	// Server-side endpoint of the render-session boundary (client -> server calls).
	// In local mode this points at the AppWindow; later it may be a network proxy.
	core::RenderServerChannel *_server = nullptr;

	// Render queues the server has announced as available (via handleRenderQueueAttached), keyed
	// by name. The client selects one per frame through the FrameRequestProxy.
	Map<StringView, Rc<core::Queue>> _availableQueues;

	core::FrameConstraints _constraints;

	uint64_t _startTime = 0;
	UpdateTime _time;
	DrawStat _drawStat;

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
