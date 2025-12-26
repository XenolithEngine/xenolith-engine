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
#include "XLCorePresentationEngine.h"
#include "XLContext.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Director::Director() { memset(&_drawStat, 0, sizeof(DrawStat)); }

Director::~Director() { log::source().info("Director", "~Director"); }

bool Director::init(NotNull<AppThread> app, const core::FrameConstraints &constraints,
		NotNull<AppWindow> window) {
	_application = app;
	_window = window;
	_engine = window->getPresentationEngine();
	_allocator = Rc<AllocRef>::alloc();
	_pool = Rc<PoolRef>::alloc(_allocator);
	_pool->perform([&, this] {
		_scheduler = Rc<Scheduler>::create();
		_actionManager = Rc<ActionManager>::create();
		_inputDispatcher = Rc<InputDispatcher>::create(_pool, _window->getWindowState());
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

bool Director::acquireFrame(FrameRequest *req) {
	if (!req) {
		return false;
	}

	if (_nextScene && !_scene) {
		// handle scene transition
		if (!req->getQueue() || req->getQueue() == _nextScene->getQueue()) {
			_scene = _nextScene;
			_nextScene = nullptr;
			_scene->setFrameConstraints(_constraints);
			updateGeneralTransform();
			_scene->handlePresented(this);
		}
	}

	if (!_scene) {
		log::source().error("xenolith::Director", "No scene defined for a FrameRequest");
		return false;
	}

	auto t = sp::platform::clock(ClockType::Monotonic);

	setFrameConstraints(req->getFrameConstraints());

	update(t);

	if (req->getQueue() && _scene->getQueue() != req->getQueue()) {
		log::source().error("xenolith::Director",
				"Scene render queue is not the same, as in FrameRequest, can't render with it");
		return false;
	}

	if (_scene) {
		req->setQueue(_scene->getQueue());
	}

	// break current stack frame, perform on next one
	_application->performOnAppThread([this, req = Rc<core::FrameRequest>(req)] {
		if (!_scene || !req) {
			return;
		}

		auto pool = Rc<PoolRef>::alloc(_allocator);

		pool->perform([&, this] {
			_scene->renderRequest(req, pool);

			if (hasActiveInteractions()) {
				if (_window) {
					_window->setReadyForNextFrame();
				}
			}
		});
	}, this, true);

	_avgFrameTime.addValue(sp::platform::clock(ClockType::Monotonic) - t);
	_avgFrameTimeValue = _avgFrameTime.getAverage();
	return true;
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

void Director::setWindow(AppWindow *w) {
	if (w != _window) {
		_textInput->cancel();
		if (w) {
			_window = w;
			_engine = w->getPresentationEngine();
			_inputDispatcher->resetWindowState(_window->getWindowState(), true);

			if (_scene && _scene->getQueue()->isCompiled()) {
				_window->getContext()->performOnThread(
						[w, q = _scene->getQueue()] { w->runWithQueue(q); }, w, false);
			}
		} else {
			_window = nullptr;
			_engine = nullptr;
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
					[](uint64_t id, Time time, const std::vector<std::string> &vec) {
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

	setWindow(nullptr);

	_autorelease.clear();
}

core::Loop *Director::getGlLoop() const { return _application->getContext()->getGlLoop(); }

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
	if (!scene || !_window) {
		return;
	}

	log::source().debug("Director", "runScene");

	auto linkId = retain();
	auto &queue = scene->getQueue();

	_nextScene = scene;

	// compile render queue
	getGlLoop()->compileQueue(queue,
			[this, scene = move(scene), linkId, w = Rc<AppWindow>(_window)](bool success) mutable {
		// now we on the main/view thread, call runWithQueue directly
		if (success) {
			auto &q = scene->getQueue();
			_window->getContext()->performOnThread([w, q] {
				w->runWithQueue(q);
				w->setReadyForNextFrame();
			}, w, false);
		}
		release(linkId);
	});
}

void Director::pushDrawStat(const DrawStat &stat) {
	_application->performOnAppThread([this, stat] { _drawStat = stat; }, this, false);
}

float Director::getFps() const {
	return _engine ? 1.0f / (_engine->getLastFrameInterval() / 1000000.0f) : 1.0f;
}

float Director::getAvgFps() const {
	return _engine ? 1.0f / (_engine->getAvgFrameInterval() / 1000000.0f) : 1.0f;
}

float Director::getSpf() const { return _engine ? _engine->getLastFrameTime() / 1000.0f : 1.0f; }

float Director::getFenceFrameTime() const {
	return _engine ? _engine->getLastFenceFrameTime() / 1000.0f : 1.0f;
}

float Director::getTimestampFrameTime() const {
	return _engine ? _engine->getLastTimestampFrameTime() / 1000.0f : 1.0f;
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
