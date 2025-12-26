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

#include "MaterialMenuSeparator.h"
#include "MaterialStyleMonitor.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

MenuSeparator::~MenuSeparator() { }

bool MenuSeparator::init(Menu *menu, MenuSourceItem *item) {
	if (!Node::init()) {
		return false;
	}

	setCascadeColorEnabled(true);

	_color = addChild(Rc<Layer>::create());
	_color->setOpacity(OpacityValue(32));
	_color->setColor(Color::Black);
	_color->setAnchorPoint(Vec2(0, 0.5f));
	_color->setPosition(Vec2::ZERO);

	_contentSizeDirty = true;

	_itemListener = addSystem(Rc<DataListener<MenuSourceItem>>::create(
			[this](SubscriptionFlags flags) { handleSourceDirty(); }));
	_itemListener->setSubscription(item);

	addSystem(Rc<StyleMonitor>::create(
			[this](const ColorScheme *scheme, const SurfaceStyleData &data) {
		_color->setColor(scheme->get(ColorRole::Outline));
	}));

	setMenu(menu);

	return true;
}

void MenuSeparator::handleContentSizeDirty() {
	Node::handleContentSizeDirty();
	handleSourceDirty();
}

void MenuSeparator::setTopLevel(bool value) {
	if (value != _topLevel) {
		_topLevel = value;
		handleSourceDirty();
	}
}

void MenuSeparator::handleSourceDirty() {
	_color->setContentSize(Size2(_contentSize.width, 2));
	if (_topLevel) {
		_color->setAnchorPoint(Vec2(0.0f, 1.0f));
		_color->setPosition(Vec2(0, _contentSize.height));
	} else {
		_color->setAnchorPoint(Vec2(0, 0.5f));
		_color->setPosition(Vec2(0, _contentSize.height / 2));
	}
}

} // namespace stappler::xenolith::material2d
