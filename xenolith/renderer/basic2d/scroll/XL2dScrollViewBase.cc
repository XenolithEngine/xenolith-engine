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

#include "XL2dScrollViewBase.h"
#include "XLInputListener.h"
#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool ScrollViewBase::init(Layout layout) {
	if (!Node::init()) {
		return false;
	}

	_layout = layout;

	_inputListener = addSystem(Rc<InputListener>::create());
	_inputListener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			onTap(tap.count, tap.pos);
		}
		return false;
	});

	_inputListener->addPressRecognizer(
			[this](const GesturePress &s) -> bool {
		switch (s.event) {
		case GestureEvent::Began: return onPressBegin(s.pos); break;
		case GestureEvent::Activated: return onLongPress(s.pos, s.time, s.tickCount); break;
		case GestureEvent::Ended: return onPressEnd(s.pos, s.time); break;
		case GestureEvent::Cancelled: return onPressCancel(s.pos, s.time); break;
		}
		return false;
	},
			InputPressInfo{makeButtonMask(InputMouseButton::Touch), TimeInterval::milliseconds(425),
				InputPressFlags::Continuous});

	_inputListener->addSwipeRecognizer([this](const GestureSwipe &s) -> bool {
		switch (s.event) {
		case GestureEvent::Began:
			return onSwipeEventBegin(s.getId(), s.midpoint, s.delta, s.velocity);
			break;
		case GestureEvent::Activated:
			return onSwipeEvent(s.getId(), s.midpoint, s.delta, s.velocity);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			return onSwipeEventEnd(s.getId(), s.midpoint, s.delta, s.velocity);
			break;
		}
		return false;
	}, InputSwipeInfo{makeButtonMask(InputMouseButton::Touch), TapDistanceAllowed, true});

	_inputListener->addScrollRecognizer([this](const GestureScroll &w) -> bool {
		auto pos = getScrollPosition();
		onSwipeBegin();
		if (_layout == Vertical) {
			onDelta(-w.amount.y * 5.0f / _globalScale.y);
		} else {
			onDelta(-w.amount.x * 5.0f / _globalScale.x);
		}
		onScroll(getScrollPosition() - pos, false);
		return true;
	});

	setCascadeOpacityEnabled(true);

	_root = addChild(Rc<Node>::create());
	_root->setPosition(Vec2::ZERO);
	_root->setAnchorPoint((_layout == Vertical) ? Vec2(0.0f, 1.0f) : Vec2::ZERO);
	_root->setCascadeOpacityEnabled(true);
	_root->setContentSizeDirtyCallback(std::bind(&ScrollViewBase::onPosition, this));
	_root->setTransformDirtyCallback(std::bind(&ScrollViewBase::onPosition, this));

	return true;
}

void ScrollViewBase::setLayout(Layout l) {
	_layout = l;
	_contentSizeDirty = true;
}

bool ScrollViewBase::visitDraw(FrameInfo &info, NodeVisitFlags parentFlags) {
	if (_scrollDirty) {
		updateScrollBounds();
	}
	if (_animationDirty) {
		fixPosition();
	}
	auto ret = Node::visitDraw(info, parentFlags);
	if (_animationDirty) {
		onPosition();
		onScroll(0, true);
		_animationDirty = false;
	}
	return ret;
}

void ScrollViewBase::handleEnter(Scene *scene) {
	Node::handleEnter(scene);
	onPosition();
}

void ScrollViewBase::handleContentSizeDirty() {
	if (!isnan(_scrollSpaceLimit)) {
		auto padding = _paddingGlobal;
		if (isVertical()) {
			if (_contentSize.width > _scrollSpaceLimit + _scrollSpacePadding * 2.0f) {
				padding.left = padding.right = (_contentSize.width - _scrollSpaceLimit) / 2.0f;
			} else {
				padding.left = padding.right = _scrollSpacePadding;
			}
		} else {
			if (_contentSize.height > _scrollSpaceLimit + _scrollSpacePadding * 2.0f) {
				padding.top = padding.bottom = (_contentSize.height - _scrollSpaceLimit) / 2.0f;
			} else {
				padding.top = padding.bottom = _scrollSpacePadding;
			}
		}
		_paddingGlobal = padding;
	}
	Node::handleContentSizeDirty();
	updateScrollBounds();
	fixPosition();
}

