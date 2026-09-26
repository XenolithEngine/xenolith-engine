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

#include "XLCompositorPipe.h"
#include "XLAppWindow.h"
#include "XLServerAppThread.h"
#include "XLContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

// A host frame asked for and not shown within this long is taken as lost.
static constexpr uint64_t Pipe_watchdogMinUs = 250'000;

DisplayPlane::~DisplayPlane() {
	if (_link) {
		_link->plane = nullptr;
	}
}

bool DisplayPlane::init(DisplayPipe *pipe, uint32_t id, NotNull<AppWindow> window) {
	_source = window->getPlaneSource();
	if (!_source) {
		log::source().error("compositor::DisplayPlane", "window '", window->getId(),
				"' publishes no frames: only a virtual window can be a plane");
		return false;
	}

	_pipe = pipe;
	_id = id;
	_window = window.get();
	_link = Rc<DisplayPlaneLink>::alloc();
	_link->plane = this;
	return true;
}

DisplayPipe::~DisplayPipe() {
	if (_vblankTimer) {
		_vblankTimer->cancel();
	}
	if (_watchdog) {
		_watchdog->cancel();
	}
}

bool DisplayPipe::init(NotNull<ServerAppThread> app) {
	_app = app.get();
	_inputRouter = Rc<InputRouter>::create(this);
	_snapshot = Rc<PipeSnapshot>::alloc();
	_snapshot->background = _background;
	return true;
}

Rc<DisplayPlane> DisplayPipe::createPlane(NotNull<AppWindow> window) {
	auto plane = Rc<DisplayPlane>::create(this, _nextPlaneId, window);
	if (!plane) {
		return nullptr;
	}

	++_nextPlaneId;
	_planes.emplace_back(plane);

	// Paused until enabled.
	window->setVirtualState(core::WindowState::Minimized, true);

	// The listener is set and called on the presentation thread; the pipe lives on this one.
	_app->getContext()->performOnThread(
			[source = plane->_source, link = plane->_link,
					looper = Rc<sprt::dispatch::Looper>(_app->getLooper())] {
		source->setListener([link, looper](NotNull<core::PlaneFrame> frame) {
			looper->performOnThread([link, serial = frame->getSerial()] {
				auto plane = link->plane;
				if (auto pipe = plane ? plane->getPipe() : nullptr) {
					pipe->handlePlanePublished(plane, serial);
				}
			}, link);
		});
	},
			plane);

	return plane;
}

void DisplayPipe::destroyPlane(NotNull<DisplayPlane> plane) {
	if (plane->_pipe != this) {
		return;
	}

	if (_focused == plane.get()) {
		_inputRouter->handleFocusChanged(_focused, nullptr);
		_focused = nullptr;
	}
	_inputRouter->handlePlaneHidden(plane);

	_app->getContext()->performOnThread([source = plane->_source] { source->setListener(nullptr); },
			plane.get());

	if (plane->_paced) {
		plane->_window->setExternalDisplayLink(false);
	}

	plane->_pipe = nullptr;
	plane->_link->plane = nullptr;

	for (auto it = _planes.begin(); it != _planes.end(); ++it) {
		if (it->get() == plane.get()) {
			_planes.erase(it);
			break;
		}
	}

	publishSnapshot();
}

Status DisplayPipe::commit() {
	uint32_t enabled = 0;
	for (auto &it : _planes) {
		if (!it->_pending.enabled) {
			continue;
		}
		++enabled;
		auto st = validate(it->_pending);
		if (!sprt::status::isSuccessful(st)) {
			return st;
		}
	}

	if (enabled > _caps.maxPlanes) {
		return Status::ErrorNotSupported;
	}

	for (auto &it : _planes) {
		auto wasEnabled = it->_state.enabled;
		it->_state = it->_pending;

		if (wasEnabled == it->_state.enabled) {
			continue;
		}

		if (it->_state.enabled) {
			it->_window->setVirtualState(core::WindowState::Minimized, false);
			// the vblank it missed while paused
			if (it->_paced) {
				emitDisplayLink(it);
			}
		} else {
			it->_window->setVirtualState(core::WindowState::Minimized, true);
		}
	}

	publishSnapshot();

	// After the snapshot: a paused plane is no longer found under the pointer.
	for (auto &it : _planes) {
		if (!it->_state.enabled) {
			_inputRouter->handlePlaneHidden(it);
		}
	}
	return Status::Ok;
}

void DisplayPipe::setFocusedPlane(DisplayPlane *plane) {
	if (plane && plane->_pipe != this) {
		return;
	}
	if (_focused == plane) {
		return;
	}
	auto prev = _focused;
	_focused = plane;
	_inputRouter->handleFocusChanged(prev, _focused);
}

void DisplayPipe::setVblankCallback(VblankCallback &&cb) { _vblankCallback = sp::move(cb); }

