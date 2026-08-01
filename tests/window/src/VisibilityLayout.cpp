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

#include "VisibilityLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr auto s_css = StringView(R"css(
.row { display: flex; flex-direction: row; column-gap: 8px; }
.item { width: 80px; height: 60px; background-color: #1e88e5; }
.gone { display: none; }
.ghost { visibility: hidden; }
)css");

} // namespace

bool VisibilityLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);

	auto makeRow = [this](Layer **mid, Layer **last, StringView midClass) {
		auto row = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
		row->addStyleClass("row");
		row->addSystem(Rc<ui::StyleResolver>::create(true));

		Layer *items[3] = {nullptr, nullptr, nullptr};
		for (auto &it : items) {
			it = row->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
			it->addStyleClass("item");
		}
		items[1]->addStyleClass(midClass);
		*mid = items[1];
		*last = items[2];
		return row;
	};

	_rowNone = makeRow(&_midNone, &_lastNone, "gone");
	_rowHidden = makeRow(&_midHidden, &_lastHidden, "ghost");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.2f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.2f), [this] { runPhase2(); }));

	return true;
}

void VisibilityLayout::runPhase1() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("VisibilityTest", "phase1: ", what);
		}
	};

	auto cNone = _midNone->getComponent<VisibilityComponent>();
	expect(cNone && cNone->displayNone && !cNone->visibilityHidden,
			"display:none item has no proper VisibilityComponent");
	auto cHidden = _midHidden->getComponent<VisibilityComponent>();
	expect(cHidden && cHidden->visibilityHidden && !cHidden->displayNone,
			"visibility:hidden item has no proper VisibilityComponent");

	// the explicit setVisible state is untouched, but the effective visibility is off
	expect(_midNone->isVisible() && !_midNone->isEffectivelyVisible(),
			"display:none item effective visibility mismatch");
	expect(_midHidden->isVisible() && !_midHidden->isEffectivelyVisible(),
			"visibility:hidden item effective visibility mismatch");

	// display:none collapses the box, visibility:hidden keeps it
	expect(!_midNone->isDisplayed(), "display:none item still occupies a layout box");
	expect(_midHidden->isDisplayed(), "visibility:hidden item lost its layout box");

	const float xNone = _lastNone->getPosition().x;
	const float xHidden = _lastHidden->getPosition().x;
	expect(xNone + 60.0f < xHidden,
			"third item did not shift left when the middle one is display:none");

	log::source().warn("VisibilityTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; removing .gone/.ghost");

	// un-hide through the styling protocol: the hidden nodes' own data phases keep running,
	// so the class change must reach the resolver and remove the components
	_midNone->removeStyleClass("gone");
	_midHidden->removeStyleClass("ghost");
}

void VisibilityLayout::runPhase2() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("VisibilityTest", "phase2: ", what);
		}
	};

	expect(_midNone->getComponent<VisibilityComponent>() == nullptr,
			"display:none item still carries VisibilityComponent after class removal");
	expect(_midHidden->getComponent<VisibilityComponent>() == nullptr,
			"visibility:hidden item still carries VisibilityComponent after class removal");
	expect(_midNone->isEffectivelyVisible() && _midNone->isDisplayed(),
			"display:none item did not become visible again");
	expect(_midHidden->isEffectivelyVisible(),
			"visibility:hidden item did not become visible again");

	// with all three items in flow, both rows place the third item identically
	const float xNone = _lastNone->getPosition().x;
	const float xHidden = _lastHidden->getPosition().x;
	expect(sprt::abs(xNone - xHidden) < 1.0f,
			"rows differ after both middle items were restored");

	log::source().warn("VisibilityTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void VisibilityLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	const float top = getWorkTop() - 180.0f;

	Layer *rows[] = {_rowNone, _rowHidden};
	for (size_t i = 0; i < 2; ++i) {
		rows[i]->setAnchorPoint(Vec2(0.0f, 0.0f));
		rows[i]->setContentSize(Size2(560.0f, 80.0f));
		rows[i]->setPosition(Vec2(24.0f, top - float(i) * 120.0f));
	}
}

} // namespace stappler::xenolith::app