void ScrollViewBase::handleTransformDirty(const Mat4 &parentTransform) {
	Node::handleTransformDirty(parentTransform);

	Vec3 scale;
	parentTransform.decompose(&scale, nullptr, nullptr);

	if (_scale.x != 1.f) {
		scale.x *= _scale.x;
	}
	if (_scale.y != 1.f) {
		scale.y *= _scale.y;
	}
	if (_scale.z != 1.f) {
		scale.z *= _scale.z;
	}

	_globalScale = Vec2(scale.x, scale.y);
}

void ScrollViewBase::setEnabled(bool value) { _inputListener->setEnabled(value); }
bool ScrollViewBase::isEnabled() const { return _inputListener->isEnabled(); }
bool ScrollViewBase::isInMotion() const { return _movement == Movement::Manual; }
bool ScrollViewBase::isMoved() const { return _movement != Movement::None; }

void ScrollViewBase::setScrollCallback(const ScrollCallback &cb) { _scrollCallback = cb; }
const ScrollViewBase::ScrollCallback &ScrollViewBase::getScrollCallback() const {
	return _scrollCallback;
}

void ScrollViewBase::setOverscrollCallback(const OverscrollCallback &cb) {
	_overscrollCallback = cb;
}
const ScrollViewBase::OverscrollCallback &ScrollViewBase::getOverscrollCallback() const {
	return _overscrollCallback;
}

bool ScrollViewBase::addSystemItem(System *cmp) {
	if (auto c = dynamic_cast<ScrollController *>(cmp)) {
		setController(c);
		return true;
	} else {
		return Node::addSystemItem(cmp);
	}
}

void ScrollViewBase::setController(ScrollController *c) {
	if (c != _controller) {
		if (_controller) {
			Node::removeSystem(_controller);
			_controller = nullptr;
		}
		_controller = c;
		if (_controller) {
			Node::addSystemItem(_controller);
		}
	}
}
ScrollController *ScrollViewBase::getController() { return _controller; }

void ScrollViewBase::setPadding(const Padding &p) {
	if (p != _paddingGlobal) {
		_paddingGlobal = p;
		_contentSizeDirty = true;
	}
}
const Padding &ScrollViewBase::getPadding() const { return _paddingGlobal; }

void ScrollViewBase::setSpaceLimit(float value, float padding) {
	if (_scrollSpaceLimit != value || _scrollSpacePadding != padding) {
		_scrollSpaceLimit = value;
		_scrollSpacePadding = padding;
		_contentSizeDirty = true;
	}
}
float ScrollViewBase::getSpaceLimit() const { return _scrollSpaceLimit; }
float ScrollViewBase::getSpacePadding() const { return _scrollSpacePadding; }

float ScrollViewBase::getScrollableAreaOffset() const {
	if (_controller) {
		return _controller->getScrollableAreaOffset();
	}
	return std::numeric_limits<float>::quiet_NaN();
}

float ScrollViewBase::getScrollableAreaSize() const {
	if (_controller) {
		return _controller->getScrollableAreaSize();
	}
	return std::numeric_limits<float>::quiet_NaN();
}

Vec2 ScrollViewBase::getPositionForNode(float scrollPos) const {
	if (isVertical()) {
		return Vec2(0.0f, scrollPos);
	} else {
		return Vec2(scrollPos, 0.0f);
	}
}
Size2 ScrollViewBase::getContentSizeForNode(float size) const {
	if (isVertical()) {
		return Size2(nan(), size);
	} else {
		return Size2(size, nan());
	}
}
Vec2 ScrollViewBase::getAnchorPointForNode() const {
	if (isVertical()) {
		return Vec2(0, 1.0f);
	} else {
		return Vec2(0, 0);
	}
}

float ScrollViewBase::getNodeScrollSize(Size2 size) const {
	return (isVertical()) ? (size.height) : (size.width);
}

float ScrollViewBase::getNodeScrollPosition(Vec2 pos) const {
	return (isVertical()) ? (pos.y) : (pos.x);
}

