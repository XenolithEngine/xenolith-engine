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

/* Scope::TargetInside test. Either direction counts: TreeView and TableView put their DropTarget
on the widget above the scroll view, while a per-row target sits below it. */
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

	// Scheduled unconditionally: the drag layer sends no drag-start notification to arm on.
	scheduleUpdate();
}

void DragScrollSystem::handleRemoved() {
	_range = nullptr;
	_scrollBy = nullptr;
	System::handleRemoved();
}

void DragScrollSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	// Cleared, not resolved: the DragSystem may be installed later by the first DragSource, so
	// update() looks it up lazily. A cached pointer would also survive a reparent to another scene.
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
		/* The scroll position already counts downward from getScrollMinPosition(), so no sign flip
		here; the node-space flip is in the ramp in update(). */
		_range = [scroll]() -> Vec2 {
			const float pos = scroll->getScrollPosition();
			const float min = scroll->getScrollMinPosition();
			const float max = scroll->getScrollMaxPosition();
			if (sprt::isnan(pos) || sprt::isnan(min) || sprt::isnan(max)) {
				return Vec2::ZERO;
			}
			// Room before and after the current position, as (back, forward) on the scroll axis.
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
		if (!target || !DragScroll_sameBranch(target, _owner)) {
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

	// A third of the box at most, so a short list keeps a neutral middle zone.
	const float edge = sprt::min(_edge, box.height / 3.0f);
	if (edge <= 0.0f) {
		stop();
		return;
	}

	/* Node space is y-up: a pointer near the top of the box (large y) pulls the scroll offset
	backward, which is negative. */
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
		// Keeps frames coming for a drag driven by an API, which leaves no active input.
		if (!_owner->getActionByTag(RenderActionTag)) {
			_owner->runAction(Rc<RenderContinuously>::create(), RenderActionTag);
		}
	}

	// Re-resolve the drop position only when the content actually moved, so handleDragOver does
	// not become a per-frame event for every drag.
	if (_range().x != before) {
		_drag->refreshDrag();
	}
}

} // namespace stappler::xenolith::ui
