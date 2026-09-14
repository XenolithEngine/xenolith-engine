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

#include "XLInheritedStyle.h" // isInlineRtl: which side the vertical bar sits on
#include "XLUiPanel.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// How far one wheel notch scrolls, in points. The wheel reports notches, not distance.
static constexpr float ScrollSystem_wheelStep = 48.0f;

// Two wheel events closer than this belong to one gesture: the second and later events of a
// burst keep the classification the first one got (see handleScrollGesture).
static constexpr uint64_t ScrollSystem_wheelBurstUs = 80'000ULL;

// How long a notch takes to ease in.
static constexpr float ScrollSystem_wheelDuration = 0.1f;

// Overlay indicator geometry. The thumb floats over the content and reserves no space.
static constexpr float ScrollSystem_indicatorThickness = 6.0f;
static constexpr float ScrollSystem_indicatorMinLength = 24.0f;
static constexpr float ScrollSystem_indicatorInset = 2.0f;

// Fling: the velocity is scaled by this factor per second, and dropped once it is this slow.
static constexpr float ScrollSystem_flingDecay = 0.002f;
static constexpr float ScrollSystem_flingCutoff = 8.0f;

// `auto` indicators hold for this long after the last movement, then fade over this long.
static constexpr float ScrollSystem_indicatorHold = 0.9f;
static constexpr float ScrollSystem_indicatorFade = 0.25f;

/* The clip uses its own system type, not the DynamicStateSystem a VectorSprite already has: that
one is `DoNotApply` and rewritten by the sprite for its own crop. The distinct type lets a rebuilt
ScrollSystem adopt the previous clip; stacked DynamicStateSystems intersect correctly. */
class ScrollSystem_ClipState : public DynamicStateSystem {
public:
	virtual ~ScrollSystem_ClipState() = default;
};

// An indicator already parented to `owner` and carrying `cls`, or null. A ScrollSystem rebuilt
// mid-frame by a style pass adopts the bars the previous instance left.
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

	// A Panel, so the thumb is styled by ordinary CSS rules; `scrollbar { ... }` matches the type.
	auto node = Rc<Panel>::create();
	node->setType("scrollbar");
	node->addStyleClass("xl-ui-scrollbar");
	node->addStyleClass(cls);
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setVisible(false);
	node->setOpacity(0.0f);
	node->setPathColor(Color4B(0, 0, 0, 90), false);
	node->setBorderRadius(ScrollSystem_indicatorThickness / 2.0f);
	// Out of flow, not style-managed. Set before parenting: Node::addChildNode re-runs the owner's
	// layout at once, and the bar would otherwise take free space as a flex item.
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

	// Bars are built lazily on the first pass with a range: handleAdded runs inside the style pass,
	// and parenting a child here would re-enter layout with the container half-configured.
	_indicatorV = ScrollSystem_findIndicator(owner, "xl-ui-scrollbar-vertical");
	_indicatorH = ScrollSystem_findIndicator(owner, "xl-ui-scrollbar-horizontal");

	scheduleUpdate();
}

void ScrollSystem::handleRemoved() {
	if (_scissor) {
		// Disabled, not removed (see the class note); a rebuilt instance adopts it by type.
		_scissor->disableScissor();
		_scissor = nullptr;
	}
	// Scan rather than trust the two pointers: a rebuilt instance may have adopted bars it did not
	// create, and a leftover bar would stay over the content.
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

	// Wheel and drag deltas are in screen units; divided by the world scale.
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
		// clips, not scrolls: `hidden` also needs natural-size layout, or shrink squashes the
		// content; only sliding is limited to `scroll`/`auto`.
		layout->setOverflowAxes(clipsX(), clipsY());
	}

	const Size2 box = _owner->getContentSize();
	// Without a LayoutSystem the node only clips; SystemManagedLayout widgets scroll themselves.
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
	// Accumulates in double before clamping, to avoid drift.
	const auto prev = getScrollPosition();
	_scrollX = math::clamp(_scrollX + double(delta.x), 0.0, double(_range.width));
	_scrollY = math::clamp(_scrollY + double(delta.y), 0.0, double(_range.height));
	if (getScrollPosition() == prev) {
		return;
	}
	commitOffset();
}

