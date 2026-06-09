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

#include "XLCommon.h"

#include "FlexboxLayout.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace simpleui;

namespace {

StringView directionName(FlexDirection d) {
	switch (d) {
	case FlexDirection::Row: return StringView("Row");
	case FlexDirection::RowReverse: return StringView("Row-rev");
	case FlexDirection::Column: return StringView("Column");
	case FlexDirection::ColumnReverse: return StringView("Col-rev");
	}
	return StringView("?");
}

StringView wrapName(FlexWrap w) {
	switch (w) {
	case FlexWrap::NoWrap: return StringView("No-wrap");
	case FlexWrap::Wrap: return StringView("Wrap");
	case FlexWrap::WrapReverse: return StringView("Wrap-rev");
	}
	return StringView("?");
}

StringView justifyName(FlexJustify j) {
	switch (j) {
	case FlexJustify::FlexStart: return StringView("Start");
	case FlexJustify::FlexEnd: return StringView("End");
	case FlexJustify::Center: return StringView("Center");
	case FlexJustify::SpaceBetween: return StringView("Between");
	case FlexJustify::SpaceAround: return StringView("Around");
	case FlexJustify::SpaceEvenly: return StringView("Evenly");
	}
	return StringView("?");
}

StringView alignName(FlexAlign a) {
	switch (a) {
	case FlexAlign::Stretch: return StringView("Stretch");
	case FlexAlign::FlexStart: return StringView("Start");
	case FlexAlign::FlexEnd: return StringView("End");
	case FlexAlign::Center: return StringView("Center");
	default: break;
	}
	return StringView("?");
}

// A colored box with a centered caption, used as a flex item in the demo.
Rc<Layer> makeBox(const Color4F &color, StringView text) {
	auto layer = Rc<Layer>::create(color);

	auto label = layer->addChild(Rc<Label>::create(), ZOrder(1));
	label->setAnchorPoint(Anchor::Middle);
	label->setAlignment(font::TextAlign::Center);
	label->setFontSize(16);
	label->setString(text);
	label->setColor(Color::White);

	layer->setName(text);

	// keep the caption centered whenever the box is (re-)sized by the layout
	layer->setContentSizeDirtyCallback([box = layer.get(), label] {
		auto cs = box->getContentSize();
		label->setPosition(cs / 2.0f);
		label->setWidth(sprt::max(cs.width - 6.0f, 1.0f));
	});

	return layer;
}

} // namespace

