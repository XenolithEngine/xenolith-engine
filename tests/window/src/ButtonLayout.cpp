/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "ButtonLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// `button` (type selector) -> vector fill (background-color) + stroke (outline-color/width),
// applied by the registered "button" type appliers, NOT the generic node-color path.
// `label` (the button's child) -> color/font, applied by the recursive resolver over the frame
// stack. Two distinct buttons prove the per-type registry is shared and data-driven.
static constexpr auto s_buttonCss = StringView(R"css(
button {
	width: 280px;
	height: 80px;
	background-color: #1e88e5;
	outline-color: #0d47a1;
	outline-width: 6px;
	border-radius: 20px;
	display: flex;
	justify-content: center;
	align-items: center;
	column-gap: 12px;
}
button.danger {
	background-color: #e53935;
	outline-color: #7f0000;
	outline-width: 10px;
	/* 4-value shorthand: TL=40 TR=0 BR=40 BL=0 -> a distinctive "leaf" to verify corner mapping */
	border-radius: 40px 0px 40px 0px;
}
label {
	color: #ffffff;
	font-size: 28px;
}
)css");

} // namespace

bool ButtonLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_buttonCss);

	// StyleSystem only supplies the stylesheet - a StyleResolver is what actually applies it.
	// One recursive resolver on the layout covers both buttons and their label/icon children.
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto primary = addChild(Rc<ui::Button>::create(), ZOrder(1));
	primary->setString("Primary");
	primary->setIcon(basic2d::IconName::Action_favorite_solid);
	_buttons.emplace_back(primary);

	auto danger = addChild(Rc<ui::Button>::create(), ZOrder(1));
	danger->setString("Danger");
	danger->addStyleClass("danger");
	_buttons.emplace_back(danger);

	return true;
}

void ButtonLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	const float top = getWorkTop() - 140.0f;
	for (size_t i = 0; i < _buttons.size(); ++i) {
		auto b = _buttons[i];
		b->setAnchorPoint(Vec2(0.0f, 1.0f));
		b->setPosition(Vec2(48.0f, top - float(i) * 128.0f));
	}
}

} // namespace stappler::xenolith::app
