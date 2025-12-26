/**
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

#include "XL2dOverlayLayout.h"
#include "XL2dLayer.h"
#include "XL2dSceneContent.h"
#include "XLInputListener.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool OverlayLayout::init(Vec2 globalOrigin, Binding b, Size2 targetSize) {
	if (!SceneLayout2d::init()) {
		return false;
	}

	_globalOrigin = globalOrigin;
	_binding = b;
	_fullSize = targetSize;
	_collapsedSize = Size2(_fullSize.width, 1);

	auto g = addSystem(Rc<FocusGroup>::create());
	g->setEventMask(FocusGroup::EventMask(EventMaskTouch));
	g->setFlags(FocusGroup::Flags::Exclusive);

	auto l = addSystem(Rc<InputListener>::create());
	l->addTapRecognizer([this](const GestureTap &tap) { return handleTap(tap.location()); },
			InputTapInfo(1, true));

	l->addTouchRecognizer([this](const GestureData &g) {
		if (g.event == GestureEvent::Began) {
			if (_content && !_content->isTouched(g.location())) {
				return true;
			}
			return false;

		} else if (g.event == GestureEvent::Ended) {
			return handleTap(g.location());
		}
		return true;
	});

	return true;
}

void OverlayLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	if (_displaySize != Size2::ZERO) {
		// pop overlay if display size changed
		if (_sceneContent) {
			_sceneContent->popOverlay(this);
		}
	} else {
		_displaySize = _contentSize;
	}
}

void OverlayLayout::handlePushTransitionEnded(SceneContent2d *l, bool replace) {
	SceneLayout2d::handlePushTransitionEnded(l, replace);

	emplaceNode(convertToNodeSpace(_globalOrigin), _binding);
}

void OverlayLayout::handlePopTransitionBegan(SceneContent2d *l, bool replace) {
	SceneLayout2d::handlePopTransitionBegan(l, replace);
	if (_readyCallback) {
		_readyCallback(false);
	}
}

Rc<OverlayLayout::Transition> OverlayLayout::makeExitTransition(SceneContent2d *) const {
	auto collapsedSize = trimSize(_collapsedSize);
	auto sourceSize = _content->getContentSize();
	return Rc<Sequence>::create(Rc<ActionProgress>::create(0.2f,
										[this, collapsedSize, sourceSize](float p) {
		_content->setContentSize(progress(sourceSize, collapsedSize, p));
	}),
			[this] {
		if (_closeCallback) {
			_closeCallback();
		}
	});
}

void OverlayLayout::setReadyCallback(Function<void(bool)> &&cb) { _readyCallback = sp::move(cb); }

void OverlayLayout::setCloseCallback(Function<void()> &&cb) { _closeCallback = sp::move(cb); }

void OverlayLayout::setTargetSize(Size2 size) {
	if (!_content) {
		return;
	}

	_fullSize = size;
	auto targetSize = trimSize(_fullSize);

	targetSize = emplaceContent(_content, _globalOrigin, _binding, _contentSize, targetSize);

	_content->stopAllActions();
	_content->runAction(Rc<Sequence>::create(Rc<ResizeTo>::create(0.2f, targetSize), [this] {
		if (_readyCallback) {
			_readyCallback(true);
		}
	}));
}

void OverlayLayout::emplaceNode(Vec2 origin, Binding b) {
	if (_content) {
		return;
	}

	auto size = _sceneContent->getContentSize();

	auto c = makeContent();
	if (!c) {
		return;
	}

	auto targetSize = trimSize(_fullSize);

	_content = addChild(sp::move(c));
	_content->setContentSize(trimSize(_collapsedSize));
	_content->setAnchorPoint(Anchor::Middle);

	targetSize = emplaceContent(_content, origin, b, size, targetSize);

	_content->runAction(Rc<Sequence>::create(Rc<ResizeTo>::create(0.2f, targetSize), [this] {
		if (_readyCallback) {
			_readyCallback(true);
		}
	}));
}

Size2 OverlayLayout::emplaceContent(Node *node, Vec2 origin, Binding b, Size2 size,
		Size2 targetSize) {
	Padding dist;
	dist.left = origin.x;
	dist.bottom = origin.y;
	dist.right = _contentSize.width - origin.x;
	dist.top = _contentSize.height - origin.y;

	switch (b) {
	case Binding::Relative:
		node->setPositionY(origin.y);
		if (origin.x < Incr / 4) {
			node->setPositionX(Incr / 4);
			node->setAnchorPoint(Vec2(0, 1.0f));
		} else if (origin.x > size.width - Incr / 4) {
			node->setPositionX(size.width - Incr / 4);
			node->setAnchorPoint(Vec2(1, 1.0f));
		} else {
			float rel = (origin.x - Incr / 4) / (size.width - Incr / 2);
			node->setPositionX(origin.x);
			node->setAnchorPoint(Vec2(rel, 1.0f));
		}
		break;
	case Binding::OriginLeft:
		node->setPositionY(origin.y);
		if (origin.x - Incr / 4 < targetSize.width) {
			node->setAnchorPoint(Vec2(0, 1.0f));
			node->setPositionX(Incr / 4);
		} else {
			node->setAnchorPoint(Vec2(1, 1.0f));
			node->setPositionX(origin.x);
		}
		break;
	case Binding::OriginRight:
		node->setPositionY(origin.y);
		if (size.width - origin.x - Incr / 4 < targetSize.width) {
			node->setAnchorPoint(Vec2(1, 1.0f));
			node->setPositionX(size.width - Incr / 4);
		} else {
			node->setAnchorPoint(Vec2(0, 1.0f));
			node->setPositionX(origin.x);
		}
		break;
	case Binding::Anchor:
		// recalculate anchor x
		if (targetSize.width * node->getAnchorPoint().x > dist.left - Incr / 4) {
			node->setAnchorPoint(
					Vec2((dist.left - Incr / 4) / targetSize.width, node->getAnchorPoint().y));
		} else if (targetSize.width * (1.0f - node->getAnchorPoint().x) > dist.right - Incr / 4) {
			node->setAnchorPoint(Vec2(1.0f - (dist.right - Incr / 4) / targetSize.width,
					node->getAnchorPoint().y));
		}
		if (targetSize.height * node->getAnchorPoint().y > dist.bottom - Incr / 4) {
			node->setAnchorPoint(
					Vec2(node->getAnchorPoint().x, (dist.bottom - Incr / 4) / targetSize.height));
		} else if (targetSize.height * (1.0f - node->getAnchorPoint().y) > dist.top - Incr / 4) {
			node->setAnchorPoint(Vec2(node->getAnchorPoint().x,
					1.0f - (dist.top - Incr / 4) / targetSize.height));
		}
		node->setPosition(origin);
		break;
	}

	if (b != Binding::Anchor && b != Binding::Relative) {
		if (targetSize.height > origin.y - Incr / 4) {
			if (origin.y - Incr / 4 < Incr * 4) {
				if (targetSize.height > Incr * 4) {
					targetSize.height = Incr * 4;
				}

				node->setPositionY(targetSize.height + Incr / 4);
			} else {
				targetSize.height = origin.y - Incr / 4;
			}
		}
	} else if (b == Binding::Relative) {
		if (targetSize.height > origin.y - Incr / 4) {
			node->setAnchorPoint(
					Vec2(getAnchorPoint().x, (origin.y - Incr / 4) / targetSize.height));
		}
	}

	if (b != Binding::Anchor && origin.y > size.height - Incr / 4) {
		node->setPositionY(size.height - Incr / 4);
	}
	return targetSize;
}

Rc<Node> OverlayLayout::makeContent() { return Rc<Layer>::create(Color::Grey_500); }

Rc<Action> OverlayLayout::makeEasing(Action *a) { return a; }

Size2 OverlayLayout::trimSize(Size2 size) const {
	if (size.width > _contentSize.width - Incr) {
		size.width = _contentSize.width - Incr;
	}
	if (size.height > _contentSize.height - Incr) {
		size.height = _contentSize.height - Incr;
	}
	return size;
}

bool OverlayLayout::handleTap(Vec2 pt) {
	if (_content && !_content->isTouched(pt)) {
		if (_sceneContent) {
			_sceneContent->popOverlay(this);
		}
	}
	return true;
}

} // namespace stappler::xenolith::basic2d
