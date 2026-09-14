/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XL2dScrollView.h"

#include "XLInheritedStyle.h" // isInlineRtl: which side the vertical bar hangs from
#include "XLInteractiveComponent.h" // the hover bit a stylesheet reads as :hover
#include "XLInputDispatcher.h"
#include "director/XLDirector.h"
#include "XL2dLayerRounded.h"
#include "XLActionEase.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool ScrollView::Overscroll::init() {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	return true;
}

bool ScrollView::Overscroll::init(Direction dir) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	_direction = dir;
	return true;
}

void ScrollView::Overscroll::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();

	if (_contentSize == Size2::ZERO) {
		_image->clear();
	} else if (_image->getImageSize() != _contentSize) {
		auto image = Rc<VectorImage>::create(_contentSize);
		updateProgress(image);
		setImage(move(image));
	} else if (_progressDirty) {
		updateProgress(_image);
	}
}

void ScrollView::Overscroll::update(const UpdateTime &time) {
	VectorSprite::update(time);
	if (TimeInterval(time.global - _delayStart) > TimeInterval::microseconds(250'000)) {
		decrementProgress(time.dt);
	}
}

void ScrollView::Overscroll::handleEnter(Scene *scene) {
	VectorSprite::handleEnter(scene);
	scheduleUpdate();
}

void ScrollView::Overscroll::handleExit() {
	unscheduleUpdate();
	VectorSprite::handleExit();
}

void ScrollView::Overscroll::setDirection(Direction dir) {
	if (_direction != dir) {
		_direction = dir;
		_progressDirty = _contentSizeDirty = true;
	}
}

ScrollView::Overscroll::Direction ScrollView::Overscroll::getDirection() const {
	return _direction;
}

void ScrollView::Overscroll::setProgress(float p) {
	p = math::clamp(p, 0.0f, 1.0f);
	if (p != _progress) {
		_progress = p;
		_progressDirty = _contentSizeDirty = true;
	}
}

void ScrollView::Overscroll::incrementProgress(float dt) {
	setProgress(_progress + (dt * ((1.0 - _progress) * (1.0 - _progress))));
	_delayStart = Time::now().toMicros();
}

void ScrollView::Overscroll::decrementProgress(float dt) { setProgress(_progress - (dt * 2.5f)); }

void ScrollView::Overscroll::updateProgress(VectorImage *) { }

bool ScrollView::init(Layout l) {
	if (!ScrollViewBase::init(l)) {
		return false;
	}

	// The track is a separate node rather than an inflated thumb: touch padding grows a box on all
	// sides, so a grabbable thumb would swallow the track's click region. Same as ui::Slider.
	_indicatorTrack = addChild(Rc<LayerRounded>::create(Color4F(0.0f, 0.0f, 0.0f, 0.0f),
									   _indicatorThicknessActive / 2.0f),
			ZOrder(1));
	_indicatorTrack->setType("scroll-indicator-track");
	_indicatorTrack->setName("scroll-indicator-track");
	_indicatorTrack->setAnchorPoint(Vec2(1, 0));

	/* The track's opacity is its own paint: it stays at zero unless hovered, and the thumb is its
	child for placement only, so opacity must not cascade to it. Visibility still cascades. */
	_indicatorTrack->setCascadeOpacityEnabled(false);

	_indicator = _indicatorTrack->addChild(
			Rc<LayerRounded>::create(Color4F(1.0f, 1.0f, 1.0f, 0.0f), 2.0f), ZOrder(1));
	_indicator->setType("scroll-indicator");
	_indicator->setName("scroll-indicator");
	_indicator->setAnchorPoint(Vec2(1, 0));


	/* The listener goes on the track, which is served before the content's own:
	InputListenerStorage walks listeners backwards in visit order, and the ZOrder(1) track registers
	after the ScrollView. A Processed result does not stop the walk, so the Begin is swallowed and
	the swipe takes the pointer exclusively. */
	_indicatorListener = _indicatorTrack->addSystem(Rc<InputListener>::create());
	_indicatorListener->setSwallowEvent(InputEventName::Begin);

	/* The device subscription goes on the content's listener: the track's listener is disabled
	where no pointing device exists (so a transparent strip does not swallow touches), and a
	disabled listener would never learn a mouse was plugged in. */
	_inputListener->setWindowStateCallback([this](core::WindowState state, core::WindowState) {
		setIndicatorHasPointer(hasFlag(state, core::WindowState::InputPointer));
		return false;
	});

	_indicatorListener->addSwipeRecognizer(
			[this](const GestureSwipe &s) -> bool {
		switch (s.event) {
		// secondTouch, not midpoint: for a one-pointer swipe the midpoint lags one event behind the
		// pointer. Same as ui::Slider and ui::DockSplitter.
		case GestureEvent::Began: return handleIndicatorDragBegin(s.secondTouch);
		case GestureEvent::Activated: handleIndicatorDragMove(s.secondTouch); return true;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleIndicatorDragEnd(); return true;
		}
		return false;
	},
			// threshold 0 with sendThreshold: a handle follows the pointer from the first pixel.
			// Same settings as ui::Slider and ui::DockSplitter.
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				0.0f, true});

	/* Records the press location: a swipe's Began arrives after recognition, when the pointer has
	already moved, and grabbing from there would leave the thumb trailing the cursor. */
	_indicatorListener->addPressRecognizer([this](const GesturePress &press) -> bool {
		if (press.event == GestureEvent::Began) {
			_indicatorPress = press.pos;
		}
		return false;
	}, InputPressInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft})});

	_indicatorListener->addTapRecognizer([this](const GestureTap &tap) -> bool {
		if (tap.event == GestureEvent::Activated) {
			return handleIndicatorTap(tap.location());
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	// Hover goes through the InteractiveComponent, so the same state makes
	// `scroll-indicator-track:hover` match.
	_indicatorListener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: handleIndicatorHover(true); break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleIndicatorHover(false); break;
		default: break;
		}
		return true;
	}, false);

	_overflowFront = addChild(Rc<Overscroll>::create());
	_overflowBack = addChild(Rc<Overscroll>::create());

	setOverscrollColor(Color4F(0.5f, 0.5f, 0.5f, 1.0f));
	setOverscrollVisible(!_bounce);

	return true;
}

