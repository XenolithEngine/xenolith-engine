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

#include "layout/CssFlowLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

// The clamp row is 600px wide with 3 growing items: without clamps each would settle at 200px.
// `min-width` pushes one above that share, `max-width` holds another below it.
static constexpr float ClampRowWidth = 600.0f;
static constexpr float ClampMin = 260.0f;
static constexpr float ClampMax = 120.0f;

// The flow row is 600px wide with two items sharing it equally; the absolute overlay must not
// count as a third one.
static constexpr float FlowRowWidth = 600.0f;

// The late row is the same width, with one 200px item in it from the start. The second child is
// added a second later and has to end up with the other 400.
static constexpr float LateFixedWidth = 200.0f;

static constexpr auto s_css = StringView(R"css(
.clamp-row {
	display: flex;
	flex-direction: row;
	column-gap: 0px;
}
.clamp-row > .item { flex-grow: 1; flex-shrink: 1; flex-basis: 0px; height: 60px; }
.clamp-row > .min { min-width: 260px; }
.clamp-row > .max { max-width: 120px; }

.flow-row {
	display: flex;
	flex-direction: row;
	column-gap: 0px;
}
.flow-row > .item { flex-grow: 1; flex-shrink: 1; flex-basis: 0px; height: 60px; }
.flow-row > .overlay {
	position: absolute;
	left: 40px;
	top: 10px;
	width: 100px;
	height: 40px;
}

/* The newcomer declares NO size of its own - only that it grows. A Layer's content size is zero
   until something gives it one, so if the container is never told that this child's item terms
   arrived, zero is exactly what it keeps. */
.late-row {
	display: flex;
	flex-direction: row;
	column-gap: 0px;
}
.late-row > .fixed { flex-basis: 200px; height: 60px; }
.late-row > .late { flex-grow: 1; flex-basis: 0px; height: 60px; }
/* toggled in phase 2. It changes ONE item term of ONE child that is already on screen and already
   laid out, and it changes nothing else - no size, no margin, no visibility. The only way the row
   can answer it is if a stylesheet writing an item component tells the CONTAINER. */
.late-row > .fixed.wide { flex-grow: 1; }

.stack-row {
	display: flex;
	flex-direction: row;
	column-gap: 0px;
}
/* the negative margin makes each box overlap the previous one, so the draw order is visible */
.stack-row > .box { width: 160px; height: 60px; margin-left: -60px; }
/* placed last, drawn first (ends up under everything) */
.stack-row > .a { background-color: #e53935; order: 3; -xl-z-order: 1; }
/* placed first, drawn last (ends up on top) */
.stack-row > .b { background-color: #43a047; order: 1; -xl-z-order: 3; }
.stack-row > .c { background-color: #1e88e5; order: 2; -xl-z-order: 2; }
)css");

} // namespace

bool CssFlowLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeItem = [](Layer *row, const Color4F &c, StringView extraClass) {
		auto item = row->addChild(Rc<Layer>::create(c), ZOrder(1));
		item->addStyleClass("item");
		if (!extraClass.empty()) {
			item->addStyleClass(extraClass);
		}
		return item;
	};

	_clampRow = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	_clampRow->addStyleClass("clamp-row");
	_clampMin = makeItem(_clampRow, Color::Red_400, "min");
	_clampMax = makeItem(_clampRow, Color::Blue_400, "max");
	_clampFree = makeItem(_clampRow, Color::Green_400, StringView());

	_flowRow = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	_flowRow->addStyleClass("flow-row");
	_flowFirst = makeItem(_flowRow, Color::Amber_400, StringView());
	_flowSecond = makeItem(_flowRow, Color::Purple_400, StringView());

	// the overlay is a CHILD of the flex container, which is exactly what used to be impossible:
	// it counted as an item, ate a third of the row and got placed by the container
	_flowOverlay = _flowRow->addChild(Rc<Layer>::create(Color::Black), ZOrder(2));
	_flowOverlay->addStyleClass("overlay");

	// Only the fixed half exists now. The other one is added in phase 1, once this row has been
	// laid out at least once - which is the whole point of it.
	_lateRow = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	_lateRow->addStyleClass("late-row");
	_lateFixed = _lateRow->addChild(Rc<Layer>::create(Color::Teal_400), ZOrder(1));
	_lateFixed->addStyleClass("fixed");

	// Added in a, b, c order; the stylesheet then places them b, c, a and draws them a, c, b.
	auto makeStackBox = [this](StringView cls) {
		auto box = _stackRow->addChild(Rc<Layer>::create(Color::White), ZOrder(0));
		box->addStyleClass("box");
		box->addStyleClass(cls);
		return box;
	};

	_stackRow = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
	_stackRow->addStyleClass("stack-row");
	_stackA = makeStackBox("a");
	_stackB = makeStackBox("b");
	_stackC = makeStackBox("c");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); }));

	return true;
}

void CssFlowLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("CssFlowTest", what);
	}
}

void CssFlowLayout::expectNear(StringView what, float actual, float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 1.0f) {
		++_failures;
		log::source().error("CssFlowTest", what, " is ", actual, ", expected ", expected);
	}
}