bool ScrollViewBase::addScrollNode(Node *node, Vec2 pos, Size2 size, ZOrder z, StringView name) {
	updateScrollNode(node, pos, size, z, name);
	if (z != ZOrder(0)) {
		_root->addChild(node, z);
	} else {
		_root->addChild(node);
	}
	return true;
}

void ScrollViewBase::updateScrollNode(Node *node, Vec2 pos, Size2 size, ZOrder z, StringView name) {
	auto p = node->getParent();
	if (p == _root || p == nullptr) {
		auto cs = Size2(isnan(size.width) ? _root->getContentSize().width : size.width,
				isnan(size.height) ? _root->getContentSize().height : size.height);

		node->setContentSize(cs);
		node->setPosition((isVertical() ? Vec2(pos.x, -pos.y) : pos));
		node->setAnchorPoint(getAnchorPointForNode());
		if (z != ZOrder(0)) {
			node->setLocalZOrder(z);
		}
	}
}

bool ScrollViewBase::removeScrollNode(Node *node) {
	if (node && node->getParent() == _root) {
		node->removeFromParent();
		return true;
	}
	return false;
}

float ScrollViewBase::getDistanceFromStart() const {
	auto min = getScrollMinPosition();
	if (!isnan(min)) {
		return fabsf(getScrollPosition() - min);
	} else {
		return std::numeric_limits<float>::quiet_NaN();
	}
}

void ScrollViewBase::setScrollMaxVelocity(float value) { _maxVelocity = value; }
float ScrollViewBase::getScrollMaxVelocity() const { return _maxVelocity; }


Node *ScrollViewBase::getFrontNode() const {
	if (_controller) {
		return _controller->getFrontNode();
	}
	return nullptr;
}
Node *ScrollViewBase::getBackNode() const {
	if (_controller) {
		return _controller->getBackNode();
	}
	return nullptr;
}

float ScrollViewBase::getScrollMinPosition() const {
	auto pos = getScrollableAreaOffset();
	if (!isnan(pos)) {
		return pos - (isVertical() ? _paddingGlobal.top : _paddingGlobal.left);
	}
	if (_controller) {
		float min = _controller->getScrollMin();
		if (!isnan(min)) {
			return min - (isVertical() ? _paddingGlobal.top : _paddingGlobal.left);
		}
	}
	return std::numeric_limits<float>::quiet_NaN();
}

float ScrollViewBase::getScrollMaxPosition() const {
	auto pos = getScrollableAreaOffset();
	auto size = getScrollableAreaSize();
	if (!isnan(pos) && !isnan(size)) {
		pos -= (isVertical() ? _paddingGlobal.top : _paddingGlobal.left);
		size += (isVertical() ? (_paddingGlobal.top + _paddingGlobal.bottom)
							  : (_paddingGlobal.left + _paddingGlobal.right));
		if (size > _scrollSize) {
			return pos + size - _scrollSize;
		} else {
			return pos;
		}
	}
	if (_controller) {
		float min = _controller->getScrollMin();
		float max = _controller->getScrollMax();
		if (!isnan(max) && !isnan(min)) {
			return std::max(min,
					max - _scrollSize
							+ (isVertical() ? _paddingGlobal.bottom : _paddingGlobal.right));
		} else if (!isnan(max)) {
			return max - _scrollSize
					+ (isVertical() ? _paddingGlobal.bottom : _paddingGlobal.right);
		}
	}

	return std::numeric_limits<float>::quiet_NaN();
}

float ScrollViewBase::getScrollLength() const {
	float size = getScrollableAreaSize();
	if (!isnan(size)) {
		return size
				+ (isVertical() ? (_paddingGlobal.top + _paddingGlobal.bottom)
								: (_paddingGlobal.left + _paddingGlobal.right));
	}

	float min = getScrollMinPosition();
	float max = getScrollMaxPosition();

	if (!isnan(min) && !isnan(max)) {
		float trueMax = max - (isVertical() ? _paddingGlobal.bottom : _paddingGlobal.right);
		float trueMin = min + (isVertical() ? _paddingGlobal.top : _paddingGlobal.left);
		if (trueMax > trueMin) {
			return max - min + _scrollSize;
		} else {
			return _scrollSize;
		}
	} else {
		return std::numeric_limits<float>::quiet_NaN();
	}
}

