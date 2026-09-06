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

#include "XLDirector.h"

#include "XLResourceCache.h"
#include "XLScheduler.h"
#include "XLScene.h"
#include "XLInputDispatcher.h"
#include "XLTextInputManager.h"
#include "XLActionManager.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLContext.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Director::Director() { sprt::memset(&_drawStat, 0, sizeof(DrawStat)); }

#if XL_FRAME_ACCOUNT
/* ---- the app account (XL_APP_ACCOUNT=N) ---------------------------------------------------------

The counterpart of the software backend's frame budget (XL_SOFT_BUDGET, see
docs/agents/measuring-frames.md). That instrument accounts for the render half and lands everything
it cannot see in one bucket called `wait`; this one says what is inside it.

Reported the same way - running averages over the whole run, every N frames - so the two logs can
be read against each other directly, even though they are counted on different threads and neither
knows about the other. They must be read as averages of the same run, not of the same frame: the
app half of frame N and the render half of frame N are published at different moments, and pairing
individual lines would describe two different frames.

The split that matters:

	update  everything acquireFrame does before posting the visit - the scheduler, the action
	        manager, input dispatch, and whatever the application's own update() does.
	visit   the scene graph walk that builds the frame's command list.
	spawned deferred tesselation tasks STARTED by that visit. A steady frame must report zero:
	        anything else means the scene is re-tesselating something every frame, and that is a
	        bug in the scene, not a cost of the renderer.

`work` and `wait` come back on DrawStat from the consuming side and ARE NOT PARTS OF ONE WHOLE -
work is summed across worker threads and may exceed the frame, wait is one thread standing still.
Never add them. */
static uint64_t Director_accountInterval() {
	static const uint64_t value = [] () -> uint64_t {
		auto env = ::getenv("XL_APP_ACCOUNT");
		if (!env) {
			return 0;
		}
		auto str = StringView(env);
		if (str == "0") {
			return 0;
		}
		auto n = str.readInteger(10).get(0);
		return n > 0 ? uint64_t(n) : 60;
	}();
	return value;
}

