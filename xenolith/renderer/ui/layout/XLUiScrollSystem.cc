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

#include "XLUiScrollSystem.h"
#include "XLUiPanel.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// How far one wheel notch scrolls, in points. The wheel reports notches, not distance.
static constexpr float ScrollSystem_wheelStep = 48.0f;

// How long a notch takes to ease in. Short enough to feel immediate, long enough to read as motion
// rather than a jump - which is what makes it possible to see WHERE the content went.
static constexpr float ScrollSystem_wheelDuration = 0.1f;

// Overlay indicator geometry. Overlay, not gutter: the thumb floats over the content and reserves
// no space, so turning it on never re-lays-out anything.
static constexpr float ScrollSystem_indicatorThickness = 6.0f;
static constexpr float ScrollSystem_indicatorMinLength = 24.0f;
static constexpr float ScrollSystem_indicatorInset = 2.0f;

// Fling: the velocity is scaled by this factor per second, and dropped once it is this slow.
static constexpr float ScrollSystem_flingDecay = 0.002f;
static constexpr float ScrollSystem_flingCutoff = 8.0f;

// `auto` indicators hold for this long after the last movement, then fade over this long.
static constexpr float ScrollSystem_indicatorHold = 0.9f;
static constexpr float ScrollSystem_indicatorFade = 0.25f;

// An indicator already parented to `owner` and carrying `cls`, or null. The ScrollSystem can be
// dropped and rebuilt mid-frame (a style pass that stops matching, then one that matches again),
// and a rebuilt instance must adopt the bars the previous one left rather than parent a second
// pair beside them.
static Node *ScrollSystem_findIndicator(Node *owner, StringView cls) {
	for (auto &child : owner->getChildren()) {
		if (child->getType() != "scrollbar") {
			continue;
		}
		if (auto id = child->getComponent<NodeIdentity>()) {
			if (id->classes.find(cls) != id->classes.end()) {
				return child;
			}
		}
	}
	return nullptr;
}

static Node *ScrollSystem_makeIndicator(Node *owner, StringView cls) {
	if (auto existing = ScrollSystem_findIndicator(owner, cls)) {
		return existing;
	}

	// A Panel rather than a plain Layer: it already routes background-color, outline-* and
	// border-radius out of CSS, so the thumb is styled by ordinary rules and needs no properties of
	// its own. `scrollbar` is a real type selector, so `scrollbar { ... }` matches.
	auto node = Rc<Panel>::create();
	node->setType("scrollbar");
	node->addStyleClass("xl-ui-scrollbar");
	node->addStyleClass(cls);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setVisible(false);
	node->setOpacity(0.0f);
	node->setPathColor(Color4B(0, 0, 0, 90), false);
	node->setBorderRadius(ScrollSystem_indicatorThickness / 2.0f);
	// Never a flex/grid item, and never moved by the container's pass. styleManaged stays false, so
	// a style pass that matched nothing leaves it alone (see OutOfFlowComponent).
	//
	// Set BEFORE parenting, not after: Node::addChildNode re-runs the owner's measure and layout
	// straight away, so an indicator parented first would take part in that pass as an ordinary
	// flex item and eat the free space a `flex-grow` sibling was owed.
	node->setComponent<OutOfFlowComponent>(OutOfFlowComponent{false});
	return owner->addChild(node, ZOrder(maxOf<int16_t>()));
}

bool ScrollSystem::init(document::Overflow x, document::Overflow y) {
	if (!InputListener::init()) {
		return false;
	}

	_overflowX = x;
	_overflowY = y;
	_systemPriority = ScrollDefaultPriority;
	setSystemFlags(getSystemFlags() | SystemFlags::HandleLayoutChildren);

	addScrollRecognizer([this](const GestureScroll &s) { return handleScrollGesture(s); });
	addSwipeRecognizer([this](const GestureSwipe &s) { return handleSwipeGesture(s); },
			InputSwipeInfo{makeButtonMask(InputMouseButton::Touch), TapDistanceAllowed, true});
	return true;
}

void ScrollSystem::handleAdded(Node *owner) {
	InputListener::handleAdded(owner);

	// The bars themselves are built lazily, on the first pass that finds a range to show - not
	// here. handleAdded runs from inside the style pass, before the owner even has its
	// LayoutSystem, and parenting a child there re-enters measurement and layout with the container
	// half-configured.
	_indicatorV = ScrollSystem_findIndicator(owner, "xl-ui-scrollbar-vertical");
	_indicatorH = ScrollSystem_findIndicator(owner, "xl-ui-scrollbar-horizontal");

	scheduleUpdate();
}

