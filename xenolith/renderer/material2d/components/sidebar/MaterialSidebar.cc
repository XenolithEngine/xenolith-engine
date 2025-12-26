/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "MaterialSidebar.h"
#include "MaterialEasing.h"
#include "MaterialStyleMonitor.h"
#include "XLInputListener.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

static constexpr uint32_t SHOW_ACTION_TAG = 154;
static constexpr uint32_t HIDE_ACTION_TAG = 154;

Sidebar::~Sidebar() { }

bool Sidebar::init(Position pos) {
	if (!Node::init()) {
		return false;
	}

	_position = pos;

	_listener = addSystem(Rc<InputListener>::create());
	_listener->setTouchFilter(
			[this](const InputEvent &ev, const InputListener::DefaultEventFilter &) -> bool {
		if (!_node) {
			return false;
		}
		if (isNodeEnabled() || (isNodeVisible() && _swallowTouches)) {
			return true;
		} else {
			auto pos = convertToNodeSpace(ev.currentLocation);
			if (_node->isTouched(ev.currentLocation)) {
				return true;
			}
			if (_edgeSwipeEnabled) {
				if ((_position == Left && pos.x < 16.0f)
						|| (_position == Right && pos.x > _contentSize.width - 16.0f)) {
					return true;
				}
			}
			return false;
		}
	});
	_listener->addPressRecognizer([this](const GesturePress &p) -> bool {
		if (isNodeEnabled() && !_node->isTouched(p.location())) {
			if (p.event == GestureEvent::Ended) {
				hide();
			}
			return true;
		}
		return false;
	});
	_listener->addSwipeRecognizer([this](const GestureSwipe &s) -> bool {
		if (!isNodeVisible() && !_edgeSwipeEnabled) {
			return false;
		}

		if (s.event == GestureEvent::Began) {
			if (std::abs(s.delta.y) < std::abs(s.delta.x) && !_node->isTouched(s.location())) {
				stopNodeActions();
				onSwipeDelta(s.delta.x / s.density);
				return true;
			}
			return false;
		} else if (s.event == GestureEvent::Activated) {
			onSwipeDelta(s.delta.x / s.density);
			return true;
		} else {
			onSwipeFinished(s.velocity.x / s.density);
			return true;
		}

		return true;
	});
	_listener->setSwallowEvents(InputListener::EventMaskTouch);

	addSystem(Rc<StyleMonitor>::create([this](const ColorScheme *scheme, const SurfaceStyleData &) {
		_background->setColor(scheme->get(ColorRole::Scrim));
	}));

	_background = addChild(Rc<Layer>::create(Color::Grey_500), ZOrder(-1));
	_background->setAnchorPoint(Vec2(0.0f, 0.0f));
	_background->setVisible(false);
	_background->setOpacity(_backgroundPassiveOpacity);

	return true;
}

void Sidebar::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_background->setContentSize(_contentSize);

	stopNodeActions();

	if (_widthCallback) {
		_nodeWidth = _widthCallback(_contentSize);
	}

	if (_node) {
		_node->setContentSize(Size2(_nodeWidth, _contentSize.height));
		if (_position == Left) {
			_node->setPosition(Vec2(0.0f, 0.0f));
		} else {
			_node->setPosition(Vec2(_contentSize.width, 0.0f));
		}
	}

	setProgress(0.0f);
}

void Sidebar::setBaseNode(Node *n, ZOrder zOrder) {
	if (_node) {
		_node->removeFromParent();
		_node = nullptr;
	}
	_node = n;
	if (getProgress() == 0) {
		_node->setVisible(false);
	}
	addChildNode(_node, zOrder);
	_contentSizeDirty = true;
}

Node *Sidebar::getNode() const { return _node; }

void Sidebar::setNodeWidth(float value) { _nodeWidth = value; }
float Sidebar::getNodeWidth() const { return _nodeWidth; }

void Sidebar::setNodeWidthCallback(const WidthCallback &cb) { _widthCallback = cb; }
const Sidebar::WidthCallback &Sidebar::getNodeWidthCallback() const { return _widthCallback; }

void Sidebar::setSwallowTouches(bool value) { _swallowTouches = value; }
bool Sidebar::isSwallowTouches() const { return _swallowTouches; }

void Sidebar::setEdgeSwipeEnabled(bool value) {
	_edgeSwipeEnabled = value;
	if (isNodeVisible()) {
		if (value) {
			_listener->setSwallowEvents(InputListener::EventMaskTouch);
		} else {
			_listener->clearSwallowEvents(InputListener::EventMaskTouch);
		}
	}
}

bool Sidebar::isEdgeSwipeEnabled() const { return _edgeSwipeEnabled; }

