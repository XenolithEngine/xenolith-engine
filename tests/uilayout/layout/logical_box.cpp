/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

// The logical box properties, resolved through the real cascade.
//
// This is the suite that guards decision Р1: `direction: rtl` must move `padding-inline-start` and
// must NOT move `padding-left`. An engine that flipped the physical properties would be
// non-standard, every stylesheet written against it would be wrong on the web, and nothing else
// here would notice - so the assertion is made against the placement, at the layer a person sees.

#include "SPCommon.h"

#include "XLNode.h"
#include "XLInheritedStyle.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleSheet.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"
#include "XLUiLayoutFlex.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::test::check;
using stappler::test::checkNear;

namespace {

/* A styled tree, driven by the real resolver: a root carrying the sheet, one child with a class.
Everything the studio does to a node goes through this path, so what it proves is what ships. */
struct StyledFixture {
	Rc<Node> root;
	Rc<Node> box;
	Rc<Node> item;

	explicit StyledFixture(StringView css) {
		root = Rc<Node>::create();
		root->setContentSize(Size2(300.0f, 100.0f));

		/* The sheet is installed in a SECOND step on purpose. A StyleResolver skips its work when
		the style system's id and version are what it last saw, and with no Scene to enter, a
		freshly added system leaves both at zero - which is exactly what a fresh resolver already
		believes. `setStyleSheet` bumps the version, which is the same signal a real reload sends. */
		auto sys = root->addSystem(Rc<StyleSystem>::create());
		sys->setStyleSheet(Rc<StyleSheet>::create(css));

		box = root->addChild(Rc<Node>::create());
		box->setName("box");
		box->setContentSize(Size2(300.0f, 100.0f));
		box->addStyleClass("box");
		box->addSystem(Rc<StyleResolver>::create());
		box->addSystem(Rc<LayoutSystem>::create());

		item = box->addChild(Rc<Node>::create());
		item->setName("item");
		item->setContentSize(Size2(40.0f, 20.0f));
		item->addStyleClass("item");
		item->addSystem(Rc<StyleResolver>::create());
	}

	// Resolve the styles, then lay out. Two passes because the resolver writes the components the
	// layout reads, exactly as the frame does.
	void run() {
		for (auto &n : {box, item}) {
			if (auto r = n->getSystemByType<StyleResolver>()) {
				r->apply();
			}
		}
		if (auto l = box->getSystemByType<LayoutSystem>()) {
			l->apply();
		}
	}

	float itemX() const { return item->getPosition().x; }
};

} // namespace

void performLogicalBoxTests() {
	sprt::cout << "\n== ui layout: logical box properties ==\n";

	/* THE ONE THAT MATTERS. Both declarations are on the same box: a physical padding of 10 and a
	logical one of 30. Under ltr they agree and the logical one wins; under rtl the logical one
	moves to the right edge and the physical one stays where it was written. */
	{
		StyledFixture ltr(R"Css(
			.box { display: flex; flex-direction: row; direction: ltr;
			       padding-left: 10px; padding-inline-start: 30px; }
		)Css");
		ltr.run();
		checkNear(ltr.itemX(), 30.0f,
				"logical: ltr - padding-inline-start resolves to the left edge");

		StyledFixture rtl(R"Css(
			.box { display: flex; flex-direction: row; direction: rtl;
			       padding-left: 10px; padding-inline-start: 30px; }
		)Css");
		rtl.run();
		// The item is flush against the inline start, which under rtl is the RIGHT edge, inset by
		// the 30 the logical property asked for. And `padding-left: 10px` is still on the left,
		// where it was written - it simply is not the edge the item is against.
		checkNear(rtl.itemX(), 300.0f - 30.0f - 40.0f,
				"logical: rtl - padding-inline-start resolves to the right edge");
	}

	/* Р1, stated as its own check: a sheet that mentions no logical property at all must lay out
	IDENTICALLY under both directions, because `padding-left` is physical in CSS and stays so. The
	only thing `direction` changes here is the flow, and one item flush at the start is at the
	other end - so the padding is asserted by measuring from the edge it belongs to. */
	{
		StyledFixture ltr(R"Css(
			.box { display: flex; flex-direction: row; direction: ltr;
			       padding-left: 25px; padding-right: 5px; }
		)Css");
		ltr.run();
		checkNear(ltr.itemX(), 25.0f, "logical: ltr - padding-left is the left edge");

		StyledFixture rtl(R"Css(
			.box { display: flex; flex-direction: row; direction: rtl;
			       padding-left: 25px; padding-right: 5px; }
		)Css");
		rtl.run();
		checkNear(rtl.itemX(), 300.0f - 5.0f - 40.0f,
				"logical: rtl - padding-right is STILL the right edge, unmoved by direction");
	}

	// `margin-inline-start` on the item, same rule.
	{
		StyledFixture rtl(R"Css(
			.box { display: flex; flex-direction: row; direction: rtl; }
			.item { margin-inline-start: 12px; }
		)Css");
		rtl.run();
		checkNear(rtl.itemX(), 300.0f - 12.0f - 40.0f,
				"logical: rtl - margin-inline-start is the item's right margin");
	}

	/* The media flag the engine seeds from the locale, driven through the UI stylesheet - which is
	a different parser from the document one and has its own selector engine. */
	{
		StyledFixture off(R"Css(
			.box { display: flex; flex-direction: row; }
			@media (x-option: rtl) { :root { direction: rtl; } }
		)Css");
		off.run();
		checkNear(off.itemX(), 0.0f, "logical: with the rtl flag unset the sheet lays out ltr");

		StyledFixture on(R"Css(
			.box { display: flex; flex-direction: row; }
			@media (x-option: rtl) {
				/* a comment INSIDE the block, which is where a real sheet explains itself */
				:root { direction: rtl; }
			}
		)Css");
		on.root->getSystemByType<StyleSystem>()->setMediaOption(StringView("rtl"), true);
		on.run();
		checkNear(on.itemX(), 300.0f - 40.0f,
				"logical: setting the rtl flag turns the same sheet right-to-left");
	}

	// And `direction` reaches a child through inheritance, not just the node that declared it.
	{
		StyledFixture rtl(R"Css(
			.box { display: flex; flex-direction: row; }
			.item { }
		)Css");
		rtl.root->setOrUpdateComponent<InheritedTextStyle>([](NotNull<InheritedTextStyle> c) {
			c->direction = font::TextDirection::RightToLeft;
			c->defined |= InheritedTextStyle::DefinedDirection;
			return true;
		});
		rtl.run();
		checkNear(rtl.itemX(), 300.0f - 40.0f,
				"logical: a direction on an ancestor reaches the container that lays out");
	}
}

} // namespace stappler::xenolith::ui