void ScrollView::handleContentSizeDirty() {
	ScrollViewBase::handleContentSizeDirty();
	if (isVertical()) {
		_overflowFront->setAnchorPoint(Vec2(0.0f, 1.0f)); // top
		_overflowFront->setDirection(Overscroll::Direction::Top);
		_overflowFront->setPosition(Vec2(0, _contentSize.height - _overscrollFrontOffset));
		_overflowFront->setContentSize(Size2(_contentSize.width,
				sprt::min(_contentSize.width * Overscroll::OverscrollScale,
						Overscroll::OverscrollMaxHeight)));

		_overflowBack->setAnchorPoint(Vec2(0.0f, 0.0f)); // bottom
		_overflowBack->setDirection(Overscroll::Direction::Bottom);
		_overflowBack->setPosition(Vec2(0, _overscrollBackOffset));
		_overflowBack->setContentSize(Size2(_contentSize.width,
				sprt::min(_contentSize.width * Overscroll::OverscrollScale,
						Overscroll::OverscrollMaxHeight)));

	} else {
		_overflowFront->setAnchorPoint(Vec2(0, 0)); // left
		_overflowFront->setDirection(Overscroll::Direction::Left);
		_overflowFront->setPosition(Vec2(_overscrollFrontOffset, 0));
		_overflowFront->setContentSize(
				Size2(sprt::min(_contentSize.height * Overscroll::OverscrollScale,
							  Overscroll::OverscrollMaxHeight),
						_contentSize.height));

		_overflowBack->setAnchorPoint(Vec2(1, 0)); // right
		_overflowBack->setDirection(Overscroll::Direction::Right);
		_overflowBack->setPosition(Vec2(_contentSize.width - _overscrollBackOffset, 0));
		_overflowBack->setContentSize(
				Size2(sprt::min(_contentSize.height * Overscroll::OverscrollScale,
							  Overscroll::OverscrollMaxHeight),
						_contentSize.height));
	}
	updateIndicatorPosition();
}

void ScrollView::handleLayoutChildren() {
	ScrollViewBase::handleLayoutChildren();
	// First point at which the resolved direction is certainly this frame's, so the bar's side is
	// decided here.
	updateIndicatorPosition();
}

void ScrollView::setOverscrollColor(const Color4F &val, bool withOpacity) {
	_overflowFront->setColor(val, withOpacity);
	_overflowBack->setColor(val, withOpacity);
}

Color4F ScrollView::getOverscrollColor() const { return _overflowFront->getColor(); }