float ScrollViewBase::getScrollSize() const { return _scrollSize; }

void ScrollViewBase::setScrollRelativePosition(float value) {
	if (!isnan(value)) {
		if (value < 0.0f) {
			value = 0.0f;
		} else if (value > 1.0f) {
			value = 1.0f;
		}
	} else {
		value = 0.0f;
	}

	float areaSize = getScrollableAreaSize();
	float areaOffset = getScrollableAreaOffset();
	float size = getScrollSize();

	if (areaSize < size) {
		value = 0.0f;
	}

	auto &padding = getPadding();
	auto paddingFront = (isVertical()) ? padding.top : padding.left;
	auto paddingBack = (isVertical()) ? padding.bottom : padding.right;

	if (!isnan(areaSize) && !isnan(areaOffset) && areaSize > 0) {
		float liveSize = areaSize + paddingFront + paddingBack - size;
		float pos = (value * liveSize) - paddingFront + areaOffset;

		doSetScrollPosition(pos);
	} else {
		_savedRelativePosition = value;
	}
}

float ScrollViewBase::getScrollRelativePosition() const {
	if (!isnan(_savedRelativePosition)) {
		return _savedRelativePosition;
	}

	return getScrollRelativePosition(getScrollPosition());
}

float ScrollViewBase::getScrollRelativePosition(float pos) const {
	float areaSize = getScrollableAreaSize();
	float areaOffset = getScrollableAreaOffset();
	float size = getScrollSize();

	auto &padding = getPadding();
	auto paddingFront = (isVertical()) ? padding.top : padding.left;
	auto paddingBack = (isVertical()) ? padding.bottom : padding.right;

	if (!isnan(areaSize) && !isnan(areaOffset)) {
		float liveSize = areaSize + paddingFront + paddingBack - size;
		return (pos - areaOffset + paddingFront) / liveSize;
	}

	return 0.0f;
}

void ScrollViewBase::setScrollPosition(float pos) {
	if (pos != _scrollPosition) {
		doSetScrollPosition(pos);
	}
}

void ScrollViewBase::doSetScrollPosition(float pos) {
	if (isVertical()) {
		_root->setPositionY(pos + _scrollSize);
	} else {
		_root->setPositionX(-pos);
	}
}

float ScrollViewBase::getScrollPosition() const {
	if (isVertical()) {
		return _root->getPosition().y - _scrollSize;
	} else {
		return -_root->getPosition().x;
	}
}

Vec2 ScrollViewBase::getPointForScrollPosition(float pos) {
	return (isVertical()) ? Vec2(_root->getPosition().x, pos + _scrollSize)
						  : Vec2(-pos, _root->getPosition().y);
}

void ScrollViewBase::onDelta(float delta) {
	auto pos = getScrollPosition();
	if (delta < 0) {
		if (!isnan(_scrollMin) && pos + delta < _scrollMin) {
			if (_bounce) {
				float mod = 1.0f / (1.0f + (_scrollMin - (pos + delta)) / 5.0f);
				setScrollPosition(pos + delta * mod);
				return;
			} else {
				onOverscroll(delta);
				setScrollPosition(_scrollMin);
				return;
			}
		}
	} else if (delta > 0) {
		if (!isnan(_scrollMax) && pos + delta > _scrollMax) {
			if (_bounce) {
				float mod = 1.0f / (1.0f + ((pos + delta) - _scrollMax) / 5.0f);
				setScrollPosition(pos + delta * mod);
				return;
			} else {
				onOverscroll(delta);
				setScrollPosition(_scrollMax);
				return;
			}
		}
	}
	setScrollPosition(pos + delta);
}

