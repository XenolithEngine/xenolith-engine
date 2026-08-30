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

#include "XLUiDragScrollSystem.h"
#include "XLUiScrollSystem.h"
#include "XL2dScrollView.h"
#include "XLAction.h"
#include "XLDropTarget.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

static bool DragScroll_isWithin(const Node *node, const Node *root) {
	for (auto it = node; it; it = it->getParent()) {
		if (it == root) {
			return true;
		}
	}
	return false;
}

/* Are the drop target and this scroller on the same branch? What Scope::TargetInside asks.

Either direction counts, and both really occur: ui::TreeView and ui::TableView put their DropTarget
on the WIDGET, which is the scroll view's parent, while a target attached to an individual row sits
below it. Testing only one direction silently disables the pull for whichever arrangement was not
the one in mind - which is exactly what a plain "is the target inside me" answered for the table. */
static bool DragScroll_sameBranch(const Node *target, const Node *owner) {
	return DragScroll_isWithin(target, owner) || DragScroll_isWithin(owner, target);
}

} // namespace

DragScrollSystem *DragScrollSystem::acquireForNode(NotNull<Node> node) {
	if (auto existing = node->getSystemByType<DragScrollSystem>()) {
		return existing;
	}
	return node->addSystem(Rc<DragScrollSystem>::create());
}

bool DragScrollSystem::init() {
	if (!System::init()) {
		return false;
	}

	return true;
}

void DragScrollSystem::handleAdded(Node *node) {
	System::handleAdded(node);
	resolveScroller();

	// Scheduled unconditionally, and cheap when nothing is dragging: one null test per frame. The
	// alternative - arming on the first drag - needs a notification the drag layer does not send.
	scheduleUpdate();
}

void DragScrollSystem::handleRemoved() {
	_range = nullptr;
	_scrollBy = nullptr;
	System::handleRemoved();
}

void DragScrollSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	// Cleared, not resolved: there may be no DragSystem yet. One is installed by the first
	// DragSource that needs it, which for a virtualized list is when a row builds its grip - long
	// after the scroll view entered the scene. So the answer is looked up lazily in update(), and
	// dropped here because findForNode walks the parent chain and a cached pointer would survive a
	// reparent into a different scene.
	_drag = nullptr;

	// The scroller may have been given to the owner after this system was added.
	if (!_scrollBy) {
		resolveScroller();
	}
}

void DragScrollSystem::handleExit() {
	_drag = nullptr;
	_scrolling = false;
	if (_owner) {
		_owner->stopAllActionsByTag(RenderActionTag);
	}
	System::handleExit();
}

void DragScrollSystem::setSpeed(float value) { _speed = sprt::max(value, 0.0f); }

void DragScrollSystem::setEdge(float value) { _edge = sprt::max(value, 0.0f); }

void DragScrollSystem::setScope(Scope value) { _scope = value; }

void DragScrollSystem::resolveScroller() {
	_range = nullptr;
	_scrollBy = nullptr;

	if (!_owner) {
		return;
	}

	if (auto scroll = dynamic_cast<basic2d::ScrollViewBase *>(_owner)) {
		/* One axis, one float, and it already counts DOWNWARD: getScrollMinPosition() is the top of
		the content. So there is no sign flip here - the flip the text widgets warn about is between
		NODE space, which is Y-up, and the offset, and it belongs in the ramp below. */
		_range = [scroll]() -> Vec2 {
			const float pos = scroll->getScrollPosition();
			const float min = scroll->getScrollMinPosition();
			const float max = scroll->getScrollMaxPosition();
			if (sprt::isnan(pos) || sprt::isnan(min) || sprt::isnan(max)) {
				return Vec2::ZERO;
			}
			// Room left BEFORE the current position and AFTER it, packed as (back, forward) on the
			// axis this view scrolls.
			const Vec2 room(sprt::max(pos - min, 0.0f), sprt::max(max - pos, 0.0f));
			return scroll->isVertical() ? room : Vec2(room.x, 0.0f);
		};

		_scrollBy = [scroll](Vec2 delta) {
			const float d = scroll->isVertical() ? delta.y : delta.x;
			const float min = scroll->getScrollMinPosition();
			const float max = scroll->getScrollMaxPosition();
			if (sprt::isnan(min) || sprt::isnan(max)) {
				return;
			}
			scroll->setScrollPosition(sprt::clamp(scroll->getScrollPosition() + d, min, max));
		};
		return;
	}

	if (auto system = _owner->getSystemByType<ScrollSystem>()) {
		_range = [system]() -> Vec2 {
			const auto range = system->getScrollRange();
			const auto pos = system->getScrollPosition();
			return Vec2(sprt::max(pos.y, 0.0f), sprt::max(range.height - pos.y, 0.0f));
		};
		_scrollBy = [system](Vec2 delta) { system->scrollBy(Vec2(0.0f, delta.y)); };
	}
}