void ScrollView::setOverscrollVisible(bool value) {
	_overflowFront->setVisible(value);
	_overflowBack->setVisible(value);
}

bool ScrollView::isOverscrollVisible() const { return _overflowFront->isVisible(); }

void ScrollView::setIndicatorColor(const Color4B &val, bool withOpacity) {
	// The node is replaceable, and only a LayerRounded is known to paint a path; a swapped-in node
	// takes its colour from a stylesheet.
	if (auto layer = dynamic_cast<LayerRounded *>(_indicator)) {
		layer->setPathColor(val, withOpacity);
	} else {
		_indicator->setColor(Color4F(val), withOpacity);
	}
}

Color4F ScrollView::getIndicatorColor() const {
	// Read back what setIndicatorColor wrote: the setter writes the path and leaves the node
	// alone, so the node's own colour would not round-trip.
	if (auto layer = dynamic_cast<LayerRounded *>(_indicator)) {
		return Color4F(layer->getPathColor());
	}
	return _indicator->getColor();
}

namespace {

// State of a bar node that belongs to the view rather than to the node: it is re-imposed on
// whatever node is swapped in.
static void ScrollView_adoptIndicatorNode(Node *from, Node *to, StringView name) {
	to->setType(name);
	to->setName(name);
	to->setAnchorPoint(from->getAnchorPoint());
	to->setPosition(from->getPosition());
	to->setContentSize(from->getContentSize());
	to->setVisible(from->isVisible());
	to->setOpacity(from->getOpacity());
	// classes, so the state the view publishes as `.active` is not lost with the node carrying it
	if (auto classes = from->getStyleClasses()) {
		for (auto &it : *classes) { to->addStyleClass(it); }
	}
}

} // namespace

void ScrollView::setIndicatorNode(Rc<Node> &&node) {
	if (!node || node == _indicator || !_indicator || !_indicatorTrack) {
		return;
	}

	// Read the colour through the accessor pair, which knows how each kind of node stores it, and
	// write it back the same way, so the swap does not change the colour
	const auto color = getIndicatorColor();

	auto old = _indicator;
	ScrollView_adoptIndicatorNode(old, node, "scroll-indicator");

	_indicator = _indicatorTrack->addChild(sp::move(node), ZOrder(1));
	old->removeFromParent(true);

	setIndicatorColor(Color4B(color), false);
	updateIndicatorPosition();
}

void ScrollView::setIndicatorTrackNode(Rc<Node> &&node) {
	if (!node || node == _indicatorTrack || !_indicatorTrack) {
		return;
	}

	auto old = _indicatorTrack;
	ScrollView_adoptIndicatorNode(old, node, "scroll-indicator-track");

	/* The thumb and the listener move across rather than being rebuilt: both belong to the view,
	and rebuilding would drop the gesture set and a running fade. `removeFromParent(false)` for
	the same reason - cleanup would stop those actions. */
	Rc<Node> thumb = _indicator;
	if (thumb && thumb->getParent() == old) {
		thumb->removeFromParent(false);
		node->addChild(thumb, ZOrder(1));
	}

	Rc<InputListener> listener = _indicatorListener;
	if (listener) {
		old->removeSystem(listener);
		node->addSystem(listener);
	}

	_indicatorTrack = addChild(sp::move(node), ZOrder(1));
	// as in init(): the track paints itself and must not paint through the thumb inside it
	_indicatorTrack->setCascadeOpacityEnabled(false);
	old->removeFromParent(true);

	// The hover state lives in a component on the track, so the new one starts without it;
	// re-impose the opacity for the view's actual state
	updateIndicatorInteractive();
}

void ScrollView::setIndicatorVisible(bool value) {
	_indicatorVisible = value;
	if (!sprt::isnan(getScrollLength())) {
		_indicator->setVisible(value);
	} else {
		_indicator->setVisible(false);
	}
}

bool ScrollView::isIndicatorVisible() const { return _indicatorVisible; }

void ScrollView::doSetScrollPosition(float pos) {
	ScrollViewBase::doSetScrollPosition(pos);
	updateIndicatorPosition();
}

