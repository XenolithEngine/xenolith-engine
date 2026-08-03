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

#include "LabelUpdateLayout.h"
#include "XLUiLayoutSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;
using ui::FlexAlign;
using ui::FlexItemInfo;
using ui::FlexLayoutInfo;
using ui::LayoutSystem;

namespace {

// short enough to fit on one line in both groups, long enough that growing it moves every box
static constexpr auto s_shortText = StringView("Ok");
static constexpr auto s_longText = StringView("Installed 1.2.3");

// main-size limit of the clamped group's chip: narrower than s_longText, so that text wraps and
// the chip has to grow along the cross axis instead
static constexpr float s_clampedMain = 90.0f;

} // namespace

bool LabelUpdateLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// One fixed-size container is what makes the test meaningful: the chains below are its flex
	// ITEMS, so their sizes are measured from their content instead of being set by hand.
	_column = addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));

	FlexLayoutInfo columnInfo;
	columnInfo.direction = ui::FlexDirection::Column;
	columnInfo.alignItems = FlexAlign::FlexStart;
	columnInfo.rowGap = 8.0f;
	columnInfo.padding = Padding(12.0f);
	_column->addSystem(Rc<LayoutSystem>::create(columnInfo));

	makeGroup(_free, StringView("free"), FlexItemInfo::Auto);
	makeGroup(_clamped, StringView("clamped"), s_clampedMain);

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.0f), [this] { runPhase2(); }, Rc<DelayTime>::create(1.0f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(1.0f), [this] { runPhase4(); }));

	return true;
}

void LabelUpdateLayout::makeGroup(Group &group, StringView name, float maxMain) {
	group.name = name;
	group.maxMain = maxMain;
	group.reference = makeChain(s_longText, maxMain, Color::Teal_400);
	group.shortReference = makeChain(s_shortText, maxMain, Color::Grey_600);
	group.subject = makeChain(s_shortText, maxMain, Color::Red_400);
}

LabelUpdateLayout::Chain LabelUpdateLayout::makeChain(StringView text, float maxMain,
		const Color4F &color) {
	Chain chain;

	// Two nested content-sized containers, so the test covers a chain and not just one hop: the
	// outer box hugs the chip, the chip hugs the label.
	chain.outer = _column->addChild(Rc<Layer>::create(Color::Grey_100), ZOrder(1));

	FlexLayoutInfo outerInfo;
	outerInfo.direction = ui::FlexDirection::Row;
	outerInfo.alignItems = FlexAlign::FlexStart;
	outerInfo.padding = Padding(6.0f);
	chain.outer->addSystem(Rc<LayoutSystem>::create(outerInfo));

	FlexItemInfo outerItem;
	outerItem.basis = FlexItemInfo::FitContent;
	LayoutSystem::setItem(chain.outer, outerItem);

	chain.chip = chain.outer->addChild(Rc<Layer>::create(color), ZOrder(1));

	FlexLayoutInfo chipInfo;
	chipInfo.direction = ui::FlexDirection::Row;
	chipInfo.alignItems = FlexAlign::FlexStart;
	chipInfo.padding = Padding(6.0f);
	chain.chip->addSystem(Rc<LayoutSystem>::create(chipInfo));

	FlexItemInfo chipItem;
	chipItem.basis = FlexItemInfo::FitContent;
	chipItem.maxMain = maxMain;
	LayoutSystem::setItem(chain.chip, chipItem);

	chain.label = chain.chip->addChild(Rc<Label>::create(), ZOrder(1));
	chain.label->setFontSize(20);
	chain.label->setString(text);
	chain.label->setColor(Color::White);

	FlexItemInfo labelItem;
	labelItem.basis = FlexItemInfo::FitContent;
	LayoutSystem::setItem(chain.label, labelItem);

	return chain;
}

void LabelUpdateLayout::compare(StringView phase, const Group &group, StringView what,
		const Size2 &subject, const Size2 &reference) {
	++_checks;
	// both chains shape the same text with the same style, so the sizes must be equal, not merely
	// close; one pixel of tolerance only absorbs the float arithmetic of the layout passes
	if (sprt::abs(subject.width - reference.width) > 1.0f
			|| sprt::abs(subject.height - reference.height) > 1.0f) {
		++_failures;
		log::source().error("LabelUpdateTest", phase, "/", group.name, ": ", what, " is ",
				subject.width, "x", subject.height, ", expected ", reference.width, "x",
				reference.height);
	}
}

void LabelUpdateLayout::runPhase1() {
	// the labels re-shape on their next visit; the resulting size change must travel
	// label -> chip -> outer -> column, though none of those containers owns its own size
	_free.subject.label->setString(s_longText);
	_clamped.subject.label->setString(s_longText);
}

void LabelUpdateLayout::runPhase2() {
	const Group *groups[] = {&_free, &_clamped};
	for (auto g : groups) {
		compare("grow", *g, "label", g->subject.label->getContentSize(),
				g->reference.label->getContentSize());
		compare("grow", *g, "chip", g->subject.chip->getContentSize(),
				g->reference.chip->getContentSize());
		compare("grow", *g, "outer", g->subject.outer->getContentSize(),
				g->reference.outer->getContentSize());

		// the label must fit inside the box that was measured for it - text drawn outside its own
		// chip was the visible symptom of the broken chain
		++_checks;
		if (g->subject.label->getContentSize().height
				> g->subject.chip->getContentSize().height + 1.0f) {
			++_failures;
			log::source().error("LabelUpdateTest", "grow/", g->name, ": label overflows its chip (",
					g->subject.label->getContentSize().height, " > ",
					g->subject.chip->getContentSize().height, ")");
		}
	}

	// the clamped group must really have wrapped, otherwise it is testing the same thing as the
	// free one and the cross-axis path stays uncovered
	++_checks;
	if (_clamped.reference.label->getLinesCount() < 2) {
		++_failures;
		log::source()
				.error("LabelUpdateTest",
						"grow/clamped: the reference text did not wrap - the cross-axis case is "
						"not " "covered, widen s_longText or lower s_clampedMain");
	}

	log::source().warn("LabelUpdateTest", "grow done: ", _checks, " checks, ", _failures,
			" failures; shrinking back");
}

void LabelUpdateLayout::runPhase3() {
	_free.subject.label->setString(s_shortText);
	_clamped.subject.label->setString(s_shortText);
}

void LabelUpdateLayout::runPhase4() {
	const Group *groups[] = {&_free, &_clamped};
	for (auto g : groups) {
		compare("shrink", *g, "label", g->subject.label->getContentSize(),
				g->shortReference.label->getContentSize());
		compare("shrink", *g, "chip", g->subject.chip->getContentSize(),
				g->shortReference.chip->getContentSize());
		compare("shrink", *g, "outer", g->subject.outer->getContentSize(),
				g->shortReference.outer->getContentSize());
	}

	log::source().warn("LabelUpdateTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void LabelUpdateLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	_column->setAnchorPoint(Vec2(0.0f, 1.0f));
	_column->setPosition(Vec2(24.0f, getWorkTop() - 16.0f));
	_column->setContentSize(Size2(420.0f, sprt::max(getWorkTop() - 48.0f, 0.0f)));
}

} // namespace stappler::xenolith::app