void CssFlowLayout::runPhase1() {
	// min-/max-width bound the flexed main size; the unclamped item absorbs what is left, so the
	// three still fill the row exactly
	expectNear("min-width item width", _clampMin->getContentSize().width, ClampMin);
	expectNear("max-width item width", _clampMax->getContentSize().width, ClampMax);
	expectNear("unclamped item width", _clampFree->getContentSize().width,
			ClampRowWidth - ClampMin - ClampMax);

	// the absolute overlay is not an item: the two real ones split the row in half, not in thirds
	expectNear("first in-flow item width", _flowFirst->getContentSize().width, FlowRowWidth / 2.0f);
	expectNear("second in-flow item width", _flowSecond->getContentSize().width,
			FlowRowWidth / 2.0f);
	expect(_flowOverlay->getComponent<ui::OutOfFlowComponent>() != nullptr,
			"the absolute overlay was not marked out of flow");

	// ...and it keeps the size and place the stylesheet gave it, instead of the container's
	expectNear("overlay width", _flowOverlay->getContentSize().width, 100.0f);
	expectNear("overlay height", _flowOverlay->getContentSize().height, 40.0f);
	expectNear("overlay x", _flowOverlay->getPosition().x, 40.0f);
	// engine Y is up: `top: 10px` puts the node's top edge 10px below the container's top edge
	expectNear("overlay y", _flowOverlay->getPosition().y,
			_flowRow->getContentSize().height - 10.0f);

	// `order` decides where a box lands in the row: b (order 1), then c (2), then a (3), whatever
	// the children's own sequence is
	expect(_stackB->getPosition().x < _stackC->getPosition().x
					&& _stackC->getPosition().x < _stackA->getPosition().x,
			"the row was not placed in `order` sequence (b, c, a)");

	// `-xl-z-order` decides which one covers which: a (1) at the bottom, then c (2), then b (3).
	// The children list is kept sorted by z-order, and that IS the draw order.
	expectNear("box a z-order", float(_stackA->getLocalZOrder().get()), 1.0f);
	expectNear("box c z-order", float(_stackC->getLocalZOrder().get()), 2.0f);
	expectNear("box b z-order", float(_stackB->getLocalZOrder().get()), 3.0f);

	auto children = _stackRow->getChildren();
	expect(children.size() == 3 && children.at(0) == _stackA && children.at(1) == _stackC
					&& children.at(2) == _stackB,
			"the children are not in draw (z-order) sequence a, c, b");

	// the two sequences really are different - if they matched, the test would prove nothing
	expect(children.at(0) != _stackB, "placement and draw order came out identical");

	/* THE LATE CHILD, added to a row that has been laid out for a second already.

	Nothing sizes it here and nothing is meant to: the sheet says `flex-grow: 1` and says nothing
	else, so the only way it can come out at anything but zero is if the container re-lays-out after
	the style pass wrote its item terms. Half a second is many frames; a failure here is not a race
	that needed longer. */
	expectNear("the late row starts with only its fixed item", _lateFixed->getContentSize().width,
			LateFixedWidth);

	_lateGrown = _lateRow->addChild(Rc<Layer>::create(Color::Pink_400), ZOrder(1));
	_lateGrown->addStyleClass("late");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.5f), [this] { runPhase2(); }));
}

void CssFlowLayout::runPhase2() {
	/* Two assertions rather than one, because the two halves of this fail differently and a single
	width would not say which.

	The sheet's word reaching the CHILD is the first: `flex-grow: 1` on the item component. The
	container being TOLD is the second: the width. A grow of 1 with a width of 0 is the defect this
	phase exists for - the terms arrived and nobody re-laid-out. A grow of 0 is a different failure
	entirely, and means the rule stopped matching. */
	auto item = ui::LayoutSystem::getItem(_lateGrown);
	expect(item != nullptr && item->grow == 1.0f,
			"the stylesheet did not reach the late child's flex item terms");
	expectNear("late child width", _lateGrown->getContentSize().width,
			FlowRowWidth - LateFixedWidth);
	expectNear("the fixed item kept its width", _lateFixed->getContentSize().width, LateFixedWidth);

	/* And now the same claim without the timing.

	Phase 1 adds a child, which is a structural change and dirties a good deal on its own; a row
	that came out right there has not necessarily heard the sheet. This toggles a class on a child
	that has been laid out for a second and a half, and the class carries ONE declaration - a
	`flex-grow`. Nothing else about the node moves, so nothing else can dirty the row: if the
	container is not told that a child's item terms changed, these widths do not move either. */
	_lateFixed->addStyleClass("wide");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.5f), [this] { runPhase3(); }));
}

void CssFlowLayout::runPhase3() {
	// 600 of row, 200 of it the fixed item's basis, 400 free - and now two items with a grow of 1
	// to share it, so 200 each on top of what they had.
	expectNear("the fixed item grew when its class did", _lateFixed->getContentSize().width,
			LateFixedWidth + 200.0f);
	expectNear("...and the late child gave up exactly that much",
			_lateGrown->getContentSize().width, 200.0f);

	log::source().warn("CssFlowTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void CssFlowLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;

	_clampRow->setAnchorPoint(Vec2(0.0f, 1.0f));
	_clampRow->setPosition(Vec2(40.0f, top));
	_clampRow->setContentSize(Size2(ClampRowWidth, 60.0f));

	_flowRow->setAnchorPoint(Vec2(0.0f, 1.0f));
	_flowRow->setPosition(Vec2(40.0f, top - 120.0f));
	_flowRow->setContentSize(Size2(FlowRowWidth, 60.0f));

	_lateRow->setAnchorPoint(Vec2(0.0f, 1.0f));
	_lateRow->setPosition(Vec2(40.0f, top - 240.0f));
	_lateRow->setContentSize(Size2(FlowRowWidth, 60.0f));

	_stackRow->setAnchorPoint(Vec2(0.0f, 1.0f));
	_stackRow->setPosition(Vec2(40.0f, top - 360.0f));
	_stackRow->setContentSize(Size2(FlowRowWidth, 60.0f));
}

} // namespace stappler::xenolith::app
