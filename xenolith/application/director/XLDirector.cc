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

App-thread counterpart of the software frame budget (XL_SOFT_BUDGET): running averages printed
every N frames. Compare the two logs as averages of one run, not line by line.

	update  acquireFrame before posting the visit: scheduler, actions, input, application update().
	visit   the scene graph walk that builds the command list.
	spawned deferred tesselation tasks started by the visit; zero on a steady frame.

`work` (summed across worker threads) and `wait` (one thread idle) come from DrawStat and must not
be added together. */
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

// Runs on the app thread when the visit closes the account, so the counters need no
// synchronization. `stat` is the last DrawStat from the render side and may lag by a frame.
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

	// Microseconds, to match the frame budget output; the clocks below are nanoseconds.
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

	// The vertex stage's phases (VertexPlan), reported here because they arrive on DrawStat.
	// `damage` and `plan` are nested parts of the command walk, not additive with write and span.
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
	// Wire both ends of the render-session boundary (locally the AppWindow is both). Registering
	// the client here lets the server announce queues before the initial scene runs.
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
	// Set credentials first, on the app thread that owns them, then open the window, so every
	// window gets them, not only the first.
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

	// On the server, so _server is the actual window.
	info->window = static_cast<AppWindow *>(info->director->getRenderServer());
	info->queue = Rc<core::Queue>::create(sp::move(builder));

	if (info->queue) {
		_server->compileRenderQueue(info->queue, [info](bool success) {
			// Main thread here.

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
					// Allow a connecting client to take this window over. The listener's address
					// and key were set by whoever opened the session; with no server dictionary,
					// the client's suggestion is used.
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
	// Reset here, not in the visit: tasks started by the update belong to this frame too.
	_deferredSpawned = 0;
	// Nanosecond clock: `t` above is microseconds, too coarse for a short visit.
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
	//
	// The window can be destroyed before that: a preserved Director keeps its scene but loses the
	// server, and the frame belongs to the server it was acquired for.
	_application->performOnAppThread(
			[this, server = _server, req = Rc<core::FrameRequestProxy>(req.get())] {
		if (!_scene || !req || !_server || _server != server) {
			return;
		}

#if XL_FRAME_ACCOUNT
		const auto visitStart = core::getAccountClock();
		core::markFrame(core::FrameMark::VisitStart);
#endif

		/* The visit's phases are cleared here, not at the top of acquireFrame: the application
		update (which may read the account) runs before this and must see the previous frame's
		values. Guarded by XL_FRAME_ACCOUNT: `VisitAccount` does not exist without it. */
#if XL_FRAME_ACCOUNT
		getVisitAccount().clear();
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

		/* Flush glyph requests made during the visit, which gate this frame; otherwise they wait
		for the controller's flush at the next update(). Called after `perform`, since the batch
		outlives the frame pool. A flush before the visit is pointless: labels request glyphs
		while shaping, inside the visit. */
		_application->flushPendingFontGlyphs();

#if XL_FRAME_ACCOUNT
		/* The app half is closed here, not where acquireFrame returns: the visit runs on a later
		loop turn. Update and visit time are summed as this thread's total work for the frame. */
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
	// Reserved for client-side pacing/stats; no-op.
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

	// An already-compiled queue (adopted from QueueCache) must not be compiled again:
	// Queue::setCompiled overwrites the release callback, leaking the first set of render passes.
	if (queue->isCompiled()) {
		_server->attachRenderQueue(queue);
		sprt::release(this, linkId);
		return;
	}

	// Compile the render graph on the server, then make it active (attachRenderQueue does the
	// context-thread hop, runWithQueue and setReadyForNextFrame). `this` is kept alive by linkId;
	// the server endpoint stays valid because the Director retains the AppWindow.
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
