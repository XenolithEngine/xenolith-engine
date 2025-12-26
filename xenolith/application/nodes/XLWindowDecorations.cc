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

#include "XLWindowDecorations.h"
#include "XLEventListener.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool WindowDecorations::init() {
	if (!Node::init()) {
		return false;
	}

	auto makeResizeLayer = [&](Vec2 anchor, WindowLayerFlags flag, WindowCursor cursor) {
		auto node = addChild(Rc<Node>::create());
		node->setAnchorPoint(anchor);
		node->setVisible(false);
		auto l = node->addSystem(Rc<InputListener>::create());
		l->setLayerFlags(flag);
		l->setCursor(cursor);
		return node;
	};

	_resizeTopLeft = makeResizeLayer(Anchor::BottomRight, WindowLayerFlags::ResizeTopLeftGrip,
			WindowCursor::ResizeTopLeft);
	_resizeTop = makeResizeLayer(Anchor::MiddleBottom, WindowLayerFlags::ResizeTopGrip,
			WindowCursor::ResizeTop);
	_resizeTopRight = makeResizeLayer(Anchor::BottomLeft, WindowLayerFlags::ResizeTopRightGrip,
			WindowCursor::ResizeTopRight);
	_resizeRight = makeResizeLayer(Anchor::MiddleLeft, WindowLayerFlags::ResizeRightGrip,
			WindowCursor::ResizeRight);
	_resizeBottomRight = makeResizeLayer(Anchor::TopLeft, WindowLayerFlags::ResizeBottomRightGrip,
			WindowCursor::ResizeBottomRight);
	_resizeBottom = makeResizeLayer(Anchor::MiddleTop, WindowLayerFlags::ResizeBottomGrip,
			WindowCursor::ResizeBottom);
	_resizeBottomLeft = makeResizeLayer(Anchor::TopRight, WindowLayerFlags::ResizeBottomLeftGrip,
			WindowCursor::ResizeBottomLeft);
	_resizeLeft = makeResizeLayer(Anchor::MiddleRight, WindowLayerFlags::ResizeLeftGrip,
			WindowCursor::ResizeLeft);

	auto l = addSystem(Rc<InputListener>::create());
	l->setWindowStateCallback([this](WindowState state, WindowState changes) {
		if (state != _currentState) {
			updateWindowState(state);
		}
		return true;
	});

	auto el = addSystem(Rc<EventListener>::create());
	el->listenForEvent(AppThread::onThemeInfo, [this](const Event &event) {
		updateWindowTheme(event.getObject<AppThread>()->getThemeInfo());
	});

	return true;
}

bool WindowDecorations::shouldBePresentedOnScene(Scene *scene) const {
	auto window = scene->getDirector()->getWindow();
	if (hasFlag(window->getInfo()->flags, WindowCreationFlags::UserSpaceDecorations)
			&& !hasFlag(window->getWindowState(), core::WindowState::Fullscreen)) {
		return true;
	}
	return false;
}

void WindowDecorations::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	_capabilities = _director->getWindow()->getInfo()->capabilities;

	updateWindowState(_director->getWindow()->getWindowState());
}

void WindowDecorations::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	auto &theme = _director->getApplication()->getThemeInfo();

	const float inset = theme.decorations.resizeInset;
	const float resizeBarWidth = 8.0f;
	const float cornerWidth = 16.0f;
	const float cornerInset = 4.0f;
	const float fullInset = cornerInset + inset;

	_resizeTopLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeTopLeft->setPosition(Vec2(fullInset, _contentSize.height - fullInset));

	_resizeTop->setContentSize(Size2(_contentSize.width - fullInset * 2.0f, resizeBarWidth));
	_resizeTop->setPosition(Vec2(_contentSize.width / 2, _contentSize.height - inset));

	_resizeTopRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeTopRight->setPosition(
			Vec2(_contentSize.width - fullInset, _contentSize.height - fullInset));

	_resizeRight->setContentSize(Size2(resizeBarWidth, _contentSize.height - fullInset * 2.0f));
	_resizeRight->setPosition(Vec2(_contentSize.width - inset, _contentSize.height / 2));

	_resizeBottomRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeBottomRight->setPosition(Vec2(_contentSize.width - fullInset, fullInset));

	_resizeBottom->setContentSize(Size2(_contentSize.width - fullInset * 2.0f, resizeBarWidth));
	_resizeBottom->setPosition(Vec2(_contentSize.width / 2, inset));

	_resizeBottomLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeBottomLeft->setPosition(Vec2(fullInset, fullInset));

	_resizeLeft->setContentSize(Size2(resizeBarWidth, _contentSize.height - fullInset * 2.0f));
	_resizeLeft->setPosition(Vec2(inset, _contentSize.height / 2));
}

void WindowDecorations::handleLayout(Node *parent) {
	Node::handleLayout(parent);

	if (!shouldBePresentedOnScene(parent->getScene())) {
		setVisible(false);
		return;
	}

	auto cs = parent->getContentSize();

	setVisible(true);
	setContentSize(Size2(cs.width, cs.height));
	setPosition(cs / 2.0f);
	setAnchorPoint(Anchor::Middle);
	setLocalZOrder(ZOrder::max() - ZOrder(1));

	auto newState = parent->getDirector()->getWindow()->getWindowState();
	if (newState != _currentState) {
		updateWindowState(parent->getDirector()->getWindow()->getWindowState());
	}
	updateWindowTheme(parent->getDirector()->getApplication()->getThemeInfo());
}

void WindowDecorations::updateWindowState(WindowState state) {
	_currentState = state;

	bool allowedResize = hasFlag(state, WindowState::AllowedResize)
			&& !hasFlag(state, WindowState::Fullscreen)
			&& !hasFlagAll(state, WindowState::Maximized);

	_resizeTopLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTopLeft));
	_resizeTop->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTop));
	_resizeTopRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTopRight));
	_resizeRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledRight));
	_resizeBottomRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottomRight));
	_resizeBottom->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottom));
	_resizeBottomLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottomLeft));
	_resizeLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledLeft));
}

void WindowDecorations::updateWindowTheme(const ThemeInfo &theme) {
	log::source().debug("WindowDecorations", "updateWindowTheme: ", theme.colorScheme);
}

} // namespace stappler::xenolith