void ScrollView::handleEnter(Scene *scene) {
	ScrollViewBase::handleEnter(scene);

	/* Seeded here, not only subscribed below: a WindowState event arrives on change only, so a view
	entering a running scene would otherwise assume no pointing device (as in
	GestureMouseOverRecognizer). */
	if (auto dir = getDirector()) {
		if (auto dispatcher = dir->getInputDispatcher()) {
			setIndicatorHasPointer(
					hasFlag(dispatcher->getWindowState(), core::WindowState::InputPointer));
		}
	}

	/* Applied even when nothing changed: setIndicatorHasPointer is a no-op without a change, but
	the track's listener starts enabled and must be switched off on a touch-only device. */
	updateIndicatorInteractive();
}

void ScrollView::setIndicatorHasPointer(bool value) {
	if (_indicatorHasPointer == value) {
		return;
	}
	_indicatorHasPointer = value;
	updateIndicatorInteractive();
}

bool ScrollView::isIndicatorFading() const {
	switch (_indicatorFade) {
	case IndicatorFade::Never: return false;
	case IndicatorFade::Always: return true;
	case IndicatorFade::Auto: break;
	}
	// Nothing to aim at, so the bar has nothing to wait for.
	return !_indicatorHasPointer;
}

bool ScrollView::isIndicatorInteractive() const {
	return _indicatorHasPointer && _indicator && _indicator->isVisible();
}

void ScrollView::setIndicatorFade(IndicatorFade value) {
	if (_indicatorFade == value) {
		return;
	}
	_indicatorFade = value;
	updateIndicatorInteractive();
}

void ScrollView::setIndicatorOpacity(float value) {
	_indicatorOpacity = sprt::clamp(value, 0.0f, 1.0f);
	updateIndicatorInteractive();
}

void ScrollView::updateIndicatorInteractive() {
	if (!_indicator || !_indicatorTrack) {
		return;
	}

	if (_indicatorListener) {
		_indicatorListener->setEnabled(isIndicatorInteractive());
	}

	// Thickness is written by the code below, so `.active` is how a stylesheet learns the bar
	// became something to aim at.
	for (auto node : {_indicatorTrack, _indicator}) {
		if (_indicatorHasPointer) {
			node->addStyleClass(IndicatorActiveClass);
		} else {
			node->removeStyleClass(IndicatorActiveClass);
		}
	}

	// Settle the bar for this state unless it is mid-animation, so a scroll crossing a device
	// change does not flicker.
	if (!isIndicatorFading() && !_indicator->getActionByTag(IndicatorShowActionTag)
			&& !_indicator->getActionByTag(IndicatorSettleActionTag)) {
		_indicator->setOpacity(_indicatorOpacity);
	}

	_indicatorTrack->setOpacity(
			(_indicatorHovered && isIndicatorInteractive()) ? _indicatorTrackOpacity : 0.0f);

	// Thickness is part of the placement, so lay the bar out again now rather than on the next
	// scroll.
	updateIndicatorPosition();
}

void ScrollView::handleIndicatorHover(bool value) {
	if (_indicatorHovered == value) {
		return;
	}
	_indicatorHovered = value;

	// Through the component, which a stylesheet reads as `:hover`. The opacity below is the default
	// for a caller with no stylesheet. The counters are cumulative, so the bit is pushed only on an
	// edge (guaranteed by the early return above).
	_indicatorTrack->setOrUpdateComponent<InteractiveComponent>(
			[value](NotNull<InteractiveComponent> state) {
		return state->handleHover(value ? 1 : -1);
	});
	updateIndicatorInteractive();
}

float ScrollView::getIndicatorTravel() const {
	if (!_indicator || !_indicatorTrack) {
		return 0.0f;
	}
	const auto track = _indicatorTrack->getContentSize();
	const auto thumb = _indicator->getContentSize();
	return sprt::max(isVertical() ? (track.height - thumb.height) : (track.width - thumb.width),
			0.0f);
}

float ScrollView::getIndicatorRelativeForLocation(const Vec2 &trackLocation, float grab) const {
	const float travel = getIndicatorTravel();
	if (travel <= 0.0f) {
		return getIndicatorRelativePosition();
	}

	// The exact inverse of the placement in updateIndicatorPosition: there the thumb's near edge is
	// put at `travel * (1 - value)` going up, and at `travel * value` going right.
	if (isVertical()) {
		return sprt::clamp(1.0f - (trackLocation.y - grab) / travel, 0.0f, 1.0f);
	}
	return sprt::clamp((trackLocation.x - grab) / travel, 0.0f, 1.0f);
}