// Discrete backends (xcb, Windows, macOS line-mode) emit ±1 or ±N*InputScrollNotch per detent;
// precise ones (macOS trackpad, wasm Chrome) emit pixel distances, often many per frame, which must
// be applied as distance, not eased as notches.
static bool ScrollSystem_isNotchComponent(float v) {
	const float a = std::fabs(v);
	if (a < 1.0e-4f) {
		return false;
	}
	if (std::fabs(a - 1.0f) < 1.0e-3f) {
		return true;
	}
	const float notches = a / sprt::window::InputScrollNotch;
	const float nearest = std::round(notches);
	return nearest >= 1.0f && nearest <= 3.0f && std::fabs(notches - nearest) < 1.0e-3f;
}

static float ScrollSystem_notchDelta(float amount, float scale) {
	if (amount == 0.0f) {
		return 0.0f;
	}
	const float a = std::fabs(amount);
	const float notches = (std::fabs(a - 1.0f) < 1.0e-3f)
			? amount
			: amount / sprt::window::InputScrollNotch;
	return -notches * ScrollSystem_wheelStep / scale;
}

bool ScrollSystem::handleScrollGesture(const GestureScroll &s) {
	if (!_owner) {
		return false;
	}
	// Classify by the shape of the amount, not by timing (a fast wheel spin is also a burst). The
	// burst only disambiguates a pixel stream whose first event lands on a notch value.
	const bool notchShaped =
			ScrollSystem_isNotchComponent(s.amount.x) || ScrollSystem_isNotchComponent(s.amount.y);
	const auto now = Time::now();
	const bool inBurst = _lastWheelTime.toMicros() != 0
			&& (now - _lastWheelTime).toMicros() < ScrollSystem_wheelBurstUs;
	const bool discrete = notchShaped && (!inBurst || _lastWheelDiscrete);
	_lastWheelTime = now;
	_lastWheelDiscrete = discrete;

	Vec2 delta;
	if (discrete) {
		delta = Vec2(ScrollSystem_notchDelta(s.amount.x, _worldScale.x),
				ScrollSystem_notchDelta(s.amount.y, _worldScale.y));
	} else {
		// Amount is already a distance in points (CSS pixels on wasm, scrollingDelta on
		// macOS). Do not fold display density in: pointer coords are backing pixels and
		// need /worldScale, wheel pixels are not.
		delta = Vec2(-s.amount.x, -s.amount.y);
		const auto own = _owner->getScale();
		if (own.x != 0.0f && own.x != 1.0f) {
			delta.x /= own.x;
		}
		if (own.y != 0.0f && own.y != 1.0f) {
			delta.y /= own.y;
		}
	}

	// A vertical wheel goes to the horizontal axis on a horizontal-only container, or with Shift
	// (browser rule). Only when delta.x is zero, so a backend's own shifted amount is not swapped.
	const bool horizontalOnly = _range.width > 0.0f && _range.height <= 0.0f;
	if (delta.x == 0.0f
			&& (horizontalOnly || hasFlag(s.input->data.input.modifiers, InputModifier::Shift))) {
		delta.x = delta.y;
		delta.y = 0.0f;
	}

	// Scroll chaining: decline an axis with no range, so the dispatcher offers the event to the
	// ancestor scroller. Keyed on "has any range", not "not at the edge": a nested list at its end
	// keeps the wheel instead of handing it to the parent.
	const bool canX = _range.width > 0.0f && delta.x != 0.0f;
	const bool canY = _range.height > 0.0f && delta.y != 0.0f;
	if (!canX && !canY) {
		return false;
	}

	_velocity = Vec2::ZERO; // a wheel notch cancels any fling in progress

	const Vec2 step(canX ? delta.x : 0.0f, canY ? delta.y : 0.0f);
	if (discrete) {
		// Added to where the easing is heading, so N quick notches travel exactly N steps.
		scrollToAnimated(getScrollTarget() + step);
	} else {
		// Precise streams are applied directly; easing each event would stutter.
		_owner->stopAllActionsByTag(WheelActionTag);
		scrollBy(step);
	}
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
	_owner->runAction(
			Rc<ActionProgress>::create(ScrollSystem_wheelDuration,
					[this, from, to](float p) { applyScrollPosition(from + (to - from) * p); }),
			WheelActionTag);
}