void Sidebar::setBackgroundActiveOpacity(float value) {
	_backgroundActiveOpacity = value;
	_background->setOpacity(
			progress(_backgroundPassiveOpacity, _backgroundActiveOpacity, getProgress()));
}
void Sidebar::setBackgroundPassiveOpacity(float value) {
	_backgroundPassiveOpacity = value;
	_background->setOpacity(
			progress(_backgroundPassiveOpacity, _backgroundActiveOpacity, getProgress()));
}

void Sidebar::show() {
	stopActionByTag(HIDE_ACTION_TAG);
	if (getActionByTag(SHOW_ACTION_TAG) == nullptr) {
		auto a = makeEasing(Rc<ActionProgress>::create(progress(0.35f, 0.0f, getProgress()),
									getProgress(), 1.0f,
									[this](float progress) { setProgress(progress); }),
				EasingType::Standard);
		a->setTag(SHOW_ACTION_TAG);
		runAction(a);
	}
}

void Sidebar::hide(float factor) {
	stopActionByTag(SHOW_ACTION_TAG);
	if (getActionByTag(HIDE_ACTION_TAG) == nullptr) {
		if (factor <= 1.0f) {
			auto a = makeEasing(
					Rc<ActionProgress>::create(progress(0.0f, 0.35f / factor, getProgress()),
							getProgress(), 0.0f, [this](float progress) { setProgress(progress); }),
					EasingType::Standard);
			a->setTag(HIDE_ACTION_TAG);
			runAction(a);
		} else {
			auto a = makeEasing(
					Rc<ActionProgress>::create(progress(0.0f, 0.35f / factor, getProgress()),
							getProgress(), 0.0f, [this](float progress) { setProgress(progress); }),
					EasingType::Standard);
			a->setTag(HIDE_ACTION_TAG);
			runAction(a);
		}
	}
}

float Sidebar::getProgress() const {
	if (_node) {
		return (_position == Left) ? (1.0f - _node->getAnchorPoint().x)
								   : (_node->getAnchorPoint().x);
	}
	return 0.0f;
}

void Sidebar::setProgress(float value) {
	auto prev = getProgress();
	if (_node && value != getProgress()) {
		_node->setAnchorPoint(Vec2((_position == Left) ? (1.0f - value) : (value), 0.0f));
		if (value == 0.0f) {
			if (_node->isVisible()) {
				_node->setVisible(false);
				onNodeVisible(false);
			}
		} else {
			if (!_node->isVisible()) {
				_node->setVisible(true);
				onNodeVisible(true);
			}

			if (value == 1.0f && prev != 1.0f) {
				onNodeEnabled(true);
			} else if (value != 1.0f && prev == 1.0f) {
				onNodeEnabled(false);
			}
		}

		_background->setOpacity(
				progress(_backgroundPassiveOpacity, _backgroundActiveOpacity, value));
		if (!_background->isVisible() && _background->getOpacity() > 0) {
			_background->setVisible(true);
		}
	}
}

void Sidebar::onSwipeDelta(float value) {
	float d = value / _nodeWidth;

	float progress = getProgress() - d * (_position == Left ? -1 : 1);
	if (progress >= 1.0f) {
		progress = 1.0f;
	} else if (progress <= 0.0f) {
		progress = 0.0f;
	}

	setProgress(progress);
}

void Sidebar::onSwipeFinished(float value) {
	float v = value / _nodeWidth;
	auto t = fabsf(v) / (5'000 / _nodeWidth);
	auto d = v * t - (5000.0f * t * t / _nodeWidth) / 2.0f;

	float progress = getProgress() - d * (_position == Left ? -1 : 1);

	if (progress > 0.5) {
		show();
	} else {
		hide(1.0f + fabsf(d) * 2.0f);
	}
}

bool Sidebar::isNodeVisible() const { return getProgress() > 0.0f; }
bool Sidebar::isNodeEnabled() const { return getProgress() == 1.0f; }

void Sidebar::setEnabled(bool value) { _listener->setEnabled(value); }
bool Sidebar::isEnabled() const { return _listener->isEnabled(); }

void Sidebar::setNodeVisibleCallback(BoolCallback &&cb) { _visibleCallback = sp::move(cb); }
void Sidebar::setNodeEnabledCallback(BoolCallback &&cb) { _enabledCallback = sp::move(cb); }

void Sidebar::onNodeEnabled(bool value) {
	if (_enabledCallback) {
		_enabledCallback(value);
	}
}
void Sidebar::onNodeVisible(bool value) {
	if (value) {
		if (_swallowTouches) {
			_listener->setSwallowEvents(InputListener::EventMaskTouch);
		}
		if (_backgroundPassiveOpacity == 0) {
			_background->setVisible(false);
		}
	} else {
		_background->setVisible(true);
		_listener->clearSwallowEvents(InputListener::EventMaskTouch);
	}

	if (_visibleCallback) {
		_visibleCallback(value);
	}
}

void Sidebar::stopNodeActions() {
	stopActionByTag(SHOW_ACTION_TAG);
	stopActionByTag(HIDE_ACTION_TAG);
}

} // namespace stappler::xenolith::material2d
