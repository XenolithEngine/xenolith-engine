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


#include "XLUiMarquee.h"
#include "XLUiLayoutSystem.h"
#include "XLDragSource.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLAction.h"
#include <sprt/runtime/geom/viewport.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

static bool Marquee_isWithin(const Node *node, const Node *root) {
	for (auto it = node; it; it = it->getParent()) {
		if (it == root) {
			return true;
		}
	}
	return false;
}

} // namespace

bool MarqueeSystem::init(MarqueeSlots &&slots, basic2d::ScrollViewBase *scroll) {
	if (!InputListener::init(DefaultPriority)) {
		return false;
	}

	_slots = sp::move(slots);
	_scroll = scroll;
	_scroller = makeEdgeScroller(scroll);

	addSwipeRecognizer([this](const GestureSwipe &swipe) { return handleSwipe(swipe); },
			InputSwipeInfo{makeButtonMask({InputMouseButton::MouseLeft})});

	// Asked before the scene, which would otherwise take Escape as a step back.
	addHotkey(EngineHotkeys::get().back,
			[this](HotkeyId, const InputEvent &) { return handleBack(); });
	return true;
}

void MarqueeSystem::handleExit() {
	cancel();
	_captured = false;
	InputListener::handleExit();
}

void MarqueeSystem::update(const UpdateTime &time) {
	InputListener::update(time);
	if (!_active) {
		return;
	}

	// The content can move with no pointer event: the pull, the wheel, a fling, a rebuild.
	pull(time.dt);
	updateBand(false);
}

void MarqueeSystem::setEnabled(bool value) {
	if (!value) {
		cancel();
	}
	InputListener::setEnabled(value);
}

void MarqueeSystem::setEdge(float value) { _edge = sprt::max(value, 0.0f); }

void MarqueeSystem::setSpeed(float value) { _speed = sprt::max(value, 0.0f); }

void MarqueeSystem::setBandZOrder(ZOrder value) {
	_bandZOrder = value;
	if (_band && _band->getParent()) {
		_band->getParent()->reorderChild(_band, value);
	}
}

Rect MarqueeSystem::getRect() const { return _active ? _event.rect : Rect::ZERO; }

Rect MarqueeSystem::getBandRect() const {
	return (_active && _band && _band->isVisible()) ? _bandRect : Rect::ZERO;
}

Rect MarqueeSystem::getViewport() const {
	if (_slots.viewport) {
		return _slots.viewport();
	}
	if (!_owner) {
		return Rect::ZERO;
	}
	if (_scroll && _scroll != _owner) {
		const auto &size = _scroll->getContentSize();
		return sprt::geom::rectFromPoints(
				_owner->convertToNodeSpace(_scroll->convertToWorldSpace(Vec2::ZERO)),
				_owner->convertToNodeSpace(
						_scroll->convertToWorldSpace(Vec2(size.width, size.height))));
	}
	const auto &size = _owner->getContentSize();
	return Rect(0.0f, 0.0f, size.width, size.height);
}

void MarqueeSystem::refresh() { updateBand(true); }

void MarqueeSystem::cancel() { finish(false); }

bool MarqueeSystem::handleSwipe(const GestureSwipe &swipe) {
	switch (swipe.event) {
	case GestureEvent::Began: {
		const auto modifiers = swipe.input->originalModifiers;
		if (hasFlag(modifiers, InputModifier::Touch) || !canBeginAt(swipe.input->originalLocation)) {
			return false;
		}

		auto content = getContent();
		MarqueeEvent ev;
		ev.press = swipe.input->originalLocation;
		ev.location = swipe.input->currentLocation;
		ev.modifiers = modifiers;
		ev.op = getListSweepOp(modifiers);
		ev.origin = content->convertToNodeSpace(ev.press);
		ev.point = content->convertToNodeSpace(ev.location);
		ev.rect = sprt::geom::rectFromPoints(ev.origin, ev.point);
		ev.hostRect = sprt::geom::rectFromPoints(_owner->convertToNodeSpace(ev.press),
				_owner->convertToNodeSpace(ev.location));
		if (!_slots.begin(ev)) {
			return false;
		}

		_event = ev;
		_active = true;
		_captured = true;
		setExclusive();
		scheduleUpdate();
		updateBand(true);
		return true;
	}
	case GestureEvent::Activated:
		if (!_captured) {
			return false;
		}
		_event.location = swipe.input->currentLocation;
		updateBand(false);
		return true;
	case GestureEvent::Ended:
		if (!_captured) {
			return false;
		}
		_captured = false;
		finish(true);
		return true;
	case GestureEvent::Cancelled:
		_captured = false;
		finish(false);
		return true;
	}
	return false;
}

bool MarqueeSystem::handleBack() {
	if (!_active) {
		return false;
	}
	cancel();
	return true;
}