// Runs on the app thread, at the point where the visit closes the account, so the counters are
// touched by one thread and need no synchronization. `stat` is the last DrawStat the render half
// sent back; it lags the visit by a frame or so, which does not matter to an average.
static void Director_reportAccount(uint64_t update, uint64_t visit, uint32_t spawned,
		const DrawStat &stat) {
	auto interval = Director_accountInterval();
	if (interval == 0) {
		return;
	}

	static uint64_t frames = 0;
	static uint64_t updateSum = 0;
	static uint64_t visitSum = 0;
	static uint64_t spawnedSum = 0;
	static uint64_t deferWork = 0;
	static uint64_t deferWait = 0;
	static uint64_t deferCount = 0;
	static uint64_t deferWaited = 0;
	static uint64_t writeTime = 0;
	static uint64_t spanTime = 0;
	static uint64_t damageTime = 0;
	static uint64_t planTime = 0;

	++frames;
	updateSum += update;
	visitSum += visit;
	spawnedSum += spawned;
	deferWork += stat.deferredWorkTime;
	deferWait += stat.deferredWaitTime;
	deferCount += stat.deferredCount;
	deferWaited += stat.deferredWaited;
	writeTime += stat.writeTime;
	spanTime += stat.spanTime;
	damageTime += stat.damageTime;
	planTime += stat.planTime;

	if (frames % interval != 0) {
		return;
	}

	// Microseconds, because that is what the frame budget prints and the whole point is to put the
	// two side by side. The clocks below are nanosecond ones.
	auto per = [&] (uint64_t v) { return double(v) / double(frames) / 1'000.0; };

	log::source().debug("app::account", "frames=", frames,
			" appHalf=", per(updateSum + visitSum), "us",
			" (update=", per(updateSum), "us visit=", per(visitSum), "us)",
			" spawned/frame=", double(spawnedSum) / double(frames),
			" clock=", core::getAccountClockName(),
			" res=", double(core::getAccountClockResolution()) / 1'000.0, "us");
	log::source().debug("app::account", "  defer: work=", per(deferWork), "us",
			" wait=", per(deferWait), "us",
			" count/frame=", double(deferCount) / double(frames),
			" waited/frame=", double(deferWaited) / double(frames));

	// The vertex stage's own phases, not the app thread's - they belong to whichever budget stage
	// runs VertexPlan (`vertex` on the software backend). Reported here because DrawStat is the
	// channel they arrive on, and because a `vertex` stage that is large is answered by exactly
	// these four numbers. `damage` and `plan` are the command walk split in two and are NESTED in
	// it: read as "of the walk, this much is that", never added to write and span.
	log::source().debug("app::account", "  vertexPlan: write=", per(writeTime), "us",
			" span=", per(spanTime), "us",
			" (walk: damage=", per(damageTime), "us plan=", per(planTime), "us)");
}
#endif


Director::~Director() { log::source().info("Director", "~Director"); }

bool Director::init(NotNull<AppThread> app, const core::FrameConstraints &constraints,
		NotNull<core::RenderServerChannel> window) {
	_application = app;
	if (auto winref = dynamic_cast<Ref *>(window.get())) {
		_window = winref;
	}
	// Wire both ends of the render-session boundary at director-creation time (local mode: the
	// AppWindow is both). Registering the client here ensures the server can announce queues before
	// the initial scene runs.
	_server = window.get();
	window->setRenderClient(this);
	_allocator = Rc<sprt::AllocRef>::alloc();
	_pool = Rc<sprt::PoolRef>::alloc(_allocator);
	_pool->perform([&, this] {
		_scheduler = Rc<Scheduler>::create();
		_actionManager = Rc<ActionManager>::create();
		_inputDispatcher = Rc<InputDispatcher>::create(_pool, _server->getWindowState());
		_textInput = Rc<TextInputManager>::create(this);
	});
	_startTime = sp::platform::clock(ClockType::Monotonic);
	_time.global = 0;
	_time.app = 0;
	_time.delta = 0;

	_constraints = constraints;

	updateGeneralTransform();

	return true;
}

TextInputManager *Director::getTextInputManager() const { return _textInput; }

ResourceCache *Director::getResourceCache() const {
	return _application->getExtension<ResourceCache>();
}

Rc<core::Queue> Director::shareQueue(core::Queue::Builder &&builder, StringView addr, BytesView key,
		BytesView dict) {
	if (!_application || !_application->isServerThread()) {
		return nullptr;
	}
	// Credentials first, on the app thread that owns them, and only then the window. Setting them
	// from inside the compile callback (as this used to) meant the FIRST window carried them, which
	// worked only because there was exactly one.
	auto a = addr.str<Interface>();
	auto k = key.bytes<Interface>();
	auto d = dict.bytes<Interface>();
	_application->performOnAppThread([app = Rc<AppThread>(_application), a, k, d] {
		app->setBearerKey(BytesView(k.data(), k.size()));
		app->setListenAddress(a);
	}, this);

	return shareQueue(sp::move(builder));
}

Rc<core::Queue> Director::shareQueue(core::Queue::Builder &&builder) {
	struct ShareInfo : public Ref {
		Rc<Director> director;
		Rc<AppThread> application;
		Rc<AppWindow> window;
		Rc<core::Queue> queue;
		HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> materials;
	};

	if (!_application || !_application->isServerThread()) {
		return nullptr;
	}

	auto info = Rc<ShareInfo>::alloc();

	info->director = this;
	info->application = _application;

	// We are on server, so, it's safe to cast _server to actual window
	info->window = static_cast<AppWindow *>(info->director->getRenderServer());
	info->queue = Rc<core::Queue>::create(sp::move(builder));

	if (info->queue) {
		_server->compileRenderQueue(info->queue, [info](bool success) {
			// Note: we on main thread here

			if (success) {
				// build a list of initial materials
				for (auto &it : info->queue->getAttachments()) {
					if (it->type == core::AttachmentType::Material) {
						auto mAttachemnt =
								static_cast<core::MaterialAttachment *>(it->attachment.get());
						info->materials.emplace(mAttachemnt, mAttachemnt->getMaterials());
					}
				}

				info->application->performOnAppThread([info] {
					// Allow this window to be taken over by a connecting client (X11-style split).
					// The listener's address and bearer key were set by whoever opened the session;
					// no server dictionary is set, so a client's suggested one will be used.
					Vector<core::Queue *> queues{info->queue.get()};

					info->application->shareWindow(info->window, queues, info->materials);
				});
			}
		});
	}

	return info->queue;
}

void Director::acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy> req,
		Function<void(bool)> &&cb) {
	if (_nextScene && !_scene) {
		// Handle scene transition. The request carries no queue yet (the client selects it below),
		// so the next scene can always be adopted here.
		_scene = _nextScene;
		_nextScene = nullptr;
		_scene->setFrameConstraints(_constraints);
		updateGeneralTransform();
		_scene->handlePresented(this);
	}

	if (!_scene) {
		log::source().error("xenolith::Director", "No scene defined for a FrameRequest");
		cb(false);
		return;
	}

	auto t = sp::platform::clock(ClockType::Monotonic);

#if XL_FRAME_ACCOUNT
	// Reset HERE and not in the visit: a task started by the update - before the visit - belongs to
	// this frame just as much as one started by a node.
	_deferredSpawned = 0;
	// A clock of its own, in NANOSECONDS. `t` above is `clock()`, which is microseconds - fine for
	// a frame-rate average and too coarse for a visit that can be a few tens of microseconds.
	const auto appStart = core::getAccountClock();
	core::markFrame(core::FrameMark::AcquireStart);
#endif

	setFrameConstraints(req->getFrameConstraints());

	update(t);

	// Pick this frame's render graph by name; the server resolves it against its registry of
	// compiled queues (selectQueue logs if the name is unknown).
	req->selectQueue(_scene->getQueue());

	// Keep the scene alive for as long as the frame it produced. The pin belongs to the request,
	// not to the queue, so the queue itself stays shareable between scenes.
	req->setSceneRef(Rc<Ref>(_scene.get()));

	// break current stack frame, perform on next one
	_application->performOnAppThread([this, req = Rc<core::FrameRequestProxy>(req.get())] {
		if (!_scene || !req) {
			return;
		}

#if XL_FRAME_ACCOUNT
		const auto visitStart = core::getAccountClock();
		core::markFrame(core::FrameMark::VisitStart);
#endif

		auto pool = Rc<sprt::PoolRef>::alloc(_allocator);

		pool->perform([&, this] {
			_scene->renderRequest(req, pool);

			// apply new frame
			req->commit();

			// if there is active interactions (user input or animations)
			// - inform the server, that we want next frame immediately
			if (hasActiveInteractions()) {
				if (_server) {
					_server->setReadyForNextFrame();
				}
			}
		});

#if XL_FRAME_ACCOUNT
		/* The app half is CLOSED here, not where acquireFrame returns.

		The post above is deliberate - "break current stack frame" - so the visit happens on a later
		turn of the loop and the clock taken at the bottom of acquireFrame covers the update and the
		posting and nothing else. Measured before this was noticed: 800ns for a frame that walked
		three hundred nodes, which is the shape of a measurement that ended too early.

		The two pieces are added rather than reported apart because they are one thing - everything
		this thread does for the frame - and because between them there is nothing but the hop. */
		core::markFrame(core::FrameMark::VisitEnd);
		const auto visitTime = core::getAccountClock() - visitStart;
		_lastAppFrameTime = _pendingAppTime + visitTime;
		_lastDeferredSpawned = _deferredSpawned;

		Director_reportAccount(_pendingAppTime, visitTime, _lastDeferredSpawned, _drawStat);
#endif
	}, this, true);

	auto appTime = sp::platform::clock(ClockType::Monotonic) - t;
	_avgFrameTime.addValue(appTime);
	_avgFrameTimeValue = _avgFrameTime.getAverage();

#if XL_FRAME_ACCOUNT
	// Half of the account; the lambda above adds the visit and publishes the total.
	_pendingAppTime = core::getAccountClock() - appStart;
#endif

	cb(true);
}

void Director::handleRenderQueueAttached(const Rc<core::Queue> &queue) {
	// The server announced an available render graph; record it so the client can select it by
	// name per frame (FrameRequestProxy::selectQueue).
	if (queue) {
		_availableQueues.insert_or_assign(queue->getName(), queue);
	}
}

void Director::handleConstraintsChanged(const core::FrameConstraints &c) { setFrameConstraints(c); }

void Director::handleWindowGeometryChanged(uint64_t, const sprt::window::WindowGeometry &g) {
	// Straight through: unlike constraints, geometry changes nothing the director owns - no scene
	// size, no transform - so there is nothing to recompute and nothing to guard against. The
	// scene is the only consumer.
	if (_scene) {
		_scene->handleWindowGeometryChanged(g);
	}
}

void Director::handleInputEvents(uint64_t, Vector<core::InputEventData> &&events) {
	for (auto &event : events) {
		if (event.isPointEvent()) {
			event.point.density = _constraints.density;
		}
		_inputDispatcher->handleInputEvent(event);
	}
}

void Director::handleTextInput(uint64_t, const core::TextInputState &state) {
	auto copy = state;
	_textInput->handleInputUpdate(copy);
}

void Director::handleFramePresented(uint64_t frameOrder) {
	// Reserved for client-side pacing/stats; no-op in stage 1.
}

void Director::update(uint64_t t) {
	if (_time.global) {
		_time.delta = t - _time.global;
	} else {
		_time.delta = 0;
	}

	_time.global = t;
	_time.app = t - _startTime;

	// If we are debugging our code, prevent big delta time
	if (_time.delta && _time.delta > config::MaxDirectorDeltaTime) {
		_time.delta = config::MaxDirectorDeltaTime;
	}

	_time.dt = float(_time.delta) / 1'000'000;

	if (_nextScene) {
		if (_scene) {
			_scene->handleFinished(this);
		}

		_scene = _nextScene;

		_scene->setFrameConstraints(_constraints);
		_scene->handlePresented(this);
		_nextScene = nullptr;
	}

	_inputDispatcher->update(_time);
	_scheduler->update(_time);
	_actionManager->update(_time);

	_autorelease.clear();
}

void Director::setServer(core::RenderServerChannel *s) {
	if (s != _server) {
		_textInput->cancel();
		if (s) {
			if (auto winref = dynamic_cast<Ref *>(s)) {
				_window = winref;
			}
			_server = s;
			_inputDispatcher->resetWindowState(_server->getWindowState(), true);

			if (_scene && _scene->getQueue()->isCompiled()) {
				_server->attachRenderQueue(_scene->getQueue());
			}
		} else {
			_window = nullptr;
			_server = nullptr;
			_inputDispatcher->resetWindowState(WindowState::None, false);
		}
	}
}

void Director::end() {
	if (_scene) {
		_scene->handleFinished(this);
		_scene->removeAllChildren(true);
		_scene->cleanup();
	}

#if SP_REF_DEBUG
	if (_scene) {
		_autorelease.clear();
		if (_scene->getReferenceCount() > 1) {
			auto scene = _scene.get();
			_scene = nullptr;

			scene->foreachBacktrace(
					[](uint64_t id, Time time, const sprt::vector<sprt::string> &vec) {
				StringStream stream;
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) { stream << "\t" << it << "\n"; }
				log::source().debug("Director", stream.str());
			});
		} else {
			_scene = nullptr;
		}
	}

	if (core::FrameHandle::GetActiveFramesCount()) {
		core::FrameHandle::DescribeActiveFrames();
	}
#else
	_scene = nullptr;
#endif

	if (!_scheduler->empty()) {
		_scheduler->unscheduleAll();
	}

	_nextScene = nullptr;

	setServer(nullptr);

	_autorelease.clear();
}

