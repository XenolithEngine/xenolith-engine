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

// `direction: rtl` over a grid and over a table.
//
// Both backends keep every decision - track sizing, `justify-self`, line numbers, the collapsed
// border boxes - in LOGICAL inline coordinates and mirror once, at projection. So the assertion
// that matters is that a mirrored grid is the exact reflection of the unmirrored one, column
// order included: an item in the first column sits at the RIGHT.

#include "SPCommon.h"

#include "XLNode.h"
#include "XLInheritedStyle.h"
#include "XLUiLayoutSystem.h"
#include "XLUiLayoutGrid.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::test::checkNear;

namespace {

// Three fixed 50pt columns in a 300pt box, one item per column.
struct GridFixture {
	Rc<Node> root;
	Vector<Rc<Node>> children;

	explicit GridFixture(font::TextDirection dir, Padding padding = Padding()) {
		root = Rc<Node>::create();
		root->setContentSize(Size2(300.0f, 60.0f));
		root->setOrUpdateComponent<InheritedTextStyle>([&](NotNull<InheritedTextStyle> c) {
			c->direction = dir;
			c->defined |= InheritedTextStyle::DefinedDirection;
			return true;
		});

		GridLayoutInfo info;
		info.padding = padding;
		for (uint32_t i = 0; i < 3; ++i) {
			info.columnTracks.emplace_back(GridTrack{GridTrack::Fixed, 50.0f});
		}
		info.rowTracks.emplace_back(GridTrack{GridTrack::Fixed, 40.0f});

		for (uint32_t i = 0; i < 3; ++i) {
			auto child = root->addChild(Rc<Node>::create());
			child->setContentSize(Size2(50.0f, 40.0f));
			children.emplace_back(child);
		}

		root->addSystem(Rc<LayoutSystem>::create(info))->apply();
	}

	float x(size_t i) const { return children.at(i)->getPosition().x; }
};

} // namespace

void performGridTableRtlTests() {
	sprt::cout << "\n== ui layout: direction over a grid ==\n";

	{
		GridFixture ltr(font::TextDirection::LeftToRight);
		checkNear(ltr.x(0), 0.0f, "grid-rtl: ltr puts the first column at the left");
		checkNear(ltr.x(1), 50.0f, "grid-rtl: ... the second beside it");
		checkNear(ltr.x(2), 100.0f, "grid-rtl: ... and the third after that");

		GridFixture rtl(font::TextDirection::RightToLeft);
		// The exact reflection: column 1 is now the RIGHTMOST, and the grid still starts at the
		// content box's inline start.
		checkNear(rtl.x(0), 300.0f - 50.0f, "grid-rtl: rtl puts the first column at the right");
		checkNear(rtl.x(1), 300.0f - 100.0f, "grid-rtl: ... the second to its left");
		checkNear(rtl.x(2), 300.0f - 150.0f, "grid-rtl: ... and the third after that");
	}

	// Padding stays physical: the mirror happens INSIDE the content box.
	{
		const Padding pad(0.0f, 20.0f, 0.0f, 10.0f); // top, right, bottom, left
		GridFixture ltr(font::TextDirection::LeftToRight, pad);
		checkNear(ltr.x(0), 10.0f, "grid-rtl: ltr starts after padding-left");

		GridFixture rtl(font::TextDirection::RightToLeft, pad);
		checkNear(rtl.x(0), 300.0f - 20.0f - 50.0f, "grid-rtl: rtl starts before padding-right");
	}
}

} // namespace stappler::xenolith::ui
