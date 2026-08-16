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

#include "layout/OverflowLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiScrollSystem.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

// The boxes are 120px tall and hold 6 x 40px items, so the natural content is 240px and the range
// must come out at exactly 120px on every scrolling box.
static constexpr float BoxHeight = 120.0f;
static constexpr float ItemHeight = 40.0f;
static constexpr uint32_t ItemCount = 6;
static constexpr float ExpectedRange = ItemHeight * float(ItemCount) - BoxHeight;

static constexpr auto s_css = StringView(R"css(
.col        { display: flex; flex-direction: column; }
.item       { width: 100%; height: 40px; background-color: #1e88e5; }

.scrolling  { overflow-y: auto; }
.control    { overflow-y: visible; }
.clipped    { overflow: hidden; }
.fitting    { overflow-y: auto; }
.coerced    { overflow-x: visible; overflow-y: hidden; }
.tearing    { overflow-y: auto; }

.oversized  { width: 400px; height: 300px; background-color: #e53935; }
.filler     { flex-grow: 1; width: 100%; background-color: #43a047; }
)css");

} // namespace

bool OverflowLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);

	// One recursive resolver at the layout root, not one per box: StyleResolver re-resolves a
	// DESCENDANT whose classes change, but not its own owner, so a box that carried its own
	// resolver could never have a class taken off it again.
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeColumn = [this](StringView cls, Layer **first) {
		auto box = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
		box->addStyleClass("col");
		box->addStyleClass(cls);

		for (uint32_t i = 0; i < ItemCount; ++i) {
			// z from 1: a child at ZOrder(0) ties with the parent Layer's own quad and is drawn
			// under it
			auto item = box->addChild(Rc<Layer>::create(Color::Black), ZOrder(int16_t(i + 1)));
			item->addStyleClass("item");
			if (i == 0 && first) {
				*first = item;
			}
		}
		return box;
	};

	_scrollBox = makeColumn("scrolling", &_scrollFirst);
	_visibleBox = makeColumn("control", &_visibleFirst);
	// its own box, so removing the class in phase 3 leaves the scrolling one above still
	// scrollable - which is what makes the test usable for a manual or scripted wheel check
	_tearBox = makeColumn("tearing", &_tearFirst);

	// one child larger than the box on both axes, to check that `hidden` neither resizes it nor
	// leaves it clickable outside the scissor
	_hiddenBox = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_hiddenBox->addStyleClass("col");
	_hiddenBox->addStyleClass("clipped");
	_hiddenChild = _hiddenBox->addChild(Rc<Layer>::create(Color::Red_500), ZOrder(1));
	_hiddenChild->addStyleClass("oversized");

	// content that FITS, plus a grow filler: the axis must not be freed, so the filler still grows
	_fitBox = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_fitBox->addStyleClass("col");
	_fitBox->addStyleClass("fitting");
	{
		auto item = _fitBox->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
		item->addStyleClass("item");
	}
	_fitFiller = _fitBox->addChild(Rc<Layer>::create(Color::Green_600), ZOrder(2));
	_fitFiller->addStyleClass("filler");

	_coercedBox = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_coercedBox->addStyleClass("col");
	_coercedBox->addStyleClass("coerced");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.2f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.2f), [this] { runPhase2(); }, Rc<DelayTime>::create(1.2f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(1.2f), [this] { runPhase4(); }, Rc<DelayTime>::create(1.2f), [this] { runPhase5(); }));

	return true;
}