bool MarqueeSystem::canBeginAt(const Vec2 &world) const {
	if (!_enabled || !_running || !_owner || !_slots.begin || !getContent()) {
		return false;
	}

	if (!getViewport().containsPoint(_owner->convertToNodeSpace(world))) {
		return false;
	}

	/* The listener is asked before the scene, so what is drawn over the host is found here: the
	topmost node with a listener at the press must be the host or inside it, and nothing between it
	and the host may be a drag source of its own. An ancestor never covers the host, though one
	that listens after its children is registered above them. */
	auto director = _owner->getDirector();
	auto dispatcher = director ? director->getInputDispatcher() : nullptr;
	if (!dispatcher) {
		return true;
	}

	Node *top = nullptr;
	dispatcher->foreachHitTest(HitTestFlags::Pointer,
			[&](const InputListenerStorage::HitTestRec &rec) {
		if (rec.node != _owner && Marquee_isWithin(_owner, rec.node)) {
			return true;
		}
		if (!rec.contains(world)) {
			return true;
		}
		top = rec.node;
		return false;
	});

	if (!top) {
		return true;
	}
	if (!Marquee_isWithin(top, _owner)) {
		return false;
	}
	for (auto it = top; it && it != _owner; it = it->getParent()) {
		auto source = it->getSystemByType<DragSource>();
		if (source && source->isEnabled()) {
			return false;
		}
	}
	return true;
}

Node *MarqueeSystem::getContent() const {
	if (_scroll) {
		return _scroll->getRoot();
	}
	return _owner;
}

void MarqueeSystem::updateBand(bool force) {
	auto content = getContent();
	if (!_active || !content || !_owner) {
		return;
	}

	const auto point = content->convertToNodeSpace(_event.location);
	const auto rect = sprt::geom::rectFromPoints(_event.origin, point);
	const auto hostRect = sprt::geom::rectFromPoints(
			_owner->convertToNodeSpace(content->convertToWorldSpace(_event.origin)),
			_owner->convertToNodeSpace(_event.location));
	if (!force && rect.equals(_event.rect) && hostRect.equals(_event.hostRect)) {
		return;
	}

	_event.point = point;
	_event.rect = rect;
	_event.hostRect = hostRect;
	layoutBand();

	if (_slots.update) {
		_slots.update(_event);
	}
}

void MarqueeSystem::layoutBand() {
	if (!_band) {
		auto band = Rc<Panel>::create();
		band->setType("marquee");
		band->removeStyleClass("xl-ui-panel");
		band->addStyleClass("xl-ui-marquee");
		Panel::registerStyleAppliers("marquee");
		band->setPathColor(Color4B(0xFC, 0xB4, 0x00, 0x1E), true);
		band->setOutline(Color4B(0xFC, 0xB4, 0x00, 0xFF), 1.0f);
		// blended over the rows, which may be drawn at the Solid level
		band->setRenderingLevel(RenderingLevel::Transparent);
		band->setAnchorPoint(Anchor::BottomLeft);
		band->setComponent<OutOfFlowComponent>(OutOfFlowComponent{false});
		_band = band;
	}
	if (_band->getParent() != _owner) {
		if (_band->getParent()) {
			_band->removeFromParent();
		}
		_owner->addChild(_band, _bandZOrder);
	}

	// Cut to the viewport by hand: a list does not clip what is drawn over it.
	const auto viewport = getViewport();
	const auto &band = _event.hostRect;
	const float minX = sprt::max(band.getMinX(), viewport.getMinX());
	const float minY = sprt::max(band.getMinY(), viewport.getMinY());
	const float maxX = sprt::min(band.getMaxX(), viewport.getMaxX());
	const float maxY = sprt::min(band.getMaxY(), viewport.getMaxY());
	if (maxX < minX || maxY < minY) {
		_bandRect = Rect::ZERO;
		_band->setVisible(false);
		return;
	}

	// A line still shows while a drag runs along an edge.
	_bandRect = Rect(minX, minY, sprt::max(maxX - minX, 1.0f), sprt::max(maxY - minY, 1.0f));
	_band->setPosition(_bandRect.origin);
	_band->setContentSize(_bandRect.size);
	_band->setVisible(true);
}

void MarqueeSystem::pull(float dt) {
	auto stop = [this] {
		if (_pulling) {
			_pulling = false;
			if (_owner) {
				_owner->stopAllActionsByTag(RenderActionTag);
			}
		}
	};

	if (_scroller.empty() || !_owner) {
		stop();
		return;
	}

	const auto viewport = getViewport();
	const auto local = _owner->convertToNodeSpace(_event.location);
	const float ramp = getEdgeScrollRamp(local.y - viewport.getMinY(), viewport.size.height, _edge,
			true);
	const auto room = _scroller.range();
	if (ramp == 0.0f || (ramp < 0.0f && room.x <= 0.0f) || (ramp > 0.0f && room.y <= 0.0f)) {
		stop();
		return;
	}

	_scroller.scrollBy(Vec2(0.0f, ramp * _speed * dt));

	if (!_pulling) {
		_pulling = true;
		// A pointer parked at the edge sends no events, and without them no frames are drawn.
		if (!_owner->getActionByTag(RenderActionTag)) {
			_owner->runAction(Rc<RenderContinuously>::create(), RenderActionTag);
		}
	}
}

void MarqueeSystem::finish(bool commit) {
	if (!_active) {
		return;
	}

	_active = false;
	if (_pulling) {
		_pulling = false;
		if (_owner) {
			_owner->stopAllActionsByTag(RenderActionTag);
		}
	}
	unscheduleUpdate();
	if (_band) {
		_band->setVisible(false);
	}
	_bandRect = Rect::ZERO;

	if (_slots.end) {
		const auto ev = _event;
		_slots.end(ev, commit);
	}
}

} // namespace stappler::xenolith::ui
