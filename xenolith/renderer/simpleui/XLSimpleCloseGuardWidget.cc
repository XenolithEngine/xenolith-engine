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

#include "XLSimpleCloseGuardWidget.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

bool CloseGuardWidgetDefault::init() {
	if (!CloseGuardWidget::init()) {
		return false;
	}

	auto f = addSystem(Rc<FocusGroup>::create());
	f->setFlags(FocusGroup::Flags::Exclusive | FocusGroup::Flags::Propagate);
	f->setEventMask(makeEventMask({InputEventName::Begin, InputEventName::MouseMove,
		InputEventName::Scroll, InputEventName::KeyPressed}));

	// Add listener to capture exclusive events for the group
	auto l = addSystem(Rc<InputListener>::create());
	l->addMoveRecognizer([](const GestureData &) { return true; });
	l->addScrollRecognizer([](const GestureData &) { return true; });
	l->addTapRecognizer([this](const GestureTap &tap) {
		if (!_layer->isTouched(tap.location())) {
			reject();
		}
		return true;
	}, InputTapInfo(1));

	_background = addChild(Rc<Layer>::create(Color4F(0, 0, 0, 0.2f)), ZOrder(-1));
	_background->setAnchorPoint(Anchor::Middle);

	_layer = addChild(Rc<Layer>::create(Color::White));
	_layer->setAnchorPoint(Anchor::Middle);

	_description = addChild(Rc<Label>::create("Window asks to be closed"), ZOrder(1));
	_description->setFontSize(font::FontSize(20));
	_description->setAnchorPoint(Anchor::Middle);
	_description->setColor(Color::Black);

	_commitButton = addChild(Rc<ButtonWithLabel>::create("Commit", [this]() { commit(); }));
	_commitButton->setAnchorPoint(Anchor::TopRight);

	_rejectButton = addChild(Rc<ButtonWithLabel>::create("Reject", [this]() { reject(); }));
	_rejectButton->setAnchorPoint(Anchor::TopLeft);

	return true;
}

void CloseGuardWidgetDefault::handleContentSizeDirty() {
	CloseGuardWidget::handleContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);

	_layer->setContentSize(Size2(260.0f, 120.0f));
	_layer->setPosition(_contentSize / 2.0f);

	_description->setPosition(Vec2(_contentSize / 2.0f) + Vec2(0.0f, 20.0f));

	_commitButton->setContentSize(Size2(82.0f, 32.0f));
	_commitButton->setPosition(
			Vec2(_contentSize.width / 2.0f - 16.0f, _contentSize.height / 2.0f - 16.0f));

	_rejectButton->setContentSize(Size2(82.0f, 32.0f));
	_rejectButton->setPosition(
			Vec2(_contentSize.width / 2.0f + 16.0f, _contentSize.height / 2.0f - 16.0f));
}

void CloseGuardWidgetDefault::handleLayout(Node *parent) {
	CloseGuardWidget::handleLayout(parent);

	setAnchorPoint(Anchor::Middle);
	setContentSize(parent->getContentSize());
	setPosition(parent->getContentSize() / 2.0f);
	setLocalZOrder(ZOrder::max() - ZOrder(2));
}

} // namespace stappler::xenolith::simpleui