void ScrollSystem::handleRemoved() {
	if (_scissor) {
		// Only ever disable it: the system may have been sitting on the node before us (a
		// VectorSprite ships with one), and removing someone else's clip is not ours to do.
		_scissor->disableScissor();
		_scissor = nullptr;
	}
	// Scan rather than trust the two pointers: an instance that was rebuilt mid-frame may have
	// adopted bars it did not create, and a bar left parented after its system is gone would be a
	// permanent stripe over the content.
	_indicatorV = nullptr;
	_indicatorH = nullptr;
	if (_owner) {
		Vector<Rc<Node>> bars;
		for (auto &child : _owner->getChildren()) {
			if (child->getType() == "scrollbar") {
				bars.emplace_back(child);
			}
		}
		for (auto &it : bars) { it->removeFromParent(); }
	}
	if (auto layout = _owner->getSystemByType<LayoutSystem>()) {
		layout->setOverflowAxes(false, false);
		layout->setScrollOffset(Vec2::ZERO);
	}

	InputListener::handleRemoved();
}

void ScrollSystem::handleTransformDirty(const Mat4 &parentTransform) {
	InputListener::handleTransformDirty(parentTransform);

	// Wheel and drag deltas are in screen units; a scroller inside a scaled subtree has to divide
	// by its own world scale or it moves at the wrong rate.
	Vec3 scale;
	parentTransform.decompose(&scale, nullptr, nullptr);
	const auto own = _owner->getScale();
	_worldScale = Vec2(scale.x * own.x, scale.y * own.y);
	if (_worldScale.x == 0.0f) {
		_worldScale.x = 1.0f;
	}
	if (_worldScale.y == 0.0f) {
		_worldScale.y = 1.0f;
	}
}

void ScrollSystem::setOverflow(document::Overflow x, document::Overflow y) {
	if (_overflowX == x && _overflowY == y) {
		return;
	}
	_overflowX = x;
	_overflowY = y;
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
}

void ScrollSystem::handleLayoutChildren() {
	InputListener::handleLayoutChildren();

	auto layout = _owner->getSystemByType<LayoutSystem>();
	if (layout) {
		// clips, not scrolls: `hidden` must lay the content out at its natural size too, or there
		// is nothing to clip - shrink would simply squash it into the box instead. Only the
		// SLIDING below is limited to `scroll`/`auto`.
		layout->setOverflowAxes(clipsX(), clipsY());
	}

	const Size2 box = _owner->getContentSize();
	// Without a LayoutSystem there is nothing that lays the content out at its natural size, so
	// there is nothing to scroll - this node clips and no more. That is the right answer for a
	// SystemManagedLayout widget (a dock, a TreeView): those scroll themselves.
	const Size2 content = layout ? layout->getContentExtent() : box;

	_range = Size2(scrollsX() ? sprt::max(content.width - box.width, 0.0f) : 0.0f,
			scrollsY() ? sprt::max(content.height - box.height, 0.0f) : 0.0f);

	const auto clamped = clampPosition(getScrollPosition());
	_scrollX = double(clamped.x);
	_scrollY = double(clamped.y);
	if (layout && layout->getScrollOffset() != clamped) {
		layout->setScrollOffset(clamped);
	}

	updateClip();
	updateIndicators();
}

Vec2 ScrollSystem::clampPosition(Vec2 value) const {
	return Vec2(math::clamp(value.x, 0.0f, _range.width),
			math::clamp(value.y, 0.0f, _range.height));
}

void ScrollSystem::commitOffset() {
	const auto pos = getScrollPosition();
	if (auto layout = _owner->getSystemByType<LayoutSystem>()) {
		layout->setScrollOffset(pos);
	}
	_indicatorIdle = 0.0f;
	updateIndicators();
	if (_scrollCallback) {
		_scrollCallback(pos);
	}
}

void ScrollSystem::setScrollPosition(Vec2 value) {
	// An explicit destination overrules a wheel still easing toward its own.
	_owner->stopAllActionsByTag(WheelActionTag);
	applyScrollPosition(value);
}

