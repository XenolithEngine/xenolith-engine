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


#include "XLCompositorInputRouter.h"
#include "XLCompositorPipe.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compositor {

static bool InputRouter_contains(const IRect &rect, const Vec2 &pt) {
	return pt.x >= float(rect.x) && pt.y >= float(rect.y) && pt.x < float(rect.x) + rect.width
			&& pt.y < float(rect.y) + rect.height;
}

static bool InputRouter_contains(const URect &rect, const Vec2 &pt) {
	return pt.x >= float(rect.x) && pt.y >= float(rect.y) && pt.x < float(rect.x) + rect.width
			&& pt.y < float(rect.y) + rect.height;
}

bool InputRouter::init(DisplayPipe *pipe) {
	_pipe = pipe;
	_routes.reserve(MaxRoutes);
	return true;
}

void InputRouter::handleHostEvents(SpanView<core::InputEventData> events) {
	for (auto &it : events) {
		if (it.event == core::InputEventName::WindowState) {
			routeState(it);
		} else if (it.isKeyEvent()) {
			routeKey(it);
		} else if (it.isPointEvent()) {
			routePoint(it);
		}
	}
	flush();
}

void InputRouter::setHostState(core::WindowState state) {
	_hostState = state;
	updateFocused(_pipe->getFocusedPlane());
}

void InputRouter::handleFocusChanged(DisplayPlane *prev, DisplayPlane *next) {
	if (prev == next) {
		return;
	}

	cancelKeys();
	if (prev) {
		flush();
		prev->getWindow()->setVirtualState(core::WindowState::Focused, false);
	}
	updateFocused(next);
}

void InputRouter::handlePlaneHidden(DisplayPlane *plane) {
	for (auto it = _captures.begin(); it != _captures.end();) {
		if (it->second.plane == plane) {
			auto ev = it->second.last;
			ev.event = core::InputEventName::Cancel;
			deliver(plane, sp::move(ev));
			it = _captures.erase(it);
		} else {
			++it;
		}
	}

	if (_keyPlane == plane) {
		cancelKeys();
	}

	flush();

	if (_pointerPlane == plane) {
		setPointerPlane(nullptr);
	}
}

Vector<InputRouter::Route> InputRouter::getRoutes() const {
	if (_routes.size() < MaxRoutes) {
		return _routes;
	}
	Vector<Route> ret;
	ret.reserve(_routes.size());
	for (size_t i = 0; i < _routes.size(); ++i) {
		ret.emplace_back(_routes[(_nextRoute + i) % _routes.size()]);
	}
	return ret;
}

void InputRouter::clearRoutes() {
	_routes.clear();
	_nextRoute = 0;
}

StringView InputRouter::getReasonName(Reason reason) {
	switch (reason) {
	case Reason::None: return "none"; break;
	case Reason::Hit: return "hit"; break;
	case Reason::Capture: return "capture"; break;
	case Reason::Focus: return "focus"; break;
	}
	return StringView();
}

DisplayPlane *InputRouter::findPlane(const Vec2 &host) const {
	auto &snapshot = _pipe->getSnapshot();
	if (!snapshot) {
		return nullptr;
	}

	// Destination and input regions are y down, from the top of the output.
	auto extent = _pipe->getExtent();
	auto pt = Vec2(host.x, float(extent.height) - host.y);

	for (auto it = snapshot->planes.rbegin(); it != snapshot->planes.rend(); ++it) {
		auto &state = it->state;
		if (!InputRouter_contains(state.dst, pt)) {
			continue;
		}
		if (!state.inputRegion.empty()) {
			auto local = toPlane(it->plane, host);
			auto height = it->plane->getWindow()->getConstraints().extent.height;
			auto localDown = Vec2(local.x, float(height) - local.y);
			bool inside = false;
			for (auto &region : state.inputRegion) {
				if (InputRouter_contains(region, localDown)) {
					inside = true;
					break;
				}
			}
			if (!inside) {
				continue;
			}
		}
		return it->plane;
	}
	return nullptr;
}

Vec2 InputRouter::toPlane(const DisplayPlane *plane, const Vec2 &host) const {
	auto &state = plane->getState();
	auto extent = _pipe->getExtent();
	auto hostDown = Vec2(host.x, float(extent.height) - host.y);

	auto scaleX = state.dst.width ? float(state.src.width) / float(state.dst.width) : 1.0f;
	auto scaleY = state.dst.height ? float(state.src.height) / float(state.dst.height) : 1.0f;

	auto localDown = Vec2(float(state.src.x) + (hostDown.x - float(state.dst.x)) * scaleX,
			float(state.src.y) + (hostDown.y - float(state.dst.y)) * scaleY);

	auto height = plane->getWindow()->getConstraints().extent.height;
	return Vec2(localDown.x, float(height) - localDown.y);
}

