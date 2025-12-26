/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLRTTooltip.h"
#include "XLRTListenerView.h"
#include "XLRTRenderer.h"
#include "XLInputListener.h"
#include "XLDynamicStateSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

/*
XL_DECLARE_EVENT_CLASS(Tooltip, onCopy);

Tooltip::~Tooltip() { }

bool Tooltip::init(RendererResult *s, const Vector<String> &ids, WideStringView text) {
	if (!FlexibleLayout::init()) {
		return false;
	}

	setViewDecorationFlags(material2d::ViewDecorationFlags::None);
	setDecorationMask(material2d::DecorationMask::None);

	_closeListener = addInputListener(Rc<InputListener>::create());
	_closeListener->setTouchFilter([] (const InputEvent &ev, const InputListener::DefaultEventFilter &fn) {
		return true;
	});
	_closeListener->addTapRecognizer([this] (const GestureTap &tap) {
		if (!isTouched(tap.location(), 0.0f)) {
			close();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	Rc<ListenerView> view;
	if (!ids.empty()) {
		view = Rc<ListenerView>::create(ListenerView::Vertical, s->source, ids);
		view->setAnchorPoint(Vec2(0, 0));
		view->setPosition(Vec2(0, 0));
		view->setResultCallback(std::bind(&Tooltip::onResult, this, std::placeholders::_1));
		view->setUseSelection(true);
		view->getInputListener()->setSwallowEvents(InputListener::EventMaskTouch);

		auto el = Rc<EventListener>::create();
		el->onEventWithObject(ListenerView::onSelection, view, std::bind(&Tooltip::onSelection, this));
		view->addComponent(el);

		auto r = view->getRenderer();
		r->addOption("tooltip");
		r->addFlag(document::RenderFlags::NoImages);
		r->addFlag(document::RenderFlags::NoHeightCheck);
		r->addFlag(document::RenderFlags::RenderById);
	}

	_toolbar = setFlexibleNode(Rc<material2d::AppBar>::create(material2d::AppBarLayout::Minified,
			material2d::SurfaceStyle(material2d::NodeStyle::SurfaceTonal, material2d::ColorRole::Primary, material2d::Elevation::Level5)));
	_toolbar->setMaxActionIcons(2);
	_toolbar->setTitle(string::toUtf8<Interface>(text));
	_toolbar->setVisible(false);

	_actions = Rc<material2d::MenuSource>::create();
	_actions->addButton("", IconName::Navigation_close_solid, std::bind(&Tooltip::close, this));

	_selectionActions = Rc<material2d::MenuSource>::create();
	_selectionActions->addButton("", IconName::Content_content_copy_solid, std::bind(&Tooltip::copySelection, this));
	_selectionActions->addButton("", IconName::Content_remove_circle_outline, std::bind(&Tooltip::cancelSelection, this));

	_toolbar->setActionMenuSource(_actions);
	_toolbar->setNavButtonIcon(IconName::None);
	_toolbar->setBarCallback([this] {
		onFadeIn();
	});

	if (view) {
		_view = setBaseNode(move(view));
	}

	_contentSizeDirty = true;

	_background->setColorRole(material2d::ColorRole::Background);
	_background->setNodeStyle(material2d::NodeStyle::Elevated);
	_background->setElevation(material2d::Elevation::Level2);

	auto comp = addComponent(Rc<DynamicStateComponent>::create());
	comp->enableScissor(Padding());

	return true;
}

void Tooltip::handleContentSizeDirty() {
	FlexibleLayout::handleContentSizeDirty();
	if (_originPosition.y - 12.0f < _contentSize.height * _anchorPoint.y) {
		setAnchorPoint(Vec2(_anchorPoint.x,
				(_originPosition.y - 12.0f) / _contentSize.height));
	}

	if (_maxContentSize.width != 0.0f) {
		setContentSize(Size2(_maxContentSize.width,
				std::max(_contentSize.height, getCurrentFlexibleMax())));
	} else {
		setContentSize(Size2(_contentSize.width,
				std::max(_contentSize.height, getCurrentFlexibleMax())));
	}
}

void Tooltip::handleEnter(Scene *scene) {
	FlexibleLayout::handleEnter(scene);
}

void Tooltip::onResult(RendererResult *r) {
	Size2 cs = r->result->getContentSize();
	cs.width = _contentSize.width;

	cs.height += getFlexibleMaxHeight() + _view->getPadding().vertical();
	if (cs.height > _maxContentSize.height) {
		cs.height = _maxContentSize.height;
	}

	_defaultSize = cs;
	if (!_expanded) {
		cs.height = _toolbar->getBasicHeight();
	}

	stopAllActions();
	// setContentSize(cs);
	runAction(Rc<Sequence>::create(material2d::makeEasing(Rc<ResizeTo>::create(0.15f, cs), material2d::EasingType::Standard), [this] {
		setOpacity(1.0f);
	}));
}

void Tooltip::setMaxContentSize(const Size2 &size) {
	_maxContentSize = size;
	if (getContentSize().width != _maxContentSize.width) {
		auto initContentSize = Size2(_maxContentSize.width,
				std::max(getContentSize().height, getCurrentFlexibleMax()));
		setContentSize(initContentSize);
		setTargetContentSize(initContentSize);
	}
}

void Tooltip::setOriginPosition(const Vec2 &pos, const Size2 &parentSize, const Vec2 &worldPos) {
	_originPosition = pos;
	_worldPos = worldPos;
	_parentSize = parentSize;

	setAnchorPoint(Vec2(_originPosition.x / _parentSize.width, 1.0f));
}

void Tooltip::setExpanded(bool value) {
	if (value != _expanded) {
		_expanded = value;

		if (_expanded) {
			stopAllActions();
			runAction(Rc<Sequence>::create(Rc<ResizeTo>::create(0.15f, _defaultSize), [this] {
				_view->setVisible(true);
				_view->setOpacity(0);
				_view->runAction(Rc<FadeTo>::create(0.15f, 1.0f));
			}));
			onFadeIn();
		} else {
			_view->setVisible(false);

			auto newSize = Size2(_defaultSize.width, _toolbar->getBasicHeight());
			if (newSize != _defaultSize) {
				stopAllActions();
				runAction(Rc<Sequence>::create(Rc<ResizeTo>::create(0.15f, newSize), [this] {
					onDelayedFadeOut();
				}));
			} else {
				onDelayedFadeOut();
			}
		}
	}
}

void Tooltip::pushToForeground(Scene *scene) {
	auto content = dynamic_cast<material2d::SceneContent2d *>(scene->getContent());
	if (content) {
		content->pushOverlay(this);
	}
}

void Tooltip::close() {
	if (_closeCallback) {
		_closeCallback();
	}

	if (_view) {
		_view->getRenderer()->setEnabled(false);
	}
	//setVisible(false);
	stopAllActions();
	//setShadowZIndexAnimated(0.0f, 0.25f);
	runAction(Rc<Sequence>::create(Rc<ResizeTo>::create(0.15f, Size2(_contentSize.width, 0)), [this] {
		_sceneContent->popOverlay(this);
	}));
}

void Tooltip::onDelayedFadeOut() {
	stopActionByTag(2);
	runAction(Rc<Sequence>::create(3.0f, Rc<FadeTo>::create(0.15f, 0.25f)), 2);
}

void Tooltip::onFadeIn() {
	stopActionByTag(1);
	auto a = getActionByTag(3);
	if (!a || getOpacity() != 255) {
		runAction(Rc<FadeTo>::create(0.15f, 1.0f), 3);
	}
}

void Tooltip::onFadeOut() {
	stopActionByTag(3);
	auto a = getActionByTag(2);
	if (!a || getOpacity() != 48) {
		runAction(Rc<FadeTo>::create(0.15f, 0.25f), 2);
	}
}

void Tooltip::setCloseCallback(const CloseCallback &cb) {
	_closeCallback = cb;
}

Renderer *Tooltip::getRenderer() const {
	return _view?_view->getRenderer():nullptr;
}

String Tooltip::getSelectedString() const {
	return (_view && _view->isSelectionEnabled())?_view->getSelectedString():String();
}

void Tooltip::onPush(material2d::SceneContent2d *l, bool replace) {
	FlexibleLayout::onPush(l, replace);

	setPosition(l->convertToNodeSpace(_worldPos));
	setAnchorPoint(Vec2(_originPosition.x / _parentSize.width, 1.0f));
}

void Tooltip::copySelection() {
	if (_view && _view->isSelectionEnabled()) {
		onCopy(this);
		_view->disableSelection();
	}
}
void Tooltip::cancelSelection() {
	if (_view) {
		_view->disableSelection();
	}
}
void Tooltip::onSelection() {
	if (_view) {
		if (_view->isSelectionEnabled()) {
			_toolbar->replaceActionMenuSource(_selectionActions, 2);
		} else {
			_toolbar->replaceActionMenuSource(_actions, 2);
		}
	}
}
*/

}
