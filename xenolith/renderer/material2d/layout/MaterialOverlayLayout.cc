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

#include "MaterialOverlayLayout.h"
#include "MaterialEasing.h"
#include "XLInputListener.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

constexpr static float OverlayHorizontalIncrement = 56.0f;

OverlayLayout::~OverlayLayout() { }

bool OverlayLayout::init(const Vec2 &globalOrigin, Binding b, Surface *node, Size2 targetSize) {
	if (!SceneLayout2d::init()) {
		return false;
	}

	addChildNode(node, ZOrder(1));
	_surface = node;
	_surface->setAnchorPoint(Anchor::MiddleTop);

	_globalOrigin = globalOrigin;
	_binding = b;
	_fullSize = targetSize;

	auto l = addSystem(Rc<InputListener>::create());
	l->setSwallowEvents(InputListener::makeEventMask(
			{InputEventName::Begin, InputEventName::MouseMove, InputEventName::Scroll}));
	l->setTouchFilter([this](const InputEvent &ev, const InputListener::DefaultEventFilter &def) {
		if (!_surface->isTouched(ev.currentLocation)) {
			return def(ev);
		}
		return false;
	});
	l->addTapRecognizer([this](const GestureTap &tap) {
		if (!_surface->isTouched(tap.location())) {
			if (_sceneContent) {
				_sceneContent->popOverlay(this);
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	return true;
}

void OverlayLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	if (_initSize != Size2::ZERO) {
		if (_sceneContent) {
			_sceneContent->popOverlay(this);
		}
	} else {
		_initSize = _contentSize;
	}
}

void OverlayLayout::handlePushTransitionEnded(SceneContent2d *l, bool replace) {
	SceneLayout2d::handlePushTransitionEnded(l, replace);

	emplaceNode(_globalOrigin, _binding);
}

void OverlayLayout::handlePopTransitionBegan(SceneContent2d *l, bool replace) {
	SceneLayout2d::handlePopTransitionBegan(l, replace);
	if (_readyCallback) {
		_readyCallback(false);
	}
}

Rc<OverlayLayout::Transition> OverlayLayout::makeExitTransition(SceneContent2d *) const {
	return Rc<Sequence>::create(makeEasing(Rc<ActionProgress>::create(0.2f,
										[this](float p) {
		_surface->setContentSize(progress(_fullSize, Size2(_fullSize.width, 1), p));
	})),
			[this] {
		if (_closeCallback) {
			_closeCallback();
		}
	});
}

void OverlayLayout::setReadyCallback(Function<void(bool)> &&cb) { _readyCallback = sp::move(cb); }

void OverlayLayout::setCloseCallback(Function<void()> &&cb) { _closeCallback = sp::move(cb); }

void OverlayLayout::emplaceNode(const Vec2 &origin, Binding b) {
	const float incr = OverlayHorizontalIncrement;
	auto size = _sceneContent->getContentSize();

	switch (b) {
	case Binding::Relative:
		_surface->setPositionY(origin.y);
		if (origin.x < incr / 4) {
			_surface->setPositionX(incr / 4);
			_surface->setAnchorPoint(Vec2(0, 1.0f));
		} else if (origin.x > size.width - incr / 4) {
			_surface->setPositionX(size.width - incr / 4);
			_surface->setAnchorPoint(Vec2(1, 1.0f));
		} else {
			float rel = (origin.x - incr / 4) / (size.width - incr / 2);
			_surface->setPositionX(origin.x);
			_surface->setAnchorPoint(Vec2(rel, 1.0f));
		}
		break;
	case Binding::OriginLeft:
		_surface->setPositionY(origin.y);
		if (origin.x - incr / 4 < _fullSize.width) {
			_surface->setAnchorPoint(Vec2(0, 1.0f));
			_surface->setPositionX(incr / 4);
		} else {
			_surface->setAnchorPoint(Vec2(1, 1.0f));
			_surface->setPositionX(origin.x);
		}
		break;
	case Binding::OriginRight:
		_surface->setPositionY(origin.y);
		if (size.width - origin.x - incr / 4 < _fullSize.width) {
			_surface->setAnchorPoint(Vec2(1, 1.0f));
			_surface->setPositionX(size.width - incr / 4);
		} else {
			_surface->setAnchorPoint(Vec2(0, 1.0f));
			_surface->setPositionX(origin.x);
		}
		break;
	case Binding::Anchor: _surface->setPosition(origin); break;
	}

	if (b != Binding::Anchor && b != Binding::Relative) {
		if (_fullSize.height > origin.y - incr / 4) {
			if (origin.y - incr / 4 < incr * 4) {
				if (_fullSize.height > incr * 4) {
					_fullSize.height = incr * 4;
				}

				_surface->setPositionY(_fullSize.height + incr / 4);
			} else {
				_fullSize.height = origin.y - incr / 4;
			}
		}
	} else if (b == Binding::Relative) {
		if (_fullSize.height > origin.y - incr / 4) {
			_surface->setAnchorPoint(
					Vec2(getAnchorPoint().x, (origin.y - incr / 4) / _fullSize.height));
		}
	}

	if (origin.y > size.height - incr / 4) {
		_surface->setPositionY(size.height - incr / 4);
	}

	_surface->setContentSize(Size2(_fullSize.width, 1));
	_surface->runAction(Rc<Sequence>::create(
			material2d::makeEasing(Rc<ResizeTo>::create(0.2f, _fullSize)), [this] {
		if (_readyCallback) {
			_readyCallback(true);
		}
	}));
}

} // namespace stappler::xenolith::material2d