void ScrollSystem::applyScrollPosition(Vec2 value) {
	const auto prev = getScrollPosition();
	const auto next = clampPosition(value);
	if (next == prev) {
		return;
	}
	_scrollX = double(next.x);
	_scrollY = double(next.y);
	commitOffset();
}

void ScrollSystem::scrollBy(Vec2 delta) {
	// The delta path accumulates in double before clamping, so a long wheel session deep inside a
	// large content area does not drift (the same reason ui::TextViewContainer does it).
	const auto prev = getScrollPosition();
	_scrollX = math::clamp(_scrollX + double(delta.x), 0.0, double(_range.width));
	_scrollY = math::clamp(_scrollY + double(delta.y), 0.0, double(_range.height));
	if (getScrollPosition() == prev) {
		return;
	}
	commitOffset();
}

bool ScrollSystem::handleScrollGesture(const GestureScroll &s) {
	Vec2 delta(-s.amount.x * ScrollSystem_wheelStep / _worldScale.x,
			-s.amount.y * ScrollSystem_wheelStep / _worldScale.y);

	// A vertical wheel is redirected to the horizontal axis when this container can only scroll
	// horizontally - the browser rule, and the only thing that makes a horizontal-only strip (a tab
	// bar, a toolbar) usable with an ordinary mouse, which has no horizontal wheel. Shift asks for
	// the same thing explicitly.
	//
	// Both are FALLBACKS, guarded on delta.x being zero: a backend that already reports the shifted
	// or tilted wheel as an x amount must not have it swapped a second time.
	const bool horizontalOnly = _range.width > 0.0f && _range.height <= 0.0f;
	if (delta.x == 0.0f
			&& (horizontalOnly || hasFlag(s.input->data.input.modifiers, InputModifier::Shift))) {
		delta.x = delta.y;
		delta.y = 0.0f;
	}

	// Scroll chaining: claim only an axis this container can actually move on. Declining lets the
	// dispatcher - which walks the scene listeners topmost-first - offer the event to the ancestor
	// scroller, which is CSS's default `overscroll-behavior: auto` for free.
	//
	// Deliberately keyed on "has any range", NOT on "is not already at the edge". The latter is
	// what makes a nested list hand the wheel to its parent the moment it bottoms out, and browsers
	// had to invent scroll latching to undo it. Sticky beats jumpy.
	const bool canX = _range.width > 0.0f && delta.x != 0.0f;
	const bool canY = _range.height > 0.0f && delta.y != 0.0f;
	if (!canX && !canY) {
		return false;
	}

	_velocity = Vec2::ZERO; // a wheel notch cancels any fling in progress

	// Added to where the easing is HEADING, not to where it currently is. That is what makes N
	// notches in quick succession travel exactly N steps, the same total an un-animated wheel would
	// have covered - the animation changes when the content arrives, never how far it goes.
	scrollToAnimated(getScrollTarget() + Vec2(canX ? delta.x : 0.0f, canY ? delta.y : 0.0f));
	return true;
}

Vec2 ScrollSystem::getScrollTarget() const {
	if (_owner && _owner->getActionByTag(WheelActionTag)) {
		return _wheelTarget;
	}
	return getScrollPosition();
}

void ScrollSystem::scrollToAnimated(Vec2 target) {
	const Vec2 from = getScrollPosition();
	_wheelTarget = clampPosition(target);

	// Replace whatever is in flight: a notch arriving mid-easing restarts the curve from the
	// content's current position toward the new target, so the motion stays continuous.
	_owner->stopAllActionsByTag(WheelActionTag);

	if (_wheelTarget == from) {
		return;
	}

	const Vec2 to = _wheelTarget;
	_owner->runAction(Rc<ActionProgress>::create(ScrollSystem_wheelDuration,
							  [this, from, to](float p) { applyScrollPosition(from + (to - from) * p); }),
			WheelActionTag);
}

