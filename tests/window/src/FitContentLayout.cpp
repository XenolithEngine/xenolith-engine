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

#include "FitContentLayout.h"
#include "XLUiStyleSystem.h"
#include "XLAction.h"

#include <stdlib.h> // getenv for the headless append hook

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;
using ui::FlexDirection;
using ui::FlexWrap;

namespace {

// A label whose committed box is made visible by a background layer kept in
// sync with the content size assigned by the layout.
Rc<Label> makeFitLabel(StringView text, const Color4F &bg) {
	auto label = Rc<Label>::create();
	label->setFontSize(20);
	label->setString(text);
	label->setColor(Color::White);

	auto layer = label->addChild(Rc<Layer>::create(bg), ZOrder(-1));
	layer->setAnchorPoint(Anchor::BottomLeft);
	layer->setPosition(Vec2(0.0f, 0.0f));

	label->setContentSizeDirtyCallback(
			[label = label.get(), layer] { layer->setContentSize(label->getContentSize()); });

	return label;
}

} // namespace

bool FitContentLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	using ui::FlexAlign;
	using ui::FlexItemInfo;
	using ui::FlexLayoutInfo;
	using ui::LayoutSystem;

	// --- control bar: itself a ui flex container ----------------------------
	_controls = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));

	FlexLayoutInfo controlsInfo;
	controlsInfo.direction = FlexDirection::Row;
	controlsInfo.wrap = FlexWrap::NoWrap;
	controlsInfo.alignItems = FlexAlign::Stretch;
	controlsInfo.columnGap = 6.0f;
	controlsInfo.padding = Padding(6.0f);
	_controls->addSystem(Rc<LayoutSystem>::create(controlsInfo));

	addControlButton("Back", [this] { pop(); });
	addControlButton("Append", [this] { appendText(); });
	addControlButton("Wrap", [this] { toggleWrap(); });

	// --- demonstration container --------------------------------------------
	_demo = addChild(Rc<Layer>::create(Color::Grey_400), ZOrder(0));
	_demo->setName("fit-demo");

	// stylesheet scope for the CSS-driven item below
	_demo->addSystem(
			Rc<ui::StyleSystem>::create(StringView(".css-fit { flex-basis: fit-content; }")));

	FlexLayoutInfo demoInfo;
	demoInfo.direction = FlexDirection::Row;
	demoInfo.wrap = FlexWrap::NoWrap;
	demoInfo.justifyContent = ui::FlexJustify::FlexStart;
	// FlexStart (not Stretch) so the measured cross sizes stay visible
	demoInfo.alignItems = FlexAlign::FlexStart;
	demoInfo.columnGap = 12.0f;
	demoInfo.rowGap = 12.0f;
	demoInfo.padding = Padding(16.0f);
	_demoFlex = _demo->addSystem(Rc<LayoutSystem>::create(demoInfo));

	// 1. a short label sized to its text: max-content basis + measured cross
	{
		auto label = _demo->addChild(makeFitLabel("Fit", Color::Red_400), ZOrder(1));
		FlexItemInfo item;
		item.basis = FlexItemInfo::FitContent;
		LayoutSystem::setItem(label, item);
	}

	// 2. a long label clamped by maxMain: the committed width re-wraps the
	// text, the re-measured cross grows to the wrapped height
	{
		auto label = _demo->addChild(
				makeFitLabel("A long fit-content label that wraps when clamped", Color::Blue_400),
				ZOrder(1));
		FlexItemInfo item;
		item.basis = FlexItemInfo::FitContent;
		item.maxMain = 220.0f;
		LayoutSystem::setItem(label, item);
	}

	// 3. a nested fit-content chip: the outer row measures the chip through
	// the chip's own LayoutSystem (dry-run), so the chip hugs label + padding
	{
		auto chip = _demo->addChild(Rc<Layer>::create(Color::Teal_400), ZOrder(1));
		chip->setName("chip");

		FlexLayoutInfo chipInfo;
		chipInfo.direction = FlexDirection::Row;
		chipInfo.alignItems = FlexAlign::FlexStart;
		chipInfo.columnGap = 6.0f;
		chipInfo.padding = Padding(8.0f);
		chip->addSystem(Rc<LayoutSystem>::create(chipInfo));

		FlexItemInfo chipItem;
		chipItem.basis = FlexItemInfo::FitContent;
		LayoutSystem::setItem(chip, chipItem);

		_appendTarget = chip->addChild(Rc<Label>::create(), ZOrder(1));
		_appendTarget->setFontSize(20);
		_appendTarget->setString("Nested");
		_appendTarget->setColor(Color::White);

		FlexItemInfo labelItem;
		labelItem.basis = FlexItemInfo::FitContent;
		LayoutSystem::setItem(_appendTarget, labelItem);
	}

	// 4. CSS path: the fit-content basis comes from the stylesheet above
	{
		auto label = _demo->addChild(makeFitLabel("CSS", Color::Purple_400), ZOrder(1));
		label->addStyleClass("css-fit");
	}

	// headless invalidation hook for the screenshot workflow: extend the
	// nested label after the first layouts settled, so the capture shows the
	// child -> chip -> container reflow, not the initial placement
	if (::getenv("XL_FITCONTENT_APPEND")) {
		runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.5f), [this] {
			appendText();
			appendText();
		}));
	}

	return true;
}

basic2d::Layer *FitContentLayout::addControlButton(StringView title, Function<void()> &&cb) {
	auto btn = _controls->addChild(Rc<simpleui::ButtonWithLabel>::create(title, sp::move(cb)),
			ZOrder(1));

	ui::FlexItemInfo item;
	item.basis = 110.0f;
	item.grow = 1.0f;
	item.shrink = 1.0f;
	ui::LayoutSystem::setItem(btn, item);

	return btn;
}

void FitContentLayout::appendText() {
	++_appendCount;
	StringStream str;
	str << "Nested";
	for (uint32_t i = 0; i < _appendCount; ++i) { str << " word"; }
	// the label re-shapes lazily on its next visit; the resulting size change
	// must bubble chip -> demo container and re-run both layouts
	_appendTarget->setString(str.str());
}

void FitContentLayout::toggleWrap() {
	auto info = _demoFlex->getInfo();
	_demoFlex->setWrap(
			(info && info->wrap == FlexWrap::Wrap) ? FlexWrap::NoWrap : FlexWrap::Wrap);
}

void FitContentLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();

	// control bar pinned to the top, spanning the full width
	_controls->setAnchorPoint(Anchor::BottomLeft);
	_controls->setPosition(Vec2(0.0f, getWorkTop() - _controlsHeight));
	_controls->setContentSize(Size2(cs.width, _controlsHeight));

	// demonstration container fills the remaining area below the control bar
	_demo->setAnchorPoint(Anchor::BottomLeft);
	_demo->setPosition(Vec2(0.0f, 0.0f));
	_demo->setContentSize(Size2(cs.width, sprt::max(getWorkTop() - _controlsHeight, 0.0f)));
}

} // namespace stappler::xenolith::app