core::Loop *Director::getGlLoop() const { return _application->getGlLoop(); }

void Director::performOnRenderThread(Function<void()> &&cb, Ref *ref) {
	if (auto loop = _application->getGlLoop()) {
		loop->performOnThread(sp::move(cb), ref);
	} else {
		// No gapi loop (remote client): the app thread owns the connection that ships frame input.
		_application->performOnAppThread(sp::move(cb), ref);
	}
}

void Director::setFrameConstraints(const core::FrameConstraints &c) {
	if (_constraints != c) {
		_constraints = c;
		if (_scene) {
			_scene->setFrameConstraints(_constraints);
		}

		updateGeneralTransform();
	}
}

void Director::runScene(Rc<Scene> &&scene) {
	if (!scene || !_server) {
		return;
	}

	log::source().debug("Director", "runScene");

	auto linkId = sprt::retain(this);
	auto &queue = scene->getQueue();

	_nextScene = scene;

	// An already-compiled queue (one adopted from QueueCache) must not go through the compiler a
	// second time. That is not just wasted work: Queue::setCompiled OVERWRITES the queue's release
	// callback, so a second compile silently drops the first one and re-creates every render pass -
	// a GPU-object leak with no crash to point at it.
	if (queue->isCompiled()) {
		_server->attachRenderQueue(queue);
		sprt::release(this, linkId);
		return;
	}

	// Compile the render graph on the server, then make it the active graph (attachRenderQueue
	// performs the server-side context-thread hop + runWithQueue + setReadyForNextFrame).
	// `this` is kept alive across the async callback by linkId; the server endpoint stays valid
	// because the Director retains the AppWindow via _window.
	_server->compileRenderQueue(queue, [this, scene = move(scene), linkId](bool success) mutable {
		if (success && _server) {
			_server->attachRenderQueue(scene->getQueue());
		}
		sprt::release(this, linkId);
	});
}