bool ScrollView::handleIndicatorDragBegin(const Vec2 &location) {
	if (!isIndicatorInteractive()) {
		return false;
	}

	// Measured from where the press landed, not where the swipe was recognized; falls back to the
	// swipe's point when no press was seen (a synthesized drag).
	const auto grabAt = _indicatorPress.isValid() ? _indicatorPress : location;
	const auto local = _indicatorTrack->convertToNodeSpace(grabAt);
	const auto thumb = _indicator->getBoundingBox();

	// Grabbing the thumb keeps the point under the pointer; grabbing the track elsewhere centres
	// the thumb on it, which is what a click on empty track should do while the pointer is down.
	if (thumb.containsPoint(local)) {
		_indicatorGrab = isVertical() ? (local.y - thumb.origin.y) : (local.x - thumb.origin.x);
	} else {
		_indicatorGrab = (isVertical() ? thumb.size.height : thumb.size.width) / 2.0f;
	}

	// Manual, with running animations dropped: a fling would keep moving the root under the thumb,
	// and fixPosition() clamps an overshoot only when nothing else owns the scroll.
	onSwipeBegin();

	_indicatorDragging = true;
	_indicatorListener->setExclusive();

	setIndicatorRelativePosition(getIndicatorRelativeForLocation(local, _indicatorGrab));
	return true;
}

void ScrollView::handleIndicatorDragMove(const Vec2 &location) {
	if (!_indicatorDragging) {
		return;
	}
	setIndicatorRelativePosition(getIndicatorRelativeForLocation(
			_indicatorTrack->convertToNodeSpace(location), _indicatorGrab));
}

void ScrollView::handleIndicatorDragEnd() {
	if (!_indicatorDragging) {
		return;
	}
	_indicatorDragging = false;
	_indicatorPress = Vec2::INVALID;

	// Hand the scroll back: while _movement stays Manual nothing else may move it, and fixPosition
	// is gated on None.
	_movement = Movement::None;
	fixPosition();
}

bool ScrollView::handleIndicatorTap(const Vec2 &location) {
	if (!isIndicatorInteractive()) {
		return false;
	}

	// A tap that landed on the thumb is not a jump: the user aimed at what is already there.
	const auto local = _indicatorTrack->convertToNodeSpace(location);
	if (_indicator->getBoundingBox().containsPoint(local)) {
		return true;
	}

	const auto thumb = _indicator->getContentSize();
	setIndicatorRelativePosition(getIndicatorRelativeForLocation(local,
			(isVertical() ? thumb.height : thumb.width) / 2.0f));
	return true;
}

void ScrollView::updateScrollBounds() {
	ScrollViewBase::updateScrollBounds();
	updateIndicatorPosition();
}

void ScrollView::onOverscroll(float delta) {
	ScrollViewBase::onOverscroll(delta);
	if (isOverscrollVisible()) {
		if (delta > 0.0f) {
			_overflowBack->incrementProgress(delta / 50.0f);
		} else {
			_overflowFront->incrementProgress(-delta / 50.0f);
		}
	}
}

void ScrollView::onScroll(float delta, bool finished) {
	ScrollViewBase::onScroll(delta, finished);
	if (!finished) {
		updateIndicatorPosition();
	}
}

void ScrollView::onTap(int count, Vec2 loc) {
	if (_tapCallback) {
		_tapCallback(count, loc);
	}
}

void ScrollView::onAnimationFinished() {
	ScrollViewBase::onAnimationFinished();
	if (_animationCallback) {
		_animationCallback();
	}
	updateIndicatorPosition();
}

void ScrollView::updateIndicatorPosition() {
	if (!_indicatorVisible) {
		return;
	}

	const float scrollWidth = _contentSize.width;
	const float scrollHeight = _contentSize.height;
	const float scrollLength = getScrollLength();

	updateIndicatorPosition(_indicator, (isVertical() ? scrollHeight : scrollWidth) / scrollLength,
			getIndicatorRelativePosition(), true, _indicatorMinLength);
}

float ScrollView::getIndicatorThickness() const {
	return _indicatorHasPointer ? _indicatorThicknessActive : _indicatorThicknessIdle;
}

float ScrollView::getIndicatorReservedSize() const {
	// Same test updateIndicatorPosition() uses: content that fits has no bar and reserves no strip.
	// A nan length (no range committed yet) fails the comparison and reserves.
	if (getScrollLength() <= _scrollSize) {
		return 0.0f;
	}
	return _indicatorInset + sprt::max(_indicatorThicknessIdle, _indicatorThicknessActive);
}