void ScrollViewBase::onOverscrollPerformed(float velocity, float pos, float boundary) {
	if (_movement == Movement::Auto) {
		if (_movementAction) {
			auto n = (pos < boundary) ? 1.0f : -1.0f;

			auto vel = _movementAction->getCurrentVelocity();
			auto normal = _movementAction->getNormal();
			if (n * (isVertical() ? normal.y : -normal.x) > 0) {
				velocity = vel;
			} else {
				velocity = -vel;
			}
		}
	}

	if (_animationAction) {
		_root->stopAction(_animationAction);
		_animationAction = nullptr;
		_movementAction = nullptr;
	}

	if (_movement == Movement::Manual || _movement == Movement::None) {
		if (!_bounce && pos == boundary) {
			return;
		}
		if ((pos < boundary && velocity < 0) || (pos > boundary && velocity > 0)) {
			velocity = -fabs(velocity);
		} else {
			velocity = fabs(velocity);
		}
	}

	if (_movement != Movement::Overscroll) {
		Vec2 boundaryPos = getPointForScrollPosition(boundary);
		Vec2 currentPos = getPointForScrollPosition(pos);

		auto a = ActionAcceleratedMove::createBounce(5'000, currentPos, boundaryPos, velocity,
				std::max(25000.0f, fabsf(velocity) * 50));
		if (a) {
			_controller->dropAnimationPadding();
			_movement = Movement::Overscroll;
			_animationAction = Rc<Sequence>::create(a, [this] { onAnimationFinished(); });
			_root->runAction(_animationAction);
		}
	}
}

bool ScrollViewBase::onSwipeEventBegin(uint32_t id, Vec2 loc, Vec2 delta, Vec2 velocity) {
	if (_layout == Layout::Vertical) {
		if (std::abs(delta.x) < std::abs(delta.y)) {
			_inputListener->setExclusiveForTouch(id);
		} else {
			return false;
		}
	} else {
		if (std::abs(delta.x) > std::abs(delta.y)) {
			_inputListener->setExclusiveForTouch(id);
		} else {
			return false;
		}
	}

	auto cs = (_layout == Vertical) ? (_contentSize.height) : (_contentSize.width);
	auto length = getScrollLength();
	if (!isnan(length) && cs >= length) {
		return false;
	}

	onSwipeBegin();

	/*if (_layout == Vertical) {
		return onSwipe(delta.y / _globalScale.y, velocity.y / _globalScale.y, false);
	} else {
		return onSwipe(- delta.x / _globalScale.x, - velocity.x / _globalScale.x, false);
	}*/

	return true;
}

bool ScrollViewBase::onSwipeEvent(uint32_t id, Vec2 loc, Vec2 delta, Vec2 velocity) {
	if (_layout == Vertical) {
		return onSwipe(delta.y / _globalScale.y, velocity.y / _globalScale.y, false);
	} else {
		return onSwipe(-delta.x / _globalScale.x, -velocity.x / _globalScale.x, false);
	}
}

bool ScrollViewBase::onSwipeEventEnd(uint32_t id, Vec2 loc, Vec2 d, Vec2 velocity) {
	_movement = Movement::None;
	if (_layout == Vertical) {
		return onSwipe(0, velocity.y / _inputDensity, true);
	} else {
		return onSwipe(0, -velocity.x / _inputDensity, true);
	}
}

void ScrollViewBase::onSwipeBegin() {
	if (_controller) {
		_controller->dropAnimationPadding();
	}
	_root->stopAllActions();
	_movementAction = nullptr;
	_animationAction = nullptr;
	_movement = Movement::Manual;
}

bool ScrollViewBase::onSwipe(float delta, float velocity, bool ended) {
	if (!ended) {
		if (_scrollFilter) {
			delta = _scrollFilter(delta);
		}
		onDelta(delta);
	} else {
		float pos = getScrollPosition();

		float acceleration = (velocity > 0) ? -5000.0f : 5000.0f;
		if (!isnan(_maxVelocity)) {
			if (velocity > fabs(_maxVelocity)) {
				velocity = fabs(_maxVelocity);
			} else if (velocity < -fabs(_maxVelocity)) {
				velocity = -fabs(_maxVelocity);
			}
		}

		float duration = fabsf(velocity / acceleration);
		float path = velocity * duration + acceleration * duration * duration * 0.5f;

		if (_controller) {
			_controller->setAnimationPadding(path);
			_controller->onScrollPosition();
		}

		if (!isnan(_scrollMin)) {
			if (pos < _scrollMin) {
				float mod = 1.0f / (1.0f + fabsf(_scrollMin - pos) / 5.0f);
				onOverscrollPerformed(velocity * mod, pos, _scrollMin);
				return true;
			}
		}

		if (!isnan(_scrollMax)) {
			if (pos > _scrollMax) {
				float mod = 1.0f / (1.0f + fabsf(_scrollMax - pos) / 5.0f);
				onOverscrollPerformed(velocity * mod, pos, _scrollMax);
				return true;
			}
		}

		if (auto a = onSwipeFinalizeAction(velocity)) {
			_movement = Movement::Auto;
			_animationAction = Rc<Sequence>::create(a, [this] { onAnimationFinished(); });
			_root->runAction(_animationAction);
		} else {
			onScroll(0, true);
		}
	}
	return true;
}