void DragScrollSystem::update(const UpdateTime &time) {
	System::update(time);

	auto stop = [this] {
		if (_scrolling) {
			_scrolling = false;
			if (_owner) {
				_owner->stopAllActionsByTag(RenderActionTag);
			}
		}
	};

	// Lazily, and only while the answer is missing: see handleEnter.
	if (!_drag && _owner) {
		_drag = DragSystem::findForNode(_owner);
	}

	if (!_owner || !_scrollBy || !_range || !_drag || !_drag->isDragging()) {
		stop();
		return;
	}

	auto session = _drag->getSession();
	if (_scope == Scope::TargetInside) {
		auto target = session->getTarget();
		auto owner = target ? target->getOwner() : nullptr;
		if (!owner || !DragScroll_sameBranch(owner, _owner)) {
			stop();
			return;
		}
	}

	const auto box = _owner->getContentSize();
	const auto local = _owner->convertToNodeSpace(session->getWorldLocation());
	if (local.x < 0.0f || local.y < 0.0f || local.x > box.width || local.y > box.height) {
		stop();
		return;
	}

	// A third of the box at most: on a short list a band at each end would meet in the middle and
	// there would be no neutral zone left to hold the pointer still in.
	const float edge = sprt::min(_edge, box.height / 3.0f);
	if (edge <= 0.0f) {
		stop();
		return;
	}

	/* The ramp, and the one sign flip in this file: node space is Y-UP, so the pointer being near
	the TOP of the box (large y) has to pull the scroll offset BACKWARD, which is negative. */
	float ramp = 0.0f;
	if (local.y > box.height - edge) {
		ramp = -(local.y - (box.height - edge)) / edge;
	} else if (local.y < edge) {
		ramp = (edge - local.y) / edge;
	}

	if (ramp == 0.0f) {
		stop();
		return;
	}

	const auto room = _range();
	if ((ramp < 0.0f && room.x <= 0.0f) || (ramp > 0.0f && room.y <= 0.0f)) {
		stop(); // already against that end; nothing to give
		return;
	}

	const float delta = ramp * _speed * time.dt;
	const float before = room.x;
	_scrollBy(Vec2(0.0f, delta));

	if (!_scrolling) {
		_scrolling = true;
		// Insurance, not the clock: a live pointer already keeps frames coming, but a drag driven
		// by an API rather than by a gesture puts nothing in the dispatcher's active set.
		if (!_owner->getActionByTag(RenderActionTag)) {
			_owner->runAction(Rc<RenderContinuously>::create(), RenderActionTag);
		}
	}

	// ONLY when something actually moved. Drag events come with pointer motion alone, so the drop
	// position the target resolved is now about a row that has slid away - but asking for a
	// re-resolve on every frame regardless would make handleDragOver a 60Hz event for every drag in
	// the scene, to fix a case that arises only here.
	if (_range().x != before) {
		_drag->refreshDrag();
	}
}

} // namespace stappler::xenolith::ui