void ScrollView::setIndicatorThickness(float idle, float active) {
	_indicatorThicknessIdle = idle;
	_indicatorThicknessActive = active;
	updateIndicatorPosition();
}

float ScrollView::getIndicatorRelativePosition() const {
	const float min = getScrollMinPosition();
	const float max = getScrollMaxPosition();

	// NaN until a controller commits a range, and min == max when content fits: answer 0. Guarded
	// here because setIndicatorRelativePosition is the exact inverse and must agree on these cases.
	if (sprt::isnan(min) || sprt::isnan(max) || max <= min) {
		return 0.0f;
	}
	// getScrollPosition(), not _scrollPosition: the cache settles a frame later, while this runs
	// from inside doSetScrollPosition after the root has moved.
	return sprt::clamp((getScrollPosition() - min) / (max - min), 0.0f, 1.0f);
}

void ScrollView::setIndicatorRelativePosition(float value) {
	const float min = getScrollMinPosition();
	const float max = getScrollMaxPosition();
	if (sprt::isnan(min) || sprt::isnan(max) || max <= min) {
		return;
	}

	// Not setScrollRelativePosition: it maps through the scrollable area and padding, which differs
	// from the min/max the thumb was placed by, so a drag would slide off the pointer.
	doSetScrollPosition(min + sprt::clamp(value, 0.0f, 1.0f) * (max - min));
}

void ScrollView::updateIndicatorPosition(Node *indicator, float size, float value, bool actions,
		float min) {
	if (!_indicatorVisible) {
		return;
	}

	float scrollWidth = _contentSize.width;
	float scrollHeight = _contentSize.height;

	float scrollLength = getScrollLength();
	if (sprt::isnan(scrollLength)) {
		indicator->setVisible(false);
	} else {
		indicator->setVisible(_indicatorVisible);
	}

	auto paddingLocal = _paddingGlobal;
	if (_indicatorIgnorePadding) {
		if (isVertical()) {
			paddingLocal.top = 0;
			paddingLocal.bottom = 0;
		} else {
			paddingLocal.left = 0;
			paddingLocal.right = 0;
		}
	}

	if (scrollLength > _scrollSize) {
		const float thickness = getIndicatorThickness();
		const float inset = _indicatorInset;

		if (isVertical()) {
			const float track =
					scrollHeight - inset * 2.0f - paddingLocal.top - paddingLocal.bottom;
			float h = track * size;
			if (h < min) {
				h = min;
			}
			const float r = track - h;

			// The track spans the whole run and the thumb is placed inside it; getIndicatorTravel()
			// reads the same `r` back out of these nodes.
			/* The bar sits at the inline end of the view, so it changes sides with the interface
			direction; track and thumb anchors flip together so getIndicatorTravel stays the exact
			inverse. */
			const bool rtl = isInlineRtl(this);
			_indicatorTrack->setContentSize(Size2(thickness, track));
			_indicatorTrack->setPosition(
					Vec2(rtl ? inset : scrollWidth - inset, paddingLocal.bottom + inset));

			indicator->setContentSize(Size2(thickness, h));
			indicator->setPosition(Vec2(rtl ? 0.0f : thickness, r * (1.0f - value)));
			indicator->setAnchorPoint(Vec2(rtl ? 0.0f : 1.0f, 0.0f));
		} else {
			const float track = scrollWidth - inset * 2.0f - paddingLocal.left - paddingLocal.right;
			float h = track * size;
			if (h < min) {
				h = min;
			}
			const float r = track - h;

			_indicatorTrack->setContentSize(Size2(track, thickness));
			_indicatorTrack->setPosition(Vec2(paddingLocal.left + inset, inset));
			_indicatorTrack->setAnchorPoint(Vec2(0, 0));

			indicator->setContentSize(Size2(h, thickness));
			indicator->setPosition(Vec2(r * value, 0.0f));
			indicator->setAnchorPoint(Vec2(0, 0));
		}
		_indicatorTrack->setVisible(_indicatorVisible);
		if (actions) {
			// Pulse to full on motion, then settle: to nothing when there is nothing to aim at,
			// otherwise to the resting opacity (see isIndicatorFading()).
			const float resting = isIndicatorFading() ? 0.0f : _indicatorOpacity;

			if (indicator->getOpacity() != 1.0f) {
				if (!indicator->getActionByTag(IndicatorShowActionTag)) {
					indicator->runAction(
							Rc<FadeTo>::create(progress(0.1f, 0.0f, indicator->getOpacity()), 1.0f),
							IndicatorShowActionTag);
				}
			}

			indicator->stopActionByTag(IndicatorSettleActionTag);
			indicator->runAction(Rc<Sequence>::create(2.0f, Rc<FadeTo>::create(0.25f, resting)),
					IndicatorSettleActionTag);
		}
	} else {
		indicator->setVisible(false);
		if (_indicatorTrack) {
			_indicatorTrack->setVisible(false);
		}
	}
}

