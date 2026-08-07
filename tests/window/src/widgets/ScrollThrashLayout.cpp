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

#include "widgets/ScrollThrashLayout.h"
#include "XL2dScrollController.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;
using basic2d::ScrollController;
using basic2d::ScrollView;

namespace {

static constexpr size_t RowCount = 60;
static constexpr float RowHeight = 44.0f;

// Rows that can never satisfy the item: consecutive builds alternate between two heights, so
// every rebuild disagrees with the size the item currently holds. resizeItem then shifts the whole
// list, the window moves far enough to drop and re-add rows, and those rebuild at the other height
// again. A real app reaches the same state through a row whose label re-shapes to a different
// height depending on when it is constructed.
static constexpr float RowOvershootA = 0.0f;
static constexpr float RowOvershootB = 40.0f;

// Generous, but far below what an unbounded pass reaches: the window holds ~15 rows, and a bounded
// pass rebuilds them at most a dozen times.
static constexpr uint32_t BuiltRowsLimit = 4'000;

} // namespace

bool ScrollThrashLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_scroll = addChild(Rc<ScrollView>::create(ScrollView::Vertical), ZOrder(1));
	_scroll->setAnchorPoint(Vec2(0.0f, 1.0f));
	_scroll->setIndicatorColor(Color::Grey_500);

	// the default (nodes are destroyed as they leave the window) is the case that used to run away;
	// setKeepNodes(true) is the workaround this test exists to make unnecessary
	_controller = _scroll->setController(Rc<ScrollController>::create());

	for (size_t i = 0; i < RowCount; ++i) {
		_controller->addItem([this, i](const ScrollController::Item &item) -> Rc<Node> {
			const float overshoot = (_builtRows % 2) ? RowOvershootB : RowOvershootA;
			++_builtRows;
			auto row = Rc<Layer>::create((i % 2) ? Color::Blue_200 : Color::Blue_400);
			row->setContentSize(Size2(item.size.width, RowHeight + overshoot));

			// a label so the rows are readable on screen; it plays no part in the sizing
			auto label = row->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
			label->setFontSize(18);
			label->setString(string::toString<Interface>("row ", i));
			label->setColor(Color::Black);
			label->setAnchorPoint(Vec2(0.0f, 0.0f));

			return row;
		}, RowHeight);
	}

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.5f), [this] { runPhase2(); }));

	return true;
}

void ScrollThrashLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("ScrollThrashTest", what);
	}
}

void ScrollThrashLayout::runPhase1() {
	// Reaching this at all is the main result: an unbounded windowing pass never returns from the
	// frame that started it, so no scheduled phase would ever run.
	expect(_builtRows < BuiltRowsLimit,
			string::toString<Interface>("built ", _builtRows,
					" rows before the first phase, limit ", BuiltRowsLimit));

	// only the rows the window can hold may be alive at once - the point of the virtualization
	expect(_controller->getNodes().size() < RowCount,
			string::toString<Interface>("all ", RowCount,
					" rows are live; nothing was virtualized"));

	_builtAtPhase1 = _builtRows;

	// scrolling is what makes rows leave and re-enter the window, i.e. what drives the rebuild
	_scroll->setScrollRelativePosition(1.0f);

	log::source().warn("ScrollThrashTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures, ", _builtRows, " rows built; scrolling to the end");
}

void ScrollThrashLayout::runPhase2() {
	expect(_builtRows - _builtAtPhase1 < BuiltRowsLimit,
			string::toString<Interface>("scrolling built ", _builtRows - _builtAtPhase1,
					" rows, limit ", BuiltRowsLimit));
	expect(_controller->getNodes().size() < RowCount,
			string::toString<Interface>("all ", RowCount, " rows are live after scrolling"));

	_builtAtPhase2 = _builtRows;

	// Sweep the list from here on. This is what a memory profile needs - rows leave and re-enter
	// the window continuously, every build disagreeing with its item again - and phase 3 turns it
	// into an assertion instead of something one has to watch by hand.
	runAction(Rc<RepeatForever>::create(Rc<Sequence>::create(
			[this] { _scroll->setScrollRelativePosition(0.0f); }, Rc<DelayTime>::create(0.4f),
			[this] { _scroll->setScrollRelativePosition(1.0f); }, Rc<DelayTime>::create(0.4f))));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(4.0f), [this] { runPhase3(); }));
}

void ScrollThrashLayout::runPhase3() {
	const uint32_t built = _builtRows - _builtAtPhase2;

	// five sweeps over a 60-row list rebuild the window's worth of rows a few times each; a pass
	// that re-enters instead of settling reaches the limit within one of them
	expect(built < BuiltRowsLimit,
			string::toString<Interface>("sweeping built ", built, " rows, limit ", BuiltRowsLimit));
	expect(_controller->getNodes().size() < RowCount,
			string::toString<Interface>("all ", RowCount, " rows are live after sweeping"));

	log::source().warn("ScrollThrashTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures, ", _builtRows, " rows built in total (", built, " while sweeping)");
}

void ScrollThrashLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	_scroll->setPosition(Vec2(24.0f, getWorkTop() - 16.0f));
	_scroll->setContentSize(
			Size2(sprt::min(cs.width - 48.0f, 420.0f), sprt::max(getWorkTop() - 64.0f, 0.0f)));
}

} // namespace stappler::xenolith::app