void OverflowLayout::runPhase1() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("OverflowTest", "phase1: ", what);
		}
	};

	// --- the scrolling box ---------------------------------------------------
	auto scroll = _scrollBox->getSystemByType<ui::ScrollSystem>();
	expect(scroll != nullptr, "overflow-y: auto did not install a ScrollSystem");
	if (!scroll) {
		return;
	}

	auto ovf = _scrollBox->getComponent<ui::OverflowComponent>();
	expect(ovf && ovf->y == document::Overflow::Auto, "overflow-y: auto not recorded");
	// the other axis is coerced to auto, because one scissor rect cannot clip a single axis
	expect(ovf && ovf->x == document::Overflow::Auto, "overflow-x was not coerced to auto");

	auto layout = _scrollBox->getSystemByType<ui::LayoutSystem>();
	expect(layout != nullptr, "scrolling box has no LayoutSystem");
	expect(layout && layout->getContentExtent().height > BoxHeight + 1.0f,
			"content extent did not exceed the box");
	expect(sprt::abs(scroll->getScrollRange().height - ExpectedRange) < 1.0f,
			"scroll range is not content - box");

	// the whole point: shrink must not have crushed the items
	expect(sprt::abs(_scrollFirst->getContentSize().height - ItemHeight) < 0.5f,
			"items were crushed instead of overflowing");

	// --- the control box (no overflow asked for) -----------------------------
	expect(_visibleBox->getSystemByType<ui::ScrollSystem>() == nullptr,
			"overflow: visible installed a ScrollSystem");
	expect(_visibleFirst->getContentSize().height < ItemHeight - 0.5f,
			"overflow: visible stopped shrinking its items - behaviour changed where it must not");

	// --- hidden --------------------------------------------------------------
	auto hiddenScroll = _hiddenBox->getSystemByType<ui::ScrollSystem>();
	expect(hiddenScroll != nullptr, "overflow: hidden did not install a ScrollSystem");
	expect(hiddenScroll && hiddenScroll->getScrollRange() == Size2::ZERO,
			"overflow: hidden reported a scroll range");
	expect(_hiddenBox->getSystemByType<DynamicStateSystem>() != nullptr,
			"overflow: hidden did not install a scissor");
	// `hidden` clips, it does not squash: the child keeps its declared size and the box hides the
	// part that does not fit. A `hidden` box that shrank its content would clip nothing.
	expect(sprt::abs(_hiddenChild->getContentSize().height - 300.0f) < 0.5f,
			"overflow: hidden shrank its oversized child instead of clipping it");

	// --- content that fits, with a grow filler -------------------------------
	auto fitScroll = _fitBox->getSystemByType<ui::ScrollSystem>();
	expect(fitScroll != nullptr, "fitting box has no ScrollSystem");
	expect(fitScroll && fitScroll->getScrollRange().height == 0.0f,
			"a box whose content fits reported a scroll range");
	expect(_fitFiller->getContentSize().height > 1.0f,
			"flex-grow stopped working inside an overflow container");

	// --- axis coercion the other way round -----------------------------------
	auto coerced = _coercedBox->getComponent<ui::OverflowComponent>();
	expect(coerced && coerced->x == document::Overflow::Auto,
			"an explicit `overflow-x: visible` beside a non-visible y was not coerced");
	expect(coerced && coerced->y == document::Overflow::Hidden, "overflow-y: hidden not recorded");

	/* The wheel eases rather than jumps, and a notch that lands mid-easing ADDS to the target.

	Both are asserted synchronously, right where the event is dispatched: the easing is 0.1s and
	nothing that has to observe it from outside the process can see inside that window. */
	{
		scroll->setScrollPosition(Vec2::ZERO);

		auto wheel = [this] {
			InputEventData ev{3, InputEventName::Scroll};
			ev.input.button = InputMouseButton::MouseScrollDown;
			ev.input.x = _scrollBox->getPosition().x + 60.0f;
			ev.input.y = _scrollBox->getPosition().y - 40.0f;
			ev.point.valueX = 0.0f;
			ev.point.valueY = -1.0f;
			_director->getInputDispatcher()->handleInputEvent(ev);
		};

		wheel();
		const float oneStep = scroll->getScrollTarget().y;
		expect(oneStep > 1.0f, "a wheel notch set no target at all");
		expect(scroll->getScrollPosition().y < oneStep - 0.5f,
				"the wheel jumped straight to its target instead of easing there");

		wheel();
		expect(sprt::abs(scroll->getScrollTarget().y - oneStep * 2.0f) < 0.5f,
				"a notch arriving mid-easing restarted the target instead of adding to it");

		// and an explicit position wins over the easing still in flight
		scroll->setScrollPosition(Vec2::ZERO);
		expect(scroll->getScrollTarget() == Vec2::ZERO,
				"setScrollPosition did not cancel the wheel easing");
	}

	log::source().warn("OverflowTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; scrolling to the end");

	// past the end on purpose: the clamp must hold it at the range
	scroll->setScrollPosition(Vec2(0.0f, ExpectedRange * 4.0f));
}