bool ScrollSystem::handleSwipeGesture(const GestureSwipe &s) {
	const bool canX = _range.width > 0.0f;
	const bool canY = _range.height > 0.0f;
	if (!canX && !canY) {
		return false;
	}

	// Inertia is a TOUCH idiom, and the source is the modifier, not the button: the button here is
	// always MouseLeft (`InputMouseButton::Touch` is an alias of it), while the backend marks a real
	// touchscreen with InputModifier::Touch per event. A mouse drag therefore stops dead on release,
	// which is the only thing that reads correctly under a pointer; a finger coasts.
	const bool fromTouch = hasFlag(s.input->data.input.modifiers, InputModifier::Touch);

	switch (s.event) {
	case GestureEvent::Began:
		// The hand wins over the wheel: whatever the easing was heading for is abandoned here.
		_velocity = Vec2::ZERO;
		_owner->stopAllActionsByTag(WheelActionTag);
		break;
	case GestureEvent::Activated:
		// The pointer drags the CONTENT, so the offset always runs OPPOSITE to the pointer, on both
		// axes: drag right and the content follows right, which is a smaller x offset; drag up and
		// it follows up, which is a larger y offset - because a positive y offset is exactly what
		// moves the content up (see LayoutSystem::setScrollOffset). No axis is inverted twice.
		scrollBy(Vec2(canX ? -s.delta.x / _worldScale.x : 0.0f,
				canY ? -s.delta.y / _worldScale.y : 0.0f));
		break;
	case GestureEvent::Ended:
		// Same sign rule as the drag itself: the content keeps going the way the finger was
		// pushing it, which is opposite to the pointer's own velocity.
		if (fromTouch) {
			_velocity = Vec2(canX ? -s.velocity.x / _worldScale.x : 0.0f,
					canY ? -s.velocity.y / _worldScale.y : 0.0f);
		}
		break;
	case GestureEvent::Cancelled: _velocity = Vec2::ZERO; break;
	}
	return true;
}

void ScrollSystem::update(const UpdateTime &time) {
	InputListener::update(time);

	const float dt = time.dt;
	if (dt <= 0.0f) {
		return;
	}

	if (_velocity != Vec2::ZERO) {
		const auto before = getScrollPosition();
		scrollBy(_velocity * dt);
		// Exponential decay, framerate-independent. Reaching a bound kills the fling outright: with
		// no rubber-band there is nothing left for the remaining energy to do.
		_velocity *= std::pow(ScrollSystem_flingDecay, dt);
		if (_velocity.length() < ScrollSystem_flingCutoff || getScrollPosition() == before) {
			_velocity = Vec2::ZERO;
		}
	}

	if (_indicatorMode == IndicatorMode::Auto) {
		_indicatorIdle += dt;
		updateIndicators();
	}
}