Rc<ActionInterval> ScrollViewBase::onSwipeFinalizeAction(float velocity) {
	if (velocity == 0) {
		return nullptr;
	}

	float acceleration = (velocity > 0) ? -5000.0f : 5000.0f;
	float boundary = (velocity > 0) ? _scrollMax : _scrollMin;

	Vec2 normal = (isVertical()) ? (Vec2(0.0f, (velocity > 0) ? 1.0f : -1.0f))
								 : (Vec2((velocity > 0) ? -1.0f : 1.0f, 0.0f));

	Rc<ActionInterval> a;

	if (!isnan(_maxVelocity)) {
		if (velocity > fabs(_maxVelocity)) {
			velocity = fabs(_maxVelocity);
		} else if (velocity < -fabs(_maxVelocity)) {
			velocity = -fabs(_maxVelocity);
		}
	}

	if (!isnan(boundary)) {
		float pos = getScrollPosition();
		float duration = fabsf(velocity / acceleration);
		float path = velocity * duration + acceleration * duration * duration * 0.5f;

		auto from = _root->getPosition().xy();
		auto to = getPointForScrollPosition(boundary);

		float distance = from.distance(to);
		if (distance < 2.0f) {
			setScrollPosition(boundary);
			return nullptr;
		}

		if ((velocity > 0 && pos + path > boundary) || (velocity < 0 && pos + path < boundary)) {
			_movementAction = ActionAcceleratedMove::createAccelerationTo(from, to, fabsf(velocity),
					-fabsf(acceleration));

			auto overscrollPath = path + ((velocity < 0) ? (distance) : (-distance));
			if (overscrollPath) {
				a = Rc<Sequence>::create(_movementAction,
						[this, overscrollPath] { onOverscroll(overscrollPath); });
			}
		}
	}

	if (!_movementAction) {
		_movementAction = ActionAcceleratedMove::createDecceleration(normal,
				_root->getPosition().xy(), fabs(velocity), fabsf(acceleration));
	}

	if (!a) {
		a = _movementAction;
	}

	return a;
}

void ScrollViewBase::onAnimationFinished() {
	if (_movement != Movement::None) {
		_animationDirty = true;
	}
	if (_controller) {
		_controller->dropAnimationPadding();
	}
	_movement = Movement::None;
	_movementAction = nullptr;
	_animationAction = nullptr;
	onPosition();
}

void ScrollViewBase::fixPosition() {
	if (_movement == Movement::None) {
		auto pos = getScrollPosition();
		if (!isnan(_scrollMin)) {
			if (pos < _scrollMin) {
				setScrollPosition(_scrollMin);
				return;
			}
		}

		if (!isnan(_scrollMax)) {
			if (pos > _scrollMax) {
				setScrollPosition(_scrollMax);
				return;
			}
		}
	}
}