void ScrollView::setPadding(const Padding &p) {
	if (p != _paddingGlobal) {
		float offset = (isVertical() ? _paddingGlobal.top : _paddingGlobal.left);
		float newOffset = (isVertical() ? p.top : p.left);
		ScrollViewBase::setPadding(p);

		if (offset != newOffset) {
			setScrollPosition(getScrollPosition() + (offset - newOffset));
		}
	}
}

void ScrollView::setOverscrollFrontOffset(float value) {
	if (_overscrollFrontOffset != value) {
		_overscrollFrontOffset = value;
		_contentSizeDirty = true;
	}
}
float ScrollView::getOverscrollFrontOffset() const { return _overscrollFrontOffset; }

void ScrollView::setOverscrollBackOffset(float value) {
	if (_overscrollBackOffset != value) {
		_overscrollBackOffset = value;
		_contentSizeDirty = true;
	}
}
float ScrollView::getOverscrollBackOffset() const { return _overscrollBackOffset; }

void ScrollView::setIndicatorIgnorePadding(bool value) {
	if (_indicatorIgnorePadding != value) {
		_indicatorIgnorePadding = value;
	}
}
bool ScrollView::isIndicatorIgnorePadding() const { return _indicatorIgnorePadding; }

void ScrollView::setTapCallback(const TapCallback &cb) { _tapCallback = cb; }

const ScrollView::TapCallback &ScrollView::getTapCallback() const { return _tapCallback; }

void ScrollView::setAnimationCallback(const AnimationCallback &cb) { _animationCallback = cb; }

const ScrollView::AnimationCallback &ScrollView::getAnimationCallback() const {
	return _animationCallback;
}

void ScrollView::update(const UpdateTime &time) {
	auto newpos = getScrollPosition();
	auto factor = sprt::min(64.0f, _adjustValue);

	switch (_adjust) {
	case Adjust::Front: newpos += (45.0f + progress(0.0f, 200.0f, factor / 32.0f)) * time.dt; break;
	case Adjust::Back: newpos -= (45.0f + progress(0.0f, 200.0f, factor / 32.0f)) * time.dt; break;
	default: break;
	}

	if (newpos != getScrollPosition()) {
		if (newpos < getScrollMinPosition()) {
			newpos = getScrollMinPosition();
		} else if (newpos > getScrollMaxPosition()) {
			newpos = getScrollMaxPosition();
		}
		_root->stopAllActionsByTag("ScrollViewAdjust"_tag);
		setScrollPosition(newpos);
	}
}

void ScrollView::runAdjustPosition(float newPos, float factor) {
	if (!sprt::isnan(newPos)) {
		if (newPos < getScrollMinPosition()) {
			newPos = getScrollMinPosition();
		} else if (newPos > getScrollMaxPosition()) {
			newPos = getScrollMaxPosition();
		}
		if (_adjustValue != newPos) {
			_adjustValue = newPos;
			auto dist = fabsf(newPos - getScrollPosition());

			auto t = 0.15f;
			if (dist < 20.0f) {
				t = 0.15f;
			} else if (dist > 220.0f) {
				t = 0.45f;
			} else {
				t = progress(0.15f, 0.45f, (dist - 20.0f) / 200.0f);
			}
			_root->stopAllActionsByTag("ScrollViewAdjust"_tag);
			auto a = Rc<Sequence>::create(
					Rc<EaseActionTyped>::create(
							Rc<MoveTo>::create(t,
									isVertical()
											? Vec2(_root->getPosition().x, newPos + _scrollSize)
											: Vec2(-newPos, _root->getPosition().y)),
							EaseActionTyped::Type::QuadEaseInOut),
					[this] { _adjustValue = nan(); });
			_root->runAction(a, "ScrollViewAdjust"_tag);
		}
	}
}
void ScrollView::runAdjust(float pos, float factor) {
	auto scrollPos = getScrollPosition();
	auto scrollSize = getScrollSize();

	float newPos = nan();
	if (scrollSize < 64.0f + 48.0f) {
		newPos = ((pos - 64.0f) + (pos - scrollSize + 48.0f)) / 2.0f;
	} else if (pos < scrollPos + 64.0f) {
		newPos = pos - 64.0f;
	} else if (pos > scrollPos + scrollSize - 48.0f) {
		newPos = pos - scrollSize + 48.0f;
	}

	runAdjustPosition(newPos, factor);
}