void Director::pushDrawStat(uint64_t, const DrawStat &stat) {
	_application->performOnAppThread([this, stat] { _drawStat = stat; }, this, false);
}

#if XL_FRAME_ACCOUNT
core::FrameTimingInfo Director::getFrameTiming() const {
	return _server ? _server->getFrameTiming() : core::FrameTimingInfo();
}
#endif

float Director::getFps() const {
	auto t = _server ? _server->getFrameTiming() : core::FrameTimingInfo();
	return t.lastFrameInterval ? 1.0f / (t.lastFrameInterval / 1000000.0f) : 1.0f;
}

float Director::getAvgFps() const {
	auto t = _server ? _server->getFrameTiming() : core::FrameTimingInfo();
	return t.avgFrameInterval ? 1.0f / (t.avgFrameInterval / 1000000.0f) : 1.0f;
}

float Director::getSpf() const {
	auto t = _server ? _server->getFrameTiming() : core::FrameTimingInfo();
	return t.lastFrameTime ? t.lastFrameTime / 1000.0f : 1.0f;
}

float Director::getFenceFrameTime() const {
	auto t = _server ? _server->getFrameTiming() : core::FrameTimingInfo();
	return t.lastFenceFrameTime ? t.lastFenceFrameTime / 1000.0f : 1.0f;
}

