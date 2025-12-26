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

	_indicator =
			addChild(Rc<LayerRounded>::create(Color4F(1.0f, 1.0f, 1.0f, 0.0f), 2.0f), ZOrder(1));
	_indicator->setAnchorPoint(Vec2(1, 0));

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
				std::min(_contentSize.width * Overscroll::OverscrollScale,
						Overscroll::OverscrollMaxHeight)));

		_overflowBack->setAnchorPoint(Vec2(0.0f, 0.0f)); // bottom
		_overflowBack->setDirection(Overscroll::Direction::Bottom);
		_overflowBack->setPosition(Vec2(0, _overscrollBackOffset));
		_overflowBack->setContentSize(Size2(_contentSize.width,
				std::min(_contentSize.width * Overscroll::OverscrollScale,
						Overscroll::OverscrollMaxHeight)));

	} else {
		_overflowFront->setAnchorPoint(Vec2(0, 0)); // left
		_overflowFront->setDirection(Overscroll::Direction::Left);
		_overflowFront->setPosition(Vec2(_overscrollFrontOffset, 0));
		_overflowFront->setContentSize(
				Size2(std::min(_contentSize.height * Overscroll::OverscrollScale,
							  Overscroll::OverscrollMaxHeight),
						_contentSize.height));

		_overflowBack->setAnchorPoint(Vec2(1, 0)); // right
		_overflowBack->setDirection(Overscroll::Direction::Right);
		_overflowBack->setPosition(Vec2(_contentSize.width - _overscrollBackOffset, 0));
		_overflowBack->setContentSize(
				Size2(std::min(_contentSize.height * Overscroll::OverscrollScale,
							  Overscroll::OverscrollMaxHeight),
						_contentSize.height));
	}
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
	_indicator->setPathColor(val, withOpacity);
}

Color4F ScrollView::getIndicatorColor() const { return _indicator->getColor(); }

void ScrollView::setIndicatorVisible(bool value) {
	_indicatorVisible = value;
	if (!isnan(getScrollLength())) {
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
			(_scrollPosition - getScrollMinPosition())
					/ (getScrollMaxPosition() - getScrollMinPosition()),
			true, 20.0f);
}

void ScrollView::updateIndicatorPosition(Node *indicator, float size, float value, bool actions,
		float min) {
	if (!_indicatorVisible) {
		return;
	}

	float scrollWidth = _contentSize.width;
	float scrollHeight = _contentSize.height;

	float scrollLength = getScrollLength();
	if (isnan(scrollLength)) {
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
		if (isVertical()) {
			float h = (scrollHeight - 4 - paddingLocal.top - paddingLocal.bottom) * size;
			if (h < min) {
				h = min;
			}
			float r = scrollHeight - h - 4 - paddingLocal.top - paddingLocal.bottom;

			indicator->setContentSize(Size2(3, h));
			indicator->setPosition(
					Vec2(scrollWidth - 2, paddingLocal.bottom + 2 + r * (1.0f - value)));
			indicator->setAnchorPoint(Vec2(1, 0));
		} else {
			float h = (scrollWidth - 4 - paddingLocal.left - paddingLocal.right) * size;
			if (h < min) {
				h = min;
			}
			float r = scrollWidth - h - 4 - paddingLocal.left - paddingLocal.right;

			indicator->setContentSize(Size2(h, 3));
			indicator->setPosition(Vec2(paddingLocal.left + 2 + r * (value), 2));
			indicator->setAnchorPoint(Vec2(0, 0));
		}
		if (actions) {
			if (indicator->getOpacity() != 1.0f) {
				Action *a = indicator->getActionByTag(19);
				if (!a) {
					indicator->runAction(
							Rc<FadeTo>::create(progress(0.1f, 0.0f, indicator->getOpacity()), 1.0f),
							19);
				}
			}

			indicator->stopActionByTag(18);
			auto fade = Rc<Sequence>::create(2.0f, Rc<FadeTo>::create(0.25f, 0.0f));
			indicator->runAction(fade, 18);
		}
	} else {
		indicator->setVisible(false);
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
	auto factor = std::min(64.0f, _adjustValue);

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
	if (!isnan(newPos)) {
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