void DisplayPipe::setPlanePublishedCallback(PublishedCallback &&cb) {
	_publishedCallback = sp::move(cb);
}

void DisplayPipe::setMinFrameInterval(uint64_t value) { _minInterval = value; }

void DisplayPipe::setBackground(const Color4F &color) {
	_background = color;
	publishSnapshot();
}

DisplayPlane *DisplayPipe::getPlane(uint32_t id) const {
	for (auto &it : _planes) {
		if (it->getId() == id) {
			return it;
		}
	}
	return nullptr;
}

DisplayPlane *DisplayPipe::getPlane(const AppWindow *window) const {
	for (auto &it : _planes) {
		if (it->getWindow() == window) {
			return it;
		}
	}
	return nullptr;
}

void DisplayPipe::handleHostPresented(uint64_t) {
	++_hostFrames;
	_hostFrameOwed = false;

	auto now = sp::platform::clock(ClockType::Monotonic);
	auto elapsed = now - _lastVblank;
	if (_minInterval == 0 || elapsed >= _minInterval) {
		emitVblank();
		return;
	}

	if (!_vblankTimer) {
		_vblankTimer = _app->getLooper()->schedule(
				sprt::dispatch::TimeInterval::microseconds(_minInterval - elapsed),
				[this](sprt::dispatch::Handle *, bool success) {
			_vblankTimer = nullptr;
			if (success) {
				emitVblank();
			}
		}, this);
	}
}

void DisplayPipe::handleHostFrameDeclined() { _hostFrameOwed = false; }

void DisplayPipe::handlePlanePublished(DisplayPlane *plane, uint64_t serial) {
	++plane->_published;
	plane->_latestSerial = serial;

	// From its first frame on, the window draws on the pipe's vblank. Not earlier: the scene that
	// shares the window with its client needs a frame of its own before it does.
	if (!plane->_paced) {
		plane->_paced = true;
		plane->_window->setExternalDisplayLink(true);
	}

	if (plane->_state.enabled) {
		scheduleHostFrame();
	}

	if (_publishedCallback) {
		_publishedCallback(plane, serial);
	}
}

Status DisplayPipe::validate(const PlaneState &state) const {
	if (state.src.width == 0 || state.src.height == 0 || state.dst.width == 0
			|| state.dst.height == 0) {
		return Status::ErrorInvalidArguemnt;
	}
	if (state.alpha < 0.0f || state.alpha > 1.0f) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_caps.scaling
			&& (state.src.width != state.dst.width || state.src.height != state.dst.height)) {
		return Status::ErrorNotSupported;
	}
	if (!_caps.planeAlpha && state.alpha != 1.0f) {
		return Status::ErrorNotSupported;
	}
	if (!_caps.premultiplied && state.blend == PlaneBlend::Premultiplied) {
		return Status::ErrorNotSupported;
	}
	return Status::Ok;
}

void DisplayPipe::publishSnapshot() {
	auto snapshot = Rc<PipeSnapshot>::alloc();
	snapshot->serial = (_snapshot ? _snapshot->serial : 0) + 1;
	snapshot->background = _background;

	for (auto &it : _planes) {
		if (it->_state.enabled) {
			snapshot->planes.emplace_back(PipeSnapshot::Entry{it, it->_state});
		}
	}

	// ties keep creation order, which is also the id order
	sprt::stable_sort(snapshot->planes.begin(), snapshot->planes.end(),
			[](const PipeSnapshot::Entry &l, const PipeSnapshot::Entry &r) {
		return l.state.z < r.state.z;
	});

	_snapshot = snapshot;
	scheduleHostFrame();
}

void DisplayPipe::scheduleHostFrame() {
	if (!hasOutput()) {
		return;
	}

	_hostFrameOwed = true;

	if (!_watchdog) {
		auto timeout = sprt::max(_minInterval * 4, Pipe_watchdogMinUs);
		_watchdog = _app->getLooper()->schedule(sprt::dispatch::TimeInterval::microseconds(timeout),
				[this](sprt::dispatch::Handle *, bool success) {
			_watchdog = nullptr;
			if (success && _hostFrameOwed) {
				// Planes wait for a vblank that did not come: give them one rather than stall.
				log::source().warn("compositor::DisplayPipe", "host frame not shown in time");
				_hostFrameOwed = false;
				emitVblank();
			}
		}, this);
	}

	requestHostFrame();
}

void DisplayPipe::emitVblank() {
	_lastVblank = sp::platform::clock(ClockType::Monotonic);
	++_vblanks;

	for (auto &it : _planes) {
		if (it->_state.enabled && it->_paced) {
			emitDisplayLink(it);
		}
	}

	if (_vblankCallback) {
		_vblankCallback(_hostFrames);
	}
}

void DisplayPipe::emitDisplayLink(DisplayPlane *plane) {
	plane->_window->emitDisplayLink();
	++plane->_displayLinks;
}

} // namespace stappler::xenolith::compositor