float Director::getTimestampFrameTime() const {
	auto t = _server ? _server->getFrameTiming() : core::FrameTimingInfo();
	return t.lastTimestampFrameTime ? t.lastTimestampFrameTime / 1000.0f : 1.0f;
}

void Director::autorelease(Ref *ref) { _autorelease.emplace_back(ref); }

void Director::invalidate() { }

void Director::updateGeneralTransform() {
	auto transform = core::getPureTransform(_constraints.transform);

	Mat4 proj;
	switch (transform) {
	case core::SurfaceTransformFlags::Rotate90: proj = Mat4::ROTATION_Z_90; break;
	case core::SurfaceTransformFlags::Rotate180: proj = Mat4::ROTATION_Z_180; break;
	case core::SurfaceTransformFlags::Rotate270: proj = Mat4::ROTATION_Z_270; break;
	case core::SurfaceTransformFlags::Mirror: break;
	case core::SurfaceTransformFlags::MirrorRotate90: break;
	case core::SurfaceTransformFlags::MirrorRotate180: break;
	case core::SurfaceTransformFlags::MirrorRotate270: break;
	default: proj = Mat4::IDENTITY; break;
	}

	if (hasFlag(_constraints.transform, core::SurfaceTransformFlags::PreRotated)) {
		switch (transform) {
		case core::SurfaceTransformFlags::Rotate90:
		case core::SurfaceTransformFlags::Rotate270:
		case core::SurfaceTransformFlags::MirrorRotate90:
		case core::SurfaceTransformFlags::MirrorRotate270:
			proj.scale(2.0f / _constraints.extent.height, -2.0f / _constraints.extent.width, -1.0);
			break;
		default:
			proj.scale(2.0f / _constraints.extent.width, -2.0f / _constraints.extent.height, -1.0);
			break;
		}
	} else {
		proj.scale(2.0f / _constraints.extent.width, -2.0f / _constraints.extent.height, -1.0);
	}
	proj.m[12] = -1.0;
	proj.m[13] = 1.0f;
	proj.m[14] = 0.0f;
	proj.m[15] = 1.0f;

	switch (transform) {
	case core::SurfaceTransformFlags::Rotate90: proj.m[13] = -1.0f; break;
	case core::SurfaceTransformFlags::Rotate180:
		proj.m[12] = 1.0f;
		proj.m[13] = -1.0f;
		break;
	case core::SurfaceTransformFlags::Rotate270: proj.m[12] = 1.0f; break;
	case core::SurfaceTransformFlags::Mirror: break;
	case core::SurfaceTransformFlags::MirrorRotate90: break;
	case core::SurfaceTransformFlags::MirrorRotate180: break;
	case core::SurfaceTransformFlags::MirrorRotate270: break;
	default: break;
	}

	_generalProjection = proj;
}

bool Director::hasActiveInteractions() {
	return !_actionManager->empty() || _inputDispatcher->hasActiveInput();
}

} // namespace stappler::xenolith