void ScrollViewBase::onPosition() {
	float oldPos = _scrollPosition;
	float newPos;
	if (isVertical()) {
		newPos = _root->getPosition().y - _scrollSize;
	} else {
		newPos = -_root->getPosition().x;
	}

	_scrollPosition = newPos;

	if (_controller) {
		if (_movement == Movement::Auto) {
			_controller->updateAnimationPadding(newPos - oldPos);
		}
		_controller->onScrollPosition();
	}

	if (_movement == Movement::Auto) {
		if (!isnan(_scrollMin)) {
			if (newPos < _scrollMin) {
				onOverscrollPerformed(0, newPos, _scrollMin);
				return;
			}
		}

		if (!isnan(_scrollMax)) {
			if (newPos > _scrollMax) {
				onOverscrollPerformed(0, newPos, _scrollMax);
				return;
			}
		}
	}

	if (_movement != Movement::None && _movement != Movement::Overscroll && newPos - oldPos != 0) {
		onScroll(newPos - oldPos, false);
	} else if (_movement == Movement::Overscroll) {
		if (!isnan(_scrollMin)) {
			if (_scrollPosition < _scrollMin) {
				if (newPos - oldPos < 0) {
					onOverscroll(newPos - oldPos);
				}
				return;
			}
		}

		if (!isnan(_scrollMax)) {
			if (newPos > _scrollMax) {
				if (newPos - oldPos > 0) {
					onOverscroll(newPos - oldPos);
				}
				return;
			}
		}
	}
}

void ScrollViewBase::updateScrollBounds() {
	if ((isVertical() && _contentSize.width == 0) || (isHorizontal() && _contentSize.height == 0)) {
		return;
	}

	if (_contentSizeDirty) {
		if (isVertical()) {
			auto pos = _root->getPosition().y - _scrollSize;
			_scrollSize = _contentSize.height;
			_root->setAnchorPoint(Vec2(0.0f, 1.0f));
			_root->setContentSize(
					Size2(_contentSize.width - _paddingGlobal.left - _paddingGlobal.right, 0));
			_root->setPositionY(pos + _scrollSize);
			_root->setPositionX(_paddingGlobal.left);
		} else {
			_scrollSize = _contentSize.width;
			_root->setAnchorPoint(Vec2::ZERO);
			_root->setContentSize(
					Size2(0, _contentSize.height - _paddingGlobal.top - _paddingGlobal.bottom));
			_root->setPositionY(_paddingGlobal.bottom);
		}
	}

	_scrollMin = getScrollMinPosition();
	_scrollMax = getScrollMaxPosition();

	_scrollDirty = false;

	fixPosition();

	_scrollMin = getScrollMinPosition();
	_scrollMax = getScrollMaxPosition();

	if (!isnan(_savedRelativePosition)) {
		float value = _savedRelativePosition;
		_savedRelativePosition = nan();
		setScrollRelativePosition(value);
	}

	_root->setPositionZ(1.0f);
	_root->setPositionZ(0.0f);
}

void ScrollViewBase::onScroll(float delta, bool finished) {
	if (_controller) {
		_controller->onScroll(delta, finished);
	}
	if (_scrollCallback) {
		_scrollCallback(delta, finished);
	}
}

void ScrollViewBase::onOverscroll(float delta) {
	if (_controller) {
		_controller->onOverscroll(delta);
	}
	if (_overscrollCallback) {
		_overscrollCallback(delta);
	}
}

void ScrollViewBase::setScrollDirty(bool value) { _scrollDirty = value; }

bool ScrollViewBase::onPressBegin(Vec2) {
	_root->stopAllActions();
	onAnimationFinished();
	return false;
}

bool ScrollViewBase::onLongPress(Vec2, TimeInterval time, int count) { return true; }
bool ScrollViewBase::onPressEnd(Vec2, TimeInterval) { return true; }
bool ScrollViewBase::onPressCancel(Vec2, TimeInterval) { return true; }

void ScrollViewBase::onTap(int count, Vec2 loc) { }

Vec2 ScrollViewBase::convertFromScrollableSpace(const Vec2 &pos) {
	return _root->getNodeToParentTransform().transformPoint(pos);
}

Vec2 ScrollViewBase::convertToScrollableSpace(const Vec2 &pos) {
	return _root->getParentToNodeTransform().transformPoint(pos);
}

Vec2 ScrollViewBase::convertFromScrollableSpace(Node *node, Vec2 pos) {
	auto tmp = node->getNodeToParentTransform() * _root->getNodeToParentTransform();
	return tmp.transformPoint(pos);
}

Vec2 ScrollViewBase::convertToScrollableSpace(Node *node, Vec2 pos) {
	auto tmp = _root->getParentToNodeTransform() * node->getParentToNodeTransform();
	return tmp.transformPoint(pos);
}

} // namespace stappler::xenolith::basic2d