bool ScrollSystem::handleSwipeGesture(const GestureSwipe &s) {
	const bool canX = _range.width > 0.0f;
	const bool canY = _range.height > 0.0f;
	if (!canX && !canY) {
		return false;
	}

	// Inertia only for touch, detected by InputModifier::Touch: the button is always MouseLeft
	// (`InputMouseButton::Touch` is an alias). A mouse drag stops on release.
	const bool fromTouch = hasFlag(s.input->data.input.modifiers, InputModifier::Touch);

	switch (s.event) {
	case GestureEvent::Began:
		// The hand wins over the wheel: whatever the easing was heading for is abandoned here.
		_velocity = Vec2::ZERO;
		_owner->stopAllActionsByTag(WheelActionTag);
		break;
	case GestureEvent::Activated:
		// The pointer drags the content, so the offset runs opposite to the pointer on both axes
		// (a positive y offset moves content up, see LayoutSystem::setScrollOffset).
		scrollBy(Vec2(canX ? -s.delta.x / _worldScale.x : 0.0f,
				canY ? -s.delta.y / _worldScale.y : 0.0f));
		break;
	case GestureEvent::Ended:
		// Same sign rule as the drag: opposite to the pointer's velocity.
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
		// Exponential decay, framerate-independent. Reaching a bound stops the fling.
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
		// Ours, never the sprite's (see ScrollSystem_ClipState); maybe left by a previous instance.
		_scissor = _owner->getSystemByType<ScrollSystem_ClipState>();
		if (!_scissor) {
			// ApplyForAll: ApplyForNodesBelow covers only negative z-order, not ordinary children.
			_scissor = _owner->addSystem(
					Rc<ScrollSystem_ClipState>::create(DynamicStateApplyMode::ApplyForAll));
		}
	}
	if (_scissor) {
		if (wantClip) {
			// Only the axes that clip; an axis left out of the mask is opened, not narrowed to the
			// box (see ScissorAxes).
			auto axes = ScissorAxes::None;
			if (clipsX()) {
				axes |= ScissorAxes::Horizontal;
			}
			if (clipsY()) {
				axes |= ScissorAxes::Vertical;
			}
			_scissor->enableScissor(_scissor->getScissorOutline(), axes);
		} else {
			_scissor->disableScissor();
		}
	}
}

void ScrollSystem::updateIndicators() {
	const Size2 box = _owner->getContentSize();

	// The owner may have removed its children (removeAllChildren() on refresh), dropping the bars;
	// re-validate, and place() builds fresh ones if still wanted.
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

	const bool rtl = isInlineRtl(_owner);

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

		// Thickness from CSS when declared: `width`/`height` inside a flex container land in a
		// MeasureComponent, which the layout skips for this out-of-flow node, so read it here.
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
		const float length = sprt::max(extent * extent / content, ScrollSystem_indicatorMinLength);
		const float travel = sprt::max(extent - length, 0.0f);
		const float progress = (horizontal ? float(_scrollX) : float(_scrollY)) / range;

		if (horizontal) {
			node->setContentSize(Size2(length, thickness));
			node->setPosition(Vec2(travel * progress, ScrollSystem_indicatorInset));
		} else {
			node->setContentSize(Size2(thickness, length));
			// The bar sits at the inline end (left in RTL); the scroll origin stays physical.
			const float x = rtl ? ScrollSystem_indicatorInset
								: box.width - thickness - ScrollSystem_indicatorInset;
			// progress runs top-down, the engine's y runs up
			node->setPosition(Vec2(x, box.height - length - travel * progress));
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
	// y-up geometry, y-down offset: overshooting the top edge means a negative offset delta
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