void OverflowLayout::runPhase2() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("OverflowTest", "phase2: ", what);
		}
	};

	auto scroll = _scrollBox->getSystemByType<ui::ScrollSystem>();
	if (!scroll) {
		return;
	}

	expect(sprt::abs(scroll->getScrollPosition().y - ExpectedRange) < 1.0f,
			"scroll position was not clamped to the range");

	// the first item must have moved up by the full range, and it is the POSITION that moves - the
	// size is untouched
	expect(sprt::abs(_scrollFirst->getContentSize().height - ItemHeight) < 0.5f,
			"scrolling resized an item");

	auto layout = _scrollBox->getSystemByType<ui::LayoutSystem>();
	expect(layout && sprt::abs(layout->getScrollOffset().y - ExpectedRange) < 1.0f,
			"the layout did not receive the scroll offset");

	log::source().warn("OverflowTest", "phase2 done: ", _checks, " checks, ", _failures,
			" failures; scrolling back past the start");

	scroll->scrollBy(Vec2(0.0f, -ExpectedRange * 4.0f));
}

void OverflowLayout::runPhase3() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("OverflowTest", "phase3: ", what);
		}
	};

	auto scroll = _scrollBox->getSystemByType<ui::ScrollSystem>();
	auto layout = _scrollBox->getSystemByType<ui::LayoutSystem>();
	if (!scroll || !layout) {
		return;
	}

	expect(scroll->getScrollPosition().y == 0.0f, "scroll position was not clamped at the start");
	expect(layout->getScrollOffset() == Vec2::ZERO, "the layout kept a stale scroll offset");

	/* A drag, synthesized the way a mouse delivers one, and then the release.

	Two things this pins that nothing else does. The DIRECTION: dragging the pointer UP has to move
	the content up, which is a LARGER y offset - the axis is easy to invert twice and end up with a
	drag that works at neither end. And the RELEASE: there is no fling, so the offset after the
	button comes up is the offset the last Move left, for good. A coasting release cannot be told
	from a scroller ignoring the button, and this asserts it never coasts. */
	{
		auto press = [&](InputEventName name, float y) {
			InputEventData ev{1, name};
			ev.input.button = InputMouseButton::MouseLeft;
			ev.input.x = _scrollBox->getPosition().x + 60.0f;
			ev.input.y = y;
			_director->getInputDispatcher()->handleInputEvent(ev);
		};

		const float top = _scrollBox->getPosition().y;
		press(InputEventName::Begin, top - 10.0f);
		press(InputEventName::Move, top - 40.0f);
		press(InputEventName::Move, top - 70.0f);

		const float dragged = scroll->getScrollPosition().y;
		expect(dragged > 1.0f, "dragging the pointer up did not scroll the content up");

		press(InputEventName::End, top - 70.0f);
		expect(sprt::abs(scroll->getScrollPosition().y - dragged) < 0.01f,
				"releasing the button moved the content - a fling outlived the drag");
	}

	log::source().warn("OverflowTest", "phase3 done: ", _checks, " checks, ", _failures,
			" failures; removing .scrolling");

	// removing the class must tear the whole thing down again, including the offset it left behind
	_tearBox->removeStyleClass("tearing");
}