void InputRouter::routePoint(const core::InputEventData &ev) {
	auto host = Vec2(ev.input.x, ev.input.y);

	DisplayPlane *target = nullptr;
	auto reason = Reason::None;

	switch (ev.event) {
	case core::InputEventName::Begin:
		target = findPlane(host);
		reason = target ? Reason::Hit : Reason::None;
		break;
	case core::InputEventName::Move:
	case core::InputEventName::End:
	case core::InputEventName::Cancel: {
		auto it = _captures.find(ev.id);
		if (it != _captures.end()) {
			target = it->second.plane;
			reason = Reason::Capture;
		}
		break;
	}
	case core::InputEventName::MouseMove:
	case core::InputEventName::Scroll:
		// A pressed pointer keeps the plane it went down in.
		if (!_captures.empty()) {
			target = _captures.begin()->second.plane;
			reason = Reason::Capture;
		} else {
			target = findPlane(host);
			reason = target ? Reason::Hit : Reason::None;
		}
		break;
	default: break;
	}

	if (!target) {
		addRoute(ev, nullptr, Vec2(), Reason::None);
		if (ev.event == core::InputEventName::MouseMove && _captures.empty()) {
			setPointerPlane(nullptr);
		}
		return;
	}

	auto local = toPlane(target, host);
	auto out = ev;
	out.input.x = local.x;
	out.input.y = local.y;

	addRoute(ev, target, local, reason);

	if (ev.event == core::InputEventName::Begin) {
		_captures.erase(ev.id);
		_captures.emplace(ev.id, Capture{target, out});
	} else if (ev.event == core::InputEventName::End || ev.event == core::InputEventName::Cancel) {
		_captures.erase(ev.id);
	} else if (ev.event == core::InputEventName::Move) {
		auto it = _captures.find(ev.id);
		if (it != _captures.end()) {
			it->second.last = out;
		}
	}

	setPointerPlane(target);
	deliver(target, sp::move(out));
}

void InputRouter::routeKey(const core::InputEventData &ev) {
	auto findPressed = [&](core::InputKeyCode code) {
		for (auto it = _pressedKeys.begin(); it != _pressedKeys.end(); ++it) {
			if (it->key.keycode == code) {
				return it;
			}
		}
		return _pressedKeys.end();
	};

	DisplayPlane *target = nullptr;

	switch (ev.event) {
	case core::InputEventName::KeyPressed:
		target = _pipe->getFocusedPlane();
		if (target) {
			if (_keyPlane != target) {
				cancelKeys();
				_keyPlane = target;
			}
			if (findPressed(ev.key.keycode) == _pressedKeys.end()) {
				_pressedKeys.emplace_back(ev);
			}
		}
		break;
	case core::InputEventName::KeyRepeated:
	case core::InputEventName::KeyReleased:
	case core::InputEventName::KeyCanceled: {
		// Only the plane that saw the press sees the rest of it.
		auto it = findPressed(ev.key.keycode);
		if (it != _pressedKeys.end()) {
			target = _keyPlane;
			if (ev.event != core::InputEventName::KeyRepeated) {
				_pressedKeys.erase(it);
			}
		}
		break;
	}
	default: break;
	}

	if (!target) {
		addRoute(ev, nullptr, Vec2(), Reason::None);
		return;
	}

	// A key carries where the pointer is; the plane's listeners are hit-tested with it
	auto out = ev;
	Vec2 local;
	if (ev.hasLocation()) {
		local = toPlane(target, Vec2(ev.input.x, ev.input.y));
		out.input.x = local.x;
		out.input.y = local.y;
	}
	addRoute(ev, target, local, Reason::Focus);
	deliver(target, sp::move(out));
}

void InputRouter::routeState(const core::InputEventData &ev) {
	auto changes = ev.window.changes;
	_hostState = ev.window.state;

	if (hasFlag(changes, core::WindowState::Focused)) {
		flush();
		updateFocused(_pipe->getFocusedPlane());
	}

	if (hasFlag(changes, core::WindowState::Pointer)
			&& !hasFlag(_hostState, core::WindowState::Pointer) && _captures.empty()) {
		flush();
		setPointerPlane(nullptr);
	}
}

void InputRouter::deliver(DisplayPlane *plane, core::InputEventData &&ev) {
	if (!_batches.empty() && _batches.back().plane == plane) {
		_batches.back().events.emplace_back(sp::move(ev));
		return;
	}
	auto &batch = _batches.emplace_back(Batch{plane});
	batch.events.emplace_back(sp::move(ev));
}

void InputRouter::flush() {
	// In order: a batch per run of events for one plane.
	auto batches = sp::move(_batches);
	_batches.clear();
	for (auto &it : batches) {
		it.plane->getWindow()->handleNativeInputEvents(sp::move(it.events));
	}
}

void InputRouter::setPointerPlane(DisplayPlane *plane) {
	if (_pointerPlane == plane) {
		return;
	}

	// The state change travels after the events already routed to the plane.
	flush();

	if (_pointerPlane) {
		_pointerPlane->getWindow()->setVirtualState(core::WindowState::Pointer, false);
	}
	_pointerPlane = plane;
	if (_pointerPlane) {
		_pointerPlane->getWindow()->setVirtualState(core::WindowState::Pointer, true);
	}
}

void InputRouter::updateFocused(DisplayPlane *plane) {
	if (plane) {
		plane->getWindow()->setVirtualState(core::WindowState::Focused,
				hasFlag(_hostState, core::WindowState::Focused));
	}
}

void InputRouter::cancelKeys() {
	if (_keyPlane) {
		for (auto &it : _pressedKeys) {
			auto ev = it;
			ev.event = core::InputEventName::KeyCanceled;
			deliver(_keyPlane, sp::move(ev));
		}
		flush();
	}
	_pressedKeys.clear();
	_keyPlane = nullptr;
}

void InputRouter::addRoute(const core::InputEventData &ev, const DisplayPlane *plane,
		const Vec2 &local, Reason reason) {
	Route route;
	route.event = ev.event;
	route.id = ev.id;
	if (ev.hasLocation()) {
		route.host = Vec2(ev.input.x, ev.input.y);
		route.local = local;
	}
	route.plane = plane ? plane->getId() : 0;
	route.reason = reason;

	if (_routes.size() < MaxRoutes) {
		_routes.emplace_back(route);
	} else {
		_routes[_nextRoute] = route;
		_nextRoute = (_nextRoute + 1) % MaxRoutes;
	}
}

} // namespace stappler::xenolith::compositor
