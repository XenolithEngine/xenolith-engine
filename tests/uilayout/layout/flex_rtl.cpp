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

// `direction: rtl` and the flex main axis.
//
// The claim under test is that RTL is not new machinery: the engine already ran an axis backwards
// for `row-reverse`, and RTL is the same flip decided a different way. So the interesting cases are
// the CANCELLATIONS - `row-reverse` inside `rtl` must lay out left to right again - and the fact
// that nothing about the CROSS axis of a row moves.

#include "SPCommon.h"

#include "XLNode.h"
#include "XLInheritedStyle.h"
#include "XLUiLayoutSystem.h"
#include "XLUiLayoutFlex.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::test::check;
using stappler::test::checkNear;

namespace {

// A container of fixed-size children, laid out once. Returns the children so a caller can read
// where they landed.
struct Fixture {
	Rc<Node> root;
	Vector<Rc<Node>> children;
	LayoutSystem *layout = nullptr;

	Fixture(Size2 size, const FlexLayoutInfo &info, SpanView<Size2> sizes,
			font::TextDirection dir = font::TextDirection::LeftToRight) {
		root = Rc<Node>::create();
		root->setContentSize(size);

		// The direction as the style resolver would have left it: an InheritedTextStyle with the
		// direction bit set. Written directly because this suite has no stylesheet - what is being
		// tested is the LAYOUT's reading of it, and the parser has its own tests.
		root->setOrUpdateComponent<InheritedTextStyle>([&](NotNull<InheritedTextStyle> c) {
			c->direction = dir;
			c->defined |= InheritedTextStyle::DefinedDirection;
			return true;
		});

		for (auto &s : sizes) {
			auto child = root->addChild(Rc<Node>::create());
			child->setContentSize(s);
			child->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> i) {
				i->grow = 0.0f;
				i->shrink = 0.0f;
				return true;
			});
			children.emplace_back(child);
		}

		layout = root->addSystem(Rc<LayoutSystem>::create(info));
		layout->apply();
	}

	float x(size_t i) const { return children.at(i)->getPosition().x; }
	float y(size_t i) const { return children.at(i)->getPosition().y; }
};

static FlexLayoutInfo rowInfo() {
	FlexLayoutInfo info;
	info.direction = FlexDirection::Row;
	return info;
}

} // namespace

void performFlexRtlTests() {
	sprt::cout << "\n== ui layout: direction and the flex main axis ==\n";

	const Size2 kBox(300.0f, 50.0f);
	const Size2 kSizes[] = {Size2(40.0f, 20.0f), Size2(60.0f, 20.0f), Size2(30.0f, 20.0f)};

	// ---- the row itself -------------------------------------------------------------------
	{
		Fixture ltr(kBox, rowInfo(), makeSpanView(kSizes, 3));
		checkNear(ltr.x(0), 0.0f, "flex-rtl: ltr row starts at the left edge");
		checkNear(ltr.x(1), 40.0f, "flex-rtl: ... and runs rightwards");
		checkNear(ltr.x(2), 100.0f, "flex-rtl: ... in order");

		Fixture rtl(kBox, rowInfo(), makeSpanView(kSizes, 3), font::TextDirection::RightToLeft);
		// The MIRROR of the above: the first item's right edge is the box's right edge.
		checkNear(rtl.x(0), 300.0f - 40.0f, "flex-rtl: rtl row starts at the right edge");
		checkNear(rtl.x(1), 300.0f - 100.0f, "flex-rtl: ... and runs leftwards");
		checkNear(rtl.x(2), 300.0f - 130.0f, "flex-rtl: ... in the same document order");
	}

	// ---- the cancellation ------------------------------------------------------------------
	{
		auto info = rowInfo();
		info.direction = FlexDirection::RowReverse;

		Fixture ltrRev(kBox, info, makeSpanView(kSizes, 3));
		checkNear(ltrRev.x(0), 300.0f - 40.0f, "flex-rtl: ltr row-reverse starts at the right");

		Fixture rtlRev(kBox, info, makeSpanView(kSizes, 3), font::TextDirection::RightToLeft);
		// `row-reverse` under `rtl` is left-to-right again, exactly as CSS says.
		checkNear(rtlRev.x(0), 0.0f, "flex-rtl: rtl row-reverse cancels back to the left");
		checkNear(rtlRev.x(1), 40.0f, "flex-rtl: ... running rightwards");
	}

	// ---- a column's main axis is the block axis and does not mirror --------------------------
	{
		auto info = rowInfo();
		info.direction = FlexDirection::Column;

		Fixture ltr(kBox, info, makeSpanView(kSizes, 3));
		Fixture rtl(kBox, info, makeSpanView(kSizes, 3), font::TextDirection::RightToLeft);
		checkNear(rtl.y(0), ltr.y(0), "flex-rtl: a column's main axis is unmoved by direction");
		checkNear(rtl.y(2), ltr.y(2), "flex-rtl: ... for every item");
	}

	// ---- padding bounds the content box on both sides ----------------------------------------
	{
		auto info = rowInfo();
		info.padding = Padding(0.0f, 20.0f, 0.0f, 10.0f); // top, right, bottom, left

		Fixture ltr(kBox, info, makeSpanView(kSizes, 1));
		checkNear(ltr.x(0), 10.0f, "flex-rtl: ltr starts after padding-left");

		Fixture rtl(kBox, info, makeSpanView(kSizes, 1), font::TextDirection::RightToLeft);
		// `padding-right` is still the RIGHT edge under rtl - only the flow reversed.
		checkNear(rtl.x(0), 300.0f - 20.0f - 40.0f, "flex-rtl: rtl ends before padding-right");
	}
}

} // namespace stappler::xenolith::ui