bool FlexboxLayout::init() {
	if (!SceneLayout2d::init()) {
		return false;
	}

	// --- control bar -------------------------------------------------------
	// The control bar is itself a flex container: a single non-wrapping row of
	// buttons that grow to share the width and shrink on narrow windows.
	_controls = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));

	FlexLayoutInfo controlsInfo;
	controlsInfo.direction = FlexDirection::Row;
	controlsInfo.wrap = FlexWrap::NoWrap;
	controlsInfo.justifyContent = FlexJustify::FlexStart;
	controlsInfo.alignItems = FlexAlign::Stretch;
	controlsInfo.columnGap = 6.0f;
	controlsInfo.padding = Padding(6.0f);
	_controlsFlex = _controls->addSystem(Rc<FlexLayout>::create(controlsInfo));

	addControlButton("Back", [this] { pop(); });
	_btnDirection = static_cast<ButtonWithLabel *>(
			addControlButton("Dir", [this] { cycleDirection(); }));
	_btnWrap = static_cast<ButtonWithLabel *>(addControlButton("Wrap", [this] { cycleWrap(); }));
	_btnJustify = static_cast<ButtonWithLabel *>(
			addControlButton("Justify", [this] { cycleJustify(); }));
	_btnAlign = static_cast<ButtonWithLabel *>(addControlButton("Align", [this] { cycleAlign(); }));

	// --- demonstration container ------------------------------------------
	// The container node carries the FlexLayoutInfo component (shared params),
	// the FlexLayout system reads it, and each child carries its own FlexItemInfo.
	_demo = addChild(Rc<Layer>::create(Color::Grey_400), ZOrder(0));
	_demo->setName("flex-demo");

	FlexLayoutInfo demoInfo;
	demoInfo.direction = FlexDirection::Row;
	demoInfo.wrap = FlexWrap::Wrap;
	demoInfo.justifyContent = FlexJustify::FlexStart;
	demoInfo.alignItems = FlexAlign::Stretch;
	demoInfo.columnGap = 12.0f;
	demoInfo.rowGap = 12.0f;
	demoInfo.padding = Padding(16.0f);
	_demoFlex = _demo->addSystem(Rc<FlexLayout>::create(demoInfo));

	// Each box declares an explicit main `basis` and a definite `crossSize`, so
	// the placement is fully deterministic. With the default alignItems=Stretch
	// the boxes fill the cross axis; switching alignment reveals their own
	// cross sizes (varying heights below).

	// Box A: inflexible, keeps its basis.
	{
		auto box = _demo->addChild(makeBox(Color::Red_400, "A"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 90.0f;
		item.crossSize = 60.0f;
		box->setComponent<FlexItemInfo>(item);
	}

	// Box B: grows to take a single share of the free space.
	{
		auto box = _demo->addChild(makeBox(Color::Blue_400, "B grow:1"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 90.0f;
		item.crossSize = 80.0f;
		item.grow = 1.0f;
		box->setComponent<FlexItemInfo>(item);
	}

	// Box C: grows twice as fast as B.
	{
		auto box = _demo->addChild(makeBox(Color::Green_400, "C grow:2"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 90.0f;
		item.crossSize = 100.0f;
		item.grow = 2.0f;
		box->setComponent<FlexItemInfo>(item);
	}

	// Box D: aligns itself to the center of the line and keeps an outer margin,
	// overriding the container's alignItems.
	{
		auto box = _demo->addChild(makeBox(Color::Amber_400, "D self:center"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 120.0f;
		item.crossSize = 60.0f;
		item.alignSelf = FlexAlign::Center;
		item.margin = Padding(8.0f);
		box->setComponent<FlexItemInfo>(item);
	}

	// Box E: lower order, so it is placed before every other box.
	{
		auto box = _demo->addChild(makeBox(Color::Purple_400, "E order:-1"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 120.0f;
		item.crossSize = 70.0f;
		item.order = -1;
		box->setComponent<FlexItemInfo>(item);
	}

	// Box F: shrinks faster than the others when space runs out.
	{
		auto box = _demo->addChild(makeBox(Color::Teal_400, "F shrink:3"), ZOrder(1));
		FlexItemInfo item;
		item.basis = 140.0f;
		item.crossSize = 110.0f;
		item.shrink = 3.0f;
		box->setComponent<FlexItemInfo>(item);
	}

	updateControlLabels();

	return true;
}

basic2d::Layer *FlexboxLayout::addControlButton(StringView title, Function<void()> &&cb) {
	auto btn = _controls->addChild(Rc<ButtonWithLabel>::create(title, sp::move(cb)), ZOrder(1));

	// every control button shares the width evenly and may shrink when narrow
	FlexItemInfo item;
	item.basis = 110.0f;
	item.grow = 1.0f;
	item.shrink = 1.0f;
	FlexLayout::setItem(btn, item);

	return btn;
}

void FlexboxLayout::updateControlLabels() {
	auto info = _demoFlex->getInfo();
	if (!info) {
		return;
	}
	_btnDirection->setString(toString("Dir: ", directionName(info->direction)));
	_btnWrap->setString(toString("Wrap: ", wrapName(info->wrap)));
	_btnJustify->setString(toString("Just: ", justifyName(info->justifyContent)));
	_btnAlign->setString(toString("Align: ", alignName(info->alignItems)));
}

void FlexboxLayout::cycleDirection() {
	auto info = _demoFlex->getInfo();
	FlexDirection next = FlexDirection::Row;
	switch (info ? info->direction : FlexDirection::Row) {
	case FlexDirection::Row: next = FlexDirection::Column; break;
	case FlexDirection::Column: next = FlexDirection::RowReverse; break;
	case FlexDirection::RowReverse: next = FlexDirection::ColumnReverse; break;
	case FlexDirection::ColumnReverse: next = FlexDirection::Row; break;
	}
	_demoFlex->setDirection(next);
	updateControlLabels();
}

void FlexboxLayout::cycleWrap() {
	auto info = _demoFlex->getInfo();
	FlexWrap next = FlexWrap::NoWrap;
	switch (info ? info->wrap : FlexWrap::NoWrap) {
	case FlexWrap::NoWrap: next = FlexWrap::Wrap; break;
	case FlexWrap::Wrap: next = FlexWrap::WrapReverse; break;
	case FlexWrap::WrapReverse: next = FlexWrap::NoWrap; break;
	}
	_demoFlex->setWrap(next);
	updateControlLabels();
}

void FlexboxLayout::cycleJustify() {
	auto info = _demoFlex->getInfo();
	FlexJustify next = FlexJustify::FlexStart;
	switch (info ? info->justifyContent : FlexJustify::FlexStart) {
	case FlexJustify::FlexStart: next = FlexJustify::Center; break;
	case FlexJustify::Center: next = FlexJustify::FlexEnd; break;
	case FlexJustify::FlexEnd: next = FlexJustify::SpaceBetween; break;
	case FlexJustify::SpaceBetween: next = FlexJustify::SpaceAround; break;
	case FlexJustify::SpaceAround: next = FlexJustify::SpaceEvenly; break;
	case FlexJustify::SpaceEvenly: next = FlexJustify::FlexStart; break;
	}
	_demoFlex->setJustifyContent(next);
	updateControlLabels();
}

void FlexboxLayout::cycleAlign() {
	auto info = _demoFlex->getInfo();
	FlexAlign next = FlexAlign::Stretch;
	switch (info ? info->alignItems : FlexAlign::Stretch) {
	case FlexAlign::Stretch: next = FlexAlign::FlexStart; break;
	case FlexAlign::FlexStart: next = FlexAlign::Center; break;
	case FlexAlign::Center: next = FlexAlign::FlexEnd; break;
	default: next = FlexAlign::Stretch; break;
	}
	_demoFlex->setAlignItems(next);
	updateControlLabels();
}

void FlexboxLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	const auto cs = getContentSize();

	// control bar pinned to the top, spanning the full width
	_controls->setAnchorPoint(Anchor::BottomLeft);
	_controls->setPosition(Vec2(0.0f, cs.height - _controlsHeight));
	_controls->setContentSize(Size2(cs.width, _controlsHeight));

	// demonstration container fills the remaining area below the control bar
	_demo->setAnchorPoint(Anchor::BottomLeft);
	_demo->setPosition(Vec2(0.0f, 0.0f));
	_demo->setContentSize(Size2(cs.width, sprt::max(cs.height - _controlsHeight, 0.0f)));
}

} // namespace stappler::xenolith::app
