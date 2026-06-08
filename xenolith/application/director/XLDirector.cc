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

bool Director::acquireFrame(NotNull<core::FrameRequestProxy> req) {
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
		return false;
	}

	auto t = sp::platform::clock(ClockType::Monotonic);

	setFrameConstraints(req->getFrameConstraints());

	update(t);

	// Pick this frame's render graph by name; the server resolves it against its registry of
	// compiled queues (selectQueue logs if the name is unknown).
	req->selectQueue(_scene->getQueue());

	// break current stack frame, perform on next one
	_application->performOnAppThread([this, req = Rc<core::FrameRequestProxy>(req.get())] {
		if (!_scene || !req) {
			return;
		}

		auto pool = Rc<sprt::PoolRef>::alloc(_allocator);

		pool->perform([&, this] {
			_scene->renderRequest(req, pool);

			if (hasActiveInteractions()) {
				if (_server) {
					_server->setReadyForNextFrame();
				}
			}
		});
	}, this, true);

	_avgFrameTime.addValue(sp::platform::clock(ClockType::Monotonic) - t);
	_avgFrameTimeValue = _avgFrameTime.getAverage();
	return true;
}

void Director::handleRenderQueueAttached(const Rc<core::Queue> &queue) {
	// The server announced an available render graph; record it so the client can select it by
	// name per frame (FrameRequestProxy::selectQueue).
	if (queue) {
		_availableQueues.insert_or_assign(queue->getName(), queue);
	}
}

void Director::handleConstraintsChanged(const core::FrameConstraints &c) { setFrameConstraints(c); }

void Director::handleInputEvents(Vector<core::InputEventData> &&events) {
	for (auto &event : events) {
		if (event.isPointEvent()) {
			event.point.density = _constraints.density;
		}
		_inputDispatcher->handleInputEvent(event);
	}
}

void Director::handleTextInput(const core::TextInputState &state) {
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

void Director::pushDrawStat(const DrawStat &stat) {
	_application->performOnAppThread([this, stat] { _drawStat = stat; }, this, false);
}

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

bool Director::setListenAddress(StringView addr) { return _application->setListenAddress(addr); }

bool Director::shareWindow() {
	if (auto w = dynamic_cast<AppWindow *>(_server)) {
		return _application->shareWindow(w);
	}
	return false;
}

bool Director::shareQueue(NotNull<core::Queue> queue) { return _application->shareQueue(queue); }

bool Director::setBearerKey(BytesView key) { return _application->setBearerKey(key); }

bool Director::setCompressionDictionary(BytesView d) {
	return _application->setCompressionDictionary(d);
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
