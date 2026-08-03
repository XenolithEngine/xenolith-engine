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

#include "AutoMarginLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr float RowWidth = 520.0f;
static constexpr float RowHeight = 70.0f;
static constexpr float BoxWidth = 90.0f;
static constexpr float BoxHeight = 30.0f;

static constexpr auto s_css = StringView(R"css(
.row {
	display: flex;
	flex-direction: row;
	column-gap: 0px;
}
.row > .box { width: 90px; height: 30px; }

/* main axis: the last item takes all the leftover space in front of it */
.push > .last { margin-left: auto; }

/* main axis: equal shares on both sides centre the item */
.centre > .only { margin-left: auto; margin-right: auto; }

/* cross axis: the row aligns to the top, this box centres itself anyway */
.cross { align-items: flex-start; }
.cross > .self { margin-top: auto; margin-bottom: auto; }

/* cross axis: an auto margin means there is space to take, so stretch must not eat it */
.stretch { align-items: stretch; }
.stretch > .self { margin-top: auto; }
)css");

} // namespace

Layer *AutoMarginLayout::makeRow(StringView cls) {
	auto row = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	row->addStyleClass("row");
	row->addStyleClass(cls);
	return row;
}

bool AutoMarginLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeBox = [](Layer *row, const Color4F &c, StringView extra) {
		auto box = row->addChild(Rc<Layer>::create(c), ZOrder(1));
		box->addStyleClass("box");
		if (!extra.empty()) {
			box->addStyleClass(extra);
		}
		return box;
	};

	_pushRow = makeRow("push");
	_pushFirst = makeBox(_pushRow, Color::Red_400, StringView());
	_pushLast = makeBox(_pushRow, Color::Blue_400, "last");

	_centreRow = makeRow("centre");
	_centreOnly = makeBox(_centreRow, Color::Green_400, "only");

	_crossRow = makeRow("cross");
	_crossPlain = makeBox(_crossRow, Color::Amber_400, StringView());
	_crossAuto = makeBox(_crossRow, Color::Purple_400, "self");

	_stretchRow = makeRow("stretch");
	_stretchPlain = makeBox(_stretchRow, Color::Teal_400, StringView());
	_stretchAuto = makeBox(_stretchRow, Color::Brown_400, "self");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); }));

	return true;
}

void AutoMarginLayout::expectNear(StringView what, float actual, float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 1.0f) {
		++_failures;
		log::source().error("AutoMarginTest", what, " is ", actual, ", expected ", expected);
	}
}

void AutoMarginLayout::runPhase1() {
	// main axis, one auto margin: it swallows the whole remainder, so the item ends flush right
	expectNear("pushed row: first box x", _pushFirst->getPosition().x, 0.0f);
	expectNear("pushed row: last box x", _pushLast->getPosition().x, RowWidth - BoxWidth);

	// main axis, two auto margins: half each, so the item is centred
	expectNear("centred row: box x", _centreOnly->getPosition().x, (RowWidth - BoxWidth) / 2.0f);

	// cross axis: the plain box obeys align-items: flex-start, the other one centres itself.
	// Positions are row-local and engine Y is up, so flex-start puts the plain box at the TOP of
	// the row, and the auto-margin box sits exactly half of the leftover cross space lower.
	expectNear("cross row: plain box y", _crossPlain->getPosition().y, RowHeight - BoxHeight);
	expectNear("cross row: auto box y", _crossAuto->getPosition().y,
			_crossPlain->getPosition().y - (RowHeight - BoxHeight) / 2.0f);

	// an auto cross margin outranks align-items: stretch - the box keeps its CSS height while its
	// sibling fills the line
	expectNear("stretch row: plain box height", _stretchPlain->getContentSize().height, RowHeight);
	expectNear("stretch row: auto box height", _stretchAuto->getContentSize().height, BoxHeight);
	// with a single auto margin on the top side, the free space goes above it: it sits at the
	// bottom of the line
	expectNear("stretch row: auto box y", _stretchAuto->getPosition().y, 0.0f);

	log::source().warn("AutoMarginTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void AutoMarginLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 24.0f;

	Layer *rows[] = {_pushRow, _centreRow, _crossRow, _stretchRow};
	for (size_t i = 0; i < 4; ++i) {
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(40.0f, top - float(i) * (RowHeight + 20.0f)));
		rows[i]->setContentSize(Size2(RowWidth, RowHeight));
	}
}

} // namespace stappler::xenolith::app
