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

// The CSS Box Alignment keywords, which are three families and not one.
//
//   * `flex-start`/`flex-end` are FLEX-relative and follow the flex direction's own reversal.
//   * `start`/`end` are FLOW-relative and follow the writing mode.
//   * `left`/`right` are PHYSICAL and follow neither.
//
// Before this work all six collapsed onto two, which is invisible until something reverses. The
// table below is the whole point of the change, so it is asserted keyword by keyword.

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

using stappler::test::checkNear;

namespace {

// One 40pt item in a 300pt row, so `justify-content` has 260pt of slack to place it in: the item's
// x IS the answer, and 0 vs 260 says which end it went to.
static float placeOne(StringView flow, StringView justify, StringView dir) {
	auto css = mem_std::toString(".box { display: flex; flex-direction: ", flow,
			"; direction: ", dir, "; justify-content: ", justify, "; }");

	auto root = Rc<Node>::create();
	root->setContentSize(Size2(300.0f, 60.0f));
	auto sys = root->addSystem(Rc<StyleSystem>::create());
	sys->setStyleSheet(Rc<StyleSheet>::create(StringView(css)));

	auto box = root->addChild(Rc<Node>::create());
	box->setContentSize(Size2(300.0f, 60.0f));
	box->addStyleClass("box");
	box->addSystem(Rc<StyleResolver>::create());
	box->addSystem(Rc<LayoutSystem>::create());

	auto item = box->addChild(Rc<Node>::create());
	item->setContentSize(Size2(40.0f, 20.0f));

	box->getSystemByType<StyleResolver>()->apply();
	box->getSystemByType<LayoutSystem>()->apply();
	return item->getPosition().x;
}

constexpr float kLeft = 0.0f;
constexpr float kRight = 260.0f;

static void one(StringView flow, StringView justify, StringView dir, float expect,
		StringView name) {
	checkNear(placeOne(flow, justify, dir), expect, name);
}

} // namespace

void performAlignKeywordTests() {
	sprt::cout << "\n== ui layout: the alignment keyword families ==\n";

	/* `flex-start` is FLEX-relative: it follows the direction the flex line runs, whatever made it
	run that way. So it is at the left for an ltr row and at the right for both of the things that
	reverse one. */
	one("row", "flex-start", "ltr", kLeft, "align: flex-start - ltr row -> left");
	one("row", "flex-start", "rtl", kRight, "align: flex-start - rtl row -> right");
	one("row-reverse", "flex-start", "ltr", kRight, "align: flex-start - ltr row-reverse -> right");
	one("row-reverse", "flex-start", "rtl", kLeft, "align: flex-start - rtl row-reverse -> left");

	/* `start` is FLOW-relative: the writing mode's start. Under rtl that is the right edge - and
	the flex line ALSO reverses, so the two cancel and `start` stays at the line's start. That
	cancellation is why the mapper asks only about reversal. */
	one("row", "start", "ltr", kLeft, "align: start - ltr row -> left");
	one("row", "start", "rtl", kRight, "align: start - rtl row -> right (the inline start)");
	one("row-reverse", "start", "ltr", kLeft, "align: start - ltr row-reverse is still the left");
	one("row-reverse", "start", "rtl", kRight, "align: start - rtl row-reverse is still the right");

	// `end` is its mirror, in every one of the four.
	one("row", "end", "ltr", kRight, "align: end - ltr row -> right");
	one("row", "end", "rtl", kLeft, "align: end - rtl row -> left");
	one("row-reverse", "end", "ltr", kRight, "align: end - ltr row-reverse -> right");
	one("row-reverse", "end", "rtl", kLeft, "align: end - rtl row-reverse -> left");

	/* `left` and `right` are PHYSICAL and mean the same thing in all four. This is the family that
	used to be indistinguishable from `flex-start`/`flex-end`. */
	one("row", "left", "ltr", kLeft, "align: left - physical, ltr row");
	one("row", "left", "rtl", kLeft, "align: left - physical, rtl row - still the left");
	one("row-reverse", "left", "ltr", kLeft, "align: left - physical, ltr row-reverse");
	one("row-reverse", "left", "rtl", kLeft, "align: left - physical, rtl row-reverse");
	one("row", "right", "rtl", kRight, "align: right - physical, rtl row - still the right");
	one("row-reverse", "right", "ltr", kRight, "align: right - physical, ltr row-reverse");

	// `center` and the distributions have no side to take.
	one("row", "center", "ltr", 130.0f, "align: center - ltr");
	one("row", "center", "rtl", 130.0f, "align: center - rtl, the same place");
}

} // namespace stappler::xenolith::ui
