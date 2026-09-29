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


// A pass with nothing changed moves nothing.
//
// A node that cannot measure itself (a sprite, a bare node) is flexed from its own size, and the
// size it has after one pass is the one the pass wrote. Flexing from that again made every pass a
// step further: beside a growing box that measures to zero, the unmeasured item crept towards the
// whole line and the box towards nothing, a frame at a time. The layout now flexes such an item
// from the basis it used last time, as long as nobody else has changed its size since.

#include "SPCommon.h"

#include "XLNode.h"
#include "XLUiLayoutSystem.h"
#include "XLUiLayoutFlex.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

using stappler::test::checkNear;

namespace {

struct Line {
	Rc<Node> root;
	LayoutSystem *layout = nullptr;
	Node *plain = nullptr; // measures nothing: flexed from its own size
	Node *box = nullptr; // an empty flex container: measures to zero

	Line(float width, float plainGrow, float boxGrow) {
		root = Rc<Node>::create();
		root->setContentSize(Size2(width, 100.0f));

		plain = root->addChild(Rc<Node>::create(), ZOrder(1));
		plain->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> i) {
			i->grow = plainGrow;
			return true;
		});

		box = root->addChild(Rc<Node>::create(), ZOrder(2));
		box->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo()));
		box->setOrUpdateComponent<FlexItemInfo>([&](NotNull<FlexItemInfo> i) {
			i->grow = boxGrow;
			return true;
		});

		FlexLayoutInfo info;
		info.direction = FlexDirection::Row;
		layout = root->addSystem(Rc<LayoutSystem>::create(info));
		layout->apply();
	}
};

} // namespace

void performFlexGrowSettlesTests() {
	sprt::cout << "\n== ui layout: a pass with nothing changed moves nothing ==\n";

	{
		Line l(300.0f, 1.0f, 2.0f);
		checkNear(l.plain->getContentSize().width, 100.0f, "grow-settles: weights 1:2 give a third");
		checkNear(l.box->getContentSize().width, 200.0f, "grow-settles: ... and two thirds");
		for (int i = 0; i < 5; ++i) { l.layout->apply(); }
		checkNear(l.plain->getContentSize().width, 100.0f,
				"grow-settles: five passes later the unmeasured item has not crept");
		checkNear(l.box->getContentSize().width, 200.0f, "grow-settles: ... nor has the box shrunk");
	}

	{
		// A narrower line flexes from the same basis, so the proportion holds.
		Line l(300.0f, 1.0f, 2.0f);
		l.root->setContentSize(Size2(150.0f, 100.0f));
		l.layout->apply();
		checkNear(l.plain->getContentSize().width, 50.0f,
				"grow-settles: a narrower line keeps the third");
		checkNear(l.box->getContentSize().width, 100.0f, "grow-settles: ... and the two thirds");
	}

	{
		// A size written by someone else is the node's own again.
		Line l(300.0f, 1.0f, 2.0f);
		l.plain->setContentSize(Size2(60.0f, 100.0f));
		l.layout->apply();
		checkNear(l.plain->getContentSize().width, 140.0f,
				"grow-settles: a size set from outside is the new basis (60 + 240/3)");
		l.layout->apply();
		checkNear(l.plain->getContentSize().width, 140.0f, "grow-settles: ... and it holds");

		// What was flexed from, not what came out: a tool turning sizes into weights needs it.
		checkNear(l.layout->getFlexBase(l.plain), 60.0f,
				"grow-settles: getFlexBase reports the basis the item was flexed from");
		checkNear(l.layout->getFlexBase(l.box), 0.0f,
				"grow-settles: ... and zero for the box that measures to nothing");
		checkNear(l.layout->getFlexBase(l.root), -1.0f,
				"grow-settles: ... and a negative answer for a node it did not place");
	}
}

} // namespace stappler::xenolith::ui
