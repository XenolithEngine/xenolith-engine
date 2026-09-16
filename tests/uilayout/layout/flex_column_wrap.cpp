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

// A column measures a wrapping item's height AT THE WIDTH IT WILL GIVE IT.
//
// A row asks an item for a width and re-measures the height once the width is final. A column's
// main size is the height, and a height of wrapping content depends on the width - asked for
// max-content, a label answered with its one-line height, the column placed its next item one line
// down, and the label, stretched to the column's width, wrapped into two lines over that item. That
// is what xlstudio's Run panel showed with a report's message under its buttons.
//
// No font here: a `Paragraph` answers the measurement protocol the way a label does - 300pt in one
// line of 20pt, and as many 20pt lines as it takes at a narrower width - so the numbers are exact.

#include "SPCommon.h"

#include "XLNode.h"
#include "XLUiLayoutSystem.h"
#include "XLUiLayoutFlex.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::test::checkNear;

namespace {

constexpr float kLineWidth = 300.0f;
constexpr float kLineHeight = 20.0f;

class ParagraphMeasure : public System {
public:
	virtual bool init() override {
		if (!System::init()) {
			return false;
		}
		_systemFlags = SystemFlags::HandleMeasure;
		return true;
	}

	virtual bool handleMeasure(const MeasureConstraints &c, Size2 &result) override {
		if (c.mode == MeasureMode::MaxContent) {
			// one line, whatever the bound - which is what a label's max-content is
			result = Size2(kLineWidth, kLineHeight);
		} else {
			const float width = (c.maxWidth != maxOf<float>())
					? sprt::min(kLineWidth, sprt::max(c.maxWidth, 1.0f))
					: kLineWidth;
			result = Size2(width, kLineHeight * sprt::ceil(kLineWidth / width));
		}
		return true;
	}
};

struct Column {
	Rc<Node> root;
	Node *paragraph = nullptr;
	Node *next = nullptr;

	Column(float width, FlexDirection dir, FlexAlign align, float marginLeft = 0.0f) {
		root = Rc<Node>::create();
		root->setContentSize(Size2(width, 400.0f));

		auto p = root->addChild(Rc<Node>::create(), ZOrder(1));
		p->addSystem(Rc<ParagraphMeasure>::create());
		p->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> i) {
			i->grow = 0.0f;
			i->shrink = 1.0f;
			i->margin.left = marginLeft;
			return true;
		});
		paragraph = p;

		auto n = root->addChild(Rc<Node>::create(), ZOrder(2));
		n->setContentSize(Size2(20.0f, 20.0f));
		n->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> i) {
			i->grow = 0.0f;
			i->shrink = 0.0f;
			return true;
		});
		next = n;

		FlexLayoutInfo info;
		info.direction = dir;
		info.alignItems = align;
		root->addSystem(Rc<LayoutSystem>::create(info))->apply();
	}

	// Distance from the container's top edge to the item's top edge: the column runs down, the
	// node space runs up.
	float top(Node *node) const {
		return root->getContentSize().height - node->getPosition().y - node->getContentSize().height;
	}
};

} // namespace

void performFlexColumnWrapTests() {
	sprt::cout << "\n== ui layout: a column measures a height at its width ==\n";

	{
		Column c(200.0f, FlexDirection::Column, FlexAlign::Stretch);
		checkNear(c.paragraph->getContentSize().width, 200.0f,
				"column-wrap: stretched to the column's width");
		checkNear(c.paragraph->getContentSize().height, 40.0f,
				"column-wrap: ... and as tall as two lines at that width");
		checkNear(c.top(c.next), 40.0f, "column-wrap: the next item starts below both lines");
	}

	{
		Column c(200.0f, FlexDirection::Column, FlexAlign::FlexStart);
		checkNear(c.paragraph->getContentSize().width, 200.0f,
				"column-wrap: not stretched, it still wraps at the column's width");
		checkNear(c.paragraph->getContentSize().height, 40.0f,
				"column-wrap: ... two lines");
		checkNear(c.top(c.next), 40.0f, "column-wrap: ... and the next item is below them");
	}

	{
		// 100pt of the 200 is margin: three lines at the width that is left.
		Column c(200.0f, FlexDirection::Column, FlexAlign::Stretch, 100.0f);
		checkNear(c.paragraph->getContentSize().width, 100.0f,
				"column-wrap: a cross margin narrows the width it wraps at");
		checkNear(c.paragraph->getContentSize().height, 60.0f, "column-wrap: ... three lines");
		checkNear(c.top(c.next), 60.0f, "column-wrap: ... and the next item is below all three");
	}

	{
		Column c(400.0f, FlexDirection::Column, FlexAlign::Stretch);
		checkNear(c.paragraph->getContentSize().height, 20.0f,
				"column-wrap: a column wide enough keeps it on one line");
		checkNear(c.top(c.next), 20.0f, "column-wrap: ... and the next item one line down");
	}

	{
		// The row was right before and must stay so: width first, height from it.
		Column c(200.0f, FlexDirection::Row, FlexAlign::FlexStart);
		checkNear(c.paragraph->getContentSize().width, 180.0f,
				"column-wrap: a row still gives the width left beside its neighbour");
		checkNear(c.paragraph->getContentSize().height, 40.0f,
				"column-wrap: ... and the height follows from that width");
	}

	{
		// A column that is measured rather than laid out answers with the same height.
		Column c(200.0f, FlexDirection::Column, FlexAlign::Stretch);
		MeasureConstraints mc;
		mc.maxWidth = 200.0f;
		const Size2 m = LayoutSystem::measureNode(c.root, mc);
		checkNear(m.height, 60.0f, "column-wrap: measured at 200pt, the column is both items tall");
	}
}

} // namespace stappler::xenolith::ui