void ScrollSystem::updateClip() {
	const bool wantClip = clipsX() || clipsY();
	if (wantClip && !_scissor) {
		// Reuse an existing one rather than stacking a second: ui::Panel and every other
		// VectorSprite already ships with a DynamicStateSystem.
		_scissor = _owner->getSystemByType<DynamicStateSystem>();
		if (!_scissor) {
			// ApplyForAll, not ApplyForNodesBelow: "below" is the NEGATIVE-z-order set, and
			// ordinary children sit at ZOrder(0), i.e. "above" - they would go unclipped.
			_scissor = _owner->addSystem(
					Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
		}
	}
	if (_scissor) {
		if (wantClip) {
			_scissor->enableScissor();
		} else {
			_scissor->disableScissor();
		}
	}
}

void ScrollSystem::updateIndicators() {
	const Size2 box = _owner->getContentSize();

	// The owner may have rebuilt its children behind our back - removeAllChildren() is how a widget
	// like StudioTabBar refreshes itself - which drops the bars and leaves these pointers dangling.
	// Re-validate before touching them; place() then builds a fresh one if it still wants a bar.
	if (_indicatorV && _indicatorV->getParent() != _owner) {
		_indicatorV = nullptr;
	}
	if (_indicatorH && _indicatorH->getParent() != _owner) {
		_indicatorH = nullptr;
	}

	float target = 0.0f;
	switch (_indicatorMode) {
	case IndicatorMode::Never: target = 0.0f; break;
	case IndicatorMode::Always: target = 1.0f; break;
	case IndicatorMode::Auto:
		// `scroll` keeps the bar as long as there is a range; `auto` shows it while the content is
		// moving and fades it out once the user stops.
		if (_indicatorIdle <= ScrollSystem_indicatorHold) {
			target = 1.0f;
		} else {
			target = 1.0f
					- sprt::min((_indicatorIdle - ScrollSystem_indicatorHold)
									/ ScrollSystem_indicatorFade,
							1.0f);
		}
		break;
	}
	_indicatorOpacity = target;

	auto place = [&](Node *&node, bool horizontal) {
		const float range = horizontal ? _range.width : _range.height;
		const float extent = horizontal ? box.width : box.height;
		const bool wanted = range > 0.0f && _indicatorOpacity > 0.0f && extent > 0.0f;
		if (!wanted) {
			if (node) {
				node->setVisible(false);
			}
			return;
		}
		if (!node) {
			node = ScrollSystem_makeIndicator(_owner,
					horizontal ? StringView("xl-ui-scrollbar-horizontal")
							   : StringView("xl-ui-scrollbar-vertical"));
		}
		node->setVisible(true);
		node->setOpacity(_indicatorOpacity);

		// Thickness from CSS when the sheet declared one. The indicator sits in a flex container, so
		// `width`/`height` on it are routed into a MeasureComponent as an intrinsic-size INPUT
		// rather than committed (the ContentSize ownership rule) - and this system is the only
		// reader that input will ever have, because the layout skips an out-of-flow node.
		float thickness = ScrollSystem_indicatorThickness;
		if (auto m = node->getComponent<MeasureComponent>()) {
			const float declared = horizontal ? m->normal.height : m->normal.width;
			if (declared > 0.0f) {
				thickness = declared;
			}
		}

		// Thumb length is the visible fraction of the content, floored so it stays grabbable; its
		// travel is what is left of the track.
		const float content = extent + range;
		const float length =
				sprt::max(extent * extent / content, ScrollSystem_indicatorMinLength);
		const float travel = sprt::max(extent - length, 0.0f);
		const float progress = (horizontal ? float(_scrollX) : float(_scrollY)) / range;

		if (horizontal) {
			node->setContentSize(Size2(length, thickness));
			node->setPosition(Vec2(travel * progress, ScrollSystem_indicatorInset));
		} else {
			node->setContentSize(Size2(thickness, length));
			// progress runs top-down, the engine's y runs up
			node->setPosition(Vec2(box.width - thickness - ScrollSystem_indicatorInset,
					box.height - length - travel * progress));
		}
	};

	place(_indicatorV, false);
	place(_indicatorH, true);
}

void ScrollSystem::setIndicatorMode(IndicatorMode mode) {
	if (_indicatorMode == mode) {
		return;
	}
	_indicatorMode = mode;
	_indicatorIdle = 0.0f;
	updateIndicators();
}

void ScrollSystem::setScrollCallback(Function<void(Vec2)> &&cb) { _scrollCallback = sp::move(cb); }

bool ScrollSystem::scrollNodeIntoView(NotNull<Node> node, Padding pad) {
	bool isDescendant = false;
	for (auto p = node->getParent(); p; p = p->getParent()) {
		if (p == _owner) {
			isDescendant = true;
			break;
		}
	}
	if (!isDescendant) {
		return false;
	}
	if (_range.width <= 0.0f && _range.height <= 0.0f) {
		return true;
	}

	const auto size = node->getContentSize();
	const auto bottomLeft = _owner->convertToNodeSpace(node->convertToWorldSpace(Vec2::ZERO));
	const auto topRight =
			_owner->convertToNodeSpace(node->convertToWorldSpace(Vec2(size.width, size.height)));

	const Size2 box = _owner->getContentSize();

	// Move by the minimum that brings the box inside the scrollport; when the target is larger than
	// the port, the start edge wins (the head of a long item is the useful part).
	Vec2 delta;
	if (bottomLeft.x - pad.left < 0.0f) {
		delta.x = bottomLeft.x - pad.left;
	} else if (topRight.x + pad.right > box.width) {
		delta.x = sprt::min(topRight.x + pad.right - box.width, bottomLeft.x - pad.left);
	}
	// y-up geometry, y-down offset: overshooting the TOP edge means a negative offset delta
	if (topRight.y + pad.top > box.height) {
		delta.y = box.height - topRight.y - pad.top;
	} else if (bottomLeft.y - pad.bottom < 0.0f) {
		delta.y = sprt::max(-(bottomLeft.y - pad.bottom), box.height - topRight.y - pad.top);
	}

	if (delta != Vec2::ZERO) {
		_owner->stopAllActionsByTag(WheelActionTag);
		scrollBy(delta);
	}
	return true;
}

void scrollIntoView(NotNull<Node> node, Padding pad) {
	for (auto p = node->getParent(); p; p = p->getParent()) {
		if (auto s = p->getSystemByType<ScrollSystem>()) {
			s->scrollNodeIntoView(node, pad);
		}
	}
}

} // namespace stappler::xenolith::ui