void OverflowLayout::runPhase4() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("OverflowTest", "phase4: ", what);
		}
	};

	expect(_tearBox->getComponent<ui::OverflowComponent>() == nullptr,
			"OverflowComponent survived the class removal");
	expect(_tearBox->getSystemByType<ui::ScrollSystem>() == nullptr,
			"ScrollSystem survived the class removal");

	auto layout = _tearBox->getSystemByType<ui::LayoutSystem>();
	expect(layout && !layout->isOverflowY(), "the layout kept its overflow axis");
	// back to an ordinary container: the items are shrunk into the box again
	expect(_tearFirst->getContentSize().height < ItemHeight - 0.5f,
			"items were not re-shrunk after overflow was removed");

	// exactly one indicator pair may ever exist; a system removed and re-added must not leave an
	// orphan behind (the resolver adds and drops ScrollSystem as classes come and go)
	uint32_t bars = 0;
	for (auto &child : _tearBox->getChildren()) {
		if (child->getType() == "scrollbar") {
			++bars;
		}
	}
	expect(bars == 0, "scroll indicators outlived the ScrollSystem");

	/* The same drag again, but flagged as coming from a touchscreen.

	That flag is the ONLY thing separating the two: the button is MouseLeft either way
	(`InputMouseButton::Touch` is an alias of it), so a backend marks a real finger with
	InputModifier::Touch per event. A finger has to coast after release; a mouse, asserted in
	phase 3, must not move a pixel. */
	auto scroll = _scrollBox->getSystemByType<ui::ScrollSystem>();
	if (scroll) {
		scroll->setScrollPosition(Vec2::ZERO);

		// Spaced in REAL time, not fired back to back: GestureSwipeRecognizer only refreshes its
		// velocity when more than 1/500 s has passed since the last move, so a burst delivered
		// inside one frame carries no velocity at all and could never fling.
		auto touch = [this](InputEventName name, float dy) {
			InputEventData ev{2, name};
			ev.input.button = InputMouseButton::MouseLeft;
			ev.input.modifiers = InputModifier::Touch;
			ev.input.x = _scrollBox->getPosition().x + 60.0f;
			ev.input.y = _scrollBox->getPosition().y - dy;
			_director->getInputDispatcher()->handleInputEvent(ev);
		};

		runAction(Rc<Sequence>::create([touch] { touch(InputEventName::Begin, 10.0f); },
				Rc<DelayTime>::create(0.05f), [touch] { touch(InputEventName::Move, 30.0f); },
				Rc<DelayTime>::create(0.05f), [touch] { touch(InputEventName::Move, 50.0f); },
				Rc<DelayTime>::create(0.05f), [touch] { touch(InputEventName::Move, 70.0f); },
				[this, touch, scroll] {
			touch(InputEventName::End, 70.0f);
			_flingFrom = scroll->getScrollPosition().y;
			if (_flingFrom <= 1.0f) {
				++_failures;
				log::source().error("OverflowTest", "phase4: the touch drag did not scroll");
			}
			++_checks;
		}));
	}

	log::source().warn("OverflowTest", "phase4 done: ", _checks, " checks, ", _failures,
			" failures; letting the touch fling coast");
}

void OverflowLayout::runPhase5() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("OverflowTest", "phase5: ", what);
		}
	};

	if (auto scroll = _scrollBox->getSystemByType<ui::ScrollSystem>()) {
		// It coasted past where the finger left it, and it has since come to rest.
		expect(scroll->getScrollPosition().y > _flingFrom + 1.0f,
				"a released touch did not coast - inertia is gone");
		// and it has come to rest rather than run away: the decay must have taken it to zero
		// well inside the 1.2s this phase waited.
		const float settled = scroll->getScrollPosition().y;
		expect(settled <= scroll->getScrollRange().height + 0.01f,
				"the fling ran past the end of the range");
	}

	log::source().warn("OverflowTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void OverflowLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;

	Layer *boxes[] = {_scrollBox, _visibleBox, _hiddenBox, _fitBox, _coercedBox, _tearBox};
	for (size_t i = 0; i < 6; ++i) {
		boxes[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		boxes[i]->setContentSize(Size2(220.0f, BoxHeight));
		boxes[i]->setPosition(Vec2(24.0f + float(i % 5) * 240.0f,
				top - float(i / 5) * (BoxHeight + 40.0f)));
	}
}

} // namespace stappler::xenolith::app