void ScrollView::scheduleAdjust(Adjust a, float val) {
	_adjustValue = val;
	if (a != _adjust) {
		_adjust = a;
		switch (_adjust) {
		case Adjust::None:
			unscheduleUpdate();
			_adjustValue = nan();
			break;
		default: scheduleUpdate(); break;
		}
	}
}

Value ScrollView::save() const {
	Value ret;
	ret.setDouble(getScrollRelativePosition(), "value");
	return ret;
}

void ScrollView::load(const Value &d) {
	if (d.isDictionary()) {
		_savedRelativePosition = d.getDouble("value");
		if (_controller) {
			_controller->onScrollPosition(true);
		}
	}
}

ScrollController::Item *ScrollView::getItemForNode(Node *node) const {
	auto &items = _controller->getItems();
	for (auto &it : items) {
		if (it.node && it.node == node) {
			return &it;
		}
	}
	return nullptr;
}

Rc<ActionProgress> ScrollView::resizeNode(Node *node, float newSize, float duration,
		Function<void()> &&cb) {
	return resizeNode(getItemForNode(node), newSize, duration, sp::move(cb));
}

Rc<ActionProgress> ScrollView::resizeNode(ScrollController::Item *item, float newSize,
		float duration, Function<void()> &&cb) {
	if (!item) {
		return nullptr;
	}

	auto &items = _controller->getItems();

	float sourceSize = isVertical() ? item->size.height : item->size.width;
	float tergetSize = newSize;

	struct ItemRects {
		float startPos;
		float startSize;
		float targetPos;
		float targetSize;
		ScrollController::Item *item;
	};

	Vector<ItemRects> vec;

	float offset = 0.0f;
	for (auto &it : items) {
		if (it.node && &it == item) {
			offset += sourceSize - tergetSize;
			vec.emplace_back(ItemRects{getNodeScrollPosition(it.pos), getNodeScrollSize(it.size),
				getNodeScrollPosition(it.pos), tergetSize, &it});
		} else if (offset != 0.0f) {
			vec.emplace_back(ItemRects{getNodeScrollPosition(it.pos), getNodeScrollSize(it.size),
				getNodeScrollPosition(it.pos) - offset, getNodeScrollSize(it.size), &it});
		}
	}

	auto ret = Rc<ActionProgress>::create(duration, [this, vec](float p) {
		for (auto &it : vec) {
			if (isVertical()) {
				it.item->pos.y = progress(it.startPos, it.targetPos, p);
				it.item->size.height = progress(it.startSize, it.targetSize, p);
			} else {
				it.item->pos.x = progress(it.startPos, it.targetPos, p);
				it.item->size.width = progress(it.startSize, it.targetSize, p);
			}
			if (it.item->node) {
				updateScrollNode(it.item->node, it.item->pos, it.item->size, it.item->zIndex,
						it.item->name);
			}
		}
		_controller->onScrollPosition(true);
	}, []() {

	}, [cb = sp::move(cb)]() {
		if (cb) {
			cb();
		}
	});
	return ret;
}

Rc<ActionProgress> ScrollView::removeNode(Node *node, float duration, Function<void()> &&cb,
		bool disable) {
	return removeNode(getItemForNode(node), duration, sp::move(cb), disable);
}

Rc<ActionProgress> ScrollView::removeNode(ScrollController::Item *item, float duration,
		Function<void()> &&cb, bool disable) {
	return resizeNode(item, 0.0f, duration, [item, cb = sp::move(cb), disable] {
		if (item->node) {
			if (item->node->isRunning()) {
				item->node->removeFromParent();
			}
			item->node = nullptr;
			item->handle = nullptr;
			if (disable) {
				item->nodeFunction = nullptr;
			}
		}
		if (cb) {
			cb();
		}
	});
}

} // namespace stappler::xenolith::basic2d
