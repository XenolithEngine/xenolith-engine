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

#include "dock/DockSplitterLayout.h"
#include "XLUiDockSplitter.h"
#include "XLUiStyleResolver.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float RootWidth = 600.0f;
static constexpr float RootHeight = 300.0f;
static constexpr float Thickness = 6.0f;

static constexpr Size2 MinLeft = Size2(100.0f, 50.0f);
static constexpr Size2 MinTop = Size2(150.0f, 60.0f);
static constexpr Size2 MinBottom = Size2(150.0f, 40.0f);

// derived once, so the expectations below read as arithmetic and not as magic numbers
static constexpr float UsableH = RootWidth - Thickness;
static constexpr float FreeH = UsableH - MinLeft.width - MinTop.width;

static constexpr auto s_css = StringView(R"css(
dock-frame { background-color: #232323; }
dock-splitter { background-color: #2a2a2a; }
dock-splitter:hover, dock-splitter.dragging { background-color: #3d7ecf; }
)css");

} // namespace

bool DockSplitterLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makePanel = [](StringView id, Size2 minSize) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = id.str<Interface>();
		desc.minSize = minSize;
		desc.builder = [] { return Rc<basic2d::Layer>::create(Color::Teal_700); };
		return desc;
	};

	_root = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(1));
	_root->setAnchorPoint(Anchor::BottomLeft);
	_root->setContentSize(Size2(RootWidth, RootHeight));

	_dock = _root->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(Thickness);
	_dock->registerPanel(makePanel("left", MinLeft));
	_dock->registerPanel(makePanel("top", MinTop));
	_dock->registerPanel(makePanel("bottom", MinBottom));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(0.5f, Spec::leaf({String("left")}, {.name = String("left")}),
			Spec::vsplit(0.5f, Spec::leaf({String("top")}, {.name = String("top")}),
					Spec::leaf({String("bottom")}, {.name = String("bottom")}))));

	_left = _dock->findFrameByName("left");
	_right = _dock->findFrameByName("top");

	// A second dock whose right-hand place forbids resizing. One frozen side is enough to freeze
	// the divider - the frame on the other side has nowhere to give the space to.
	_frozenRoot = addChild(Rc<basic2d::Layer>::create(Color::Grey_800), ZOrder(1));
	_frozenRoot->setAnchorPoint(Anchor::BottomLeft);
	_frozenRoot->setContentSize(Size2(RootWidth, 80.0f));

	_frozenDock = _frozenRoot->addSystem(Rc<ui::DockSystem>::create());
	_frozenDock->setSplitterThickness(Thickness);
	_frozenDock->registerPanel(makePanel("fa", Size2(60.0f, 20.0f)));
	_frozenDock->registerPanel(makePanel("fb", Size2(60.0f, 20.0f)));
	_frozenDock->setLayout(Spec::hsplit(0.5f, Spec::leaf({String("fa")}, {.name = String("fa")}),
			Spec::leaf({String("fb")},
					{.name = String("fb"),
						.flags = ui::DockFrameFlags::Default & ~ui::DockFrameFlags::AllowResize})));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); }));
	return true;
}

void DockSplitterLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DockSplitterTest", phase, ": ", what);
	}
}

void DockSplitterLayout::expectNear(StringView phase, StringView what, float actual,
		float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 0.5f) {
		++_failures;
		log::source().error("DockSplitterTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

void DockSplitterLayout::runPhase1() {
	auto &tree = _dock->getTree();

	// one divider node per split, and nothing else
	size_t splits = 0;
	size_t splitterNodes = 0;
	tree.each([&](const ui::DockTreeNode &n) {
		if (!n.isSplit()) {
			return;
		}
		++splits;
		if (dynamic_cast<ui::DockSplitter *>(n.node.get())) {
			++splitterNodes;
		}
	});
	expect(splits == 2, "phase1", "expected two splits");
	expect(splitterNodes == splits, "phase1", "a split without a divider node");

	_split = tree.getRoot();
	auto split = tree.get(_split);
	if (!split || !split->isSplit()) {
		expect(false, "phase1", "the root is not a split");
		return;
	}

	auto splitter = static_cast<ui::DockSplitter *>(split->node.get());
	auto left = tree.get(split->first);
	auto right = tree.get(split->second);
	if (!splitter || !left || !right) {
		expect(false, "phase1", "the split is not wired up");
		return;
	}

	// ABOVE the frames, and in a band of its own: sortAllChildren is unstable, so sharing a band
	// with the frames would let one of them cover the divider on an arbitrary frame
	expect(splitter->getLocalZOrder() == ui::DockSystem::SplitterZOrder, "phase1",
			"the divider is not in the splitter ZOrder band");
	if (auto leftNode = _dock->getFrameNode(split->first)) {
		expect(leftNode->getLocalZOrder() < splitter->getLocalZOrder(), "phase1",
				"a frame is not below the divider");
	}

	// geometrically between its two children, exactly filling the gap
	expectNear("phase1", "divider thickness", splitter->getContentSize().width, Thickness);
	expectNear("phase1", "divider height", splitter->getContentSize().height, RootHeight);
	expectNear("phase1", "divider x", splitter->getPosition().x, left->rect.getMaxX());
	expectNear("phase1", "gap to the right child", right->rect.origin.x - splitter->getPosition().x,
			Thickness);

	// a horizontal split divides left from right, so the bar is vertical and moves a column edge
	if (auto listener = splitter->getSystemByType<InputListener>()) {
		expect(listener->getCursor() == WindowCursor::ResizeCol, "phase1",
				"a horizontal split needs the column-resize cursor");
	} else {
		expect(false, "phase1", "the divider has no input listener");
	}

	// the vertical split's divider gets the row cursor instead
	auto vsplit = tree.get(split->second);
	if (vsplit && vsplit->isSplit()) {
		if (auto vnode = static_cast<ui::DockSplitter *>(vsplit->node.get())) {
			if (auto listener = vnode->getSystemByType<InputListener>()) {
				expect(listener->getCursor() == WindowCursor::ResizeRow, "phase1",
						"a vertical split needs the row-resize cursor");
			}
		}
	}

	log::source().warn("DockSplitterTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; dragging in steps");

	// Ten small deltas. The ratio is re-derived in ratio space, so they must land exactly where a
	// single delta of the same total would - and must not depend on a placement pass running in
	// between, which is why they are all issued inside this one frame.
	for (size_t i = 0; i < 10; ++i) { _dock->updateSplitterDrag(_split, Vec2(8.0f, 0.0f)); }
}

void DockSplitterLayout::runPhase2() {
	auto &tree = _dock->getTree();
	auto split = tree.get(_split);
	auto left = tree.get(split->first);
	if (!left) {
		expect(false, "phase2", "the left frame is gone");
		return;
	}

	_widthAfterSteps = left->rect.size.width;

	// ten steps of 8pt from the middle of the free space
	const float expected = MinLeft.width + FreeH * 0.5f + 80.0f;
	expectNear("phase2", "width after ten small drags", _widthAfterSteps, expected);

	// now the same travel in one go, from the same starting ratio
	_dock->setSplitRatio(_split, 0.5f);
	_dock->updateSplitterDrag(_split, Vec2(80.0f, 0.0f));
}

void DockSplitterLayout::runPhase3() {
	auto &tree = _dock->getTree();
	auto split = tree.get(_split);
	auto left = tree.get(split->first);
	if (!left) {
		expect(false, "phase3", "the left frame is gone");
		return;
	}

	expectNear("phase3", "one large drag equals ten small ones", left->rect.size.width,
			_widthAfterSteps);

	// A drag writes the RATIO immediately; the rects only follow on the next placement pass. So
	// everything checked in this phase is read out of the tree's ratios, and the geometry that
	// results from them is checked one phase later.

	// Pull it far past the left frame's floor: it must stop AT the floor, which in ratio space is
	// exactly 0 - the whole free space went to the other side.
	_dock->updateSplitterDrag(_split, Vec2(-10'000.0f, 0.0f));
	expectNear("phase3", "ratio clamped at the left floor", tree.get(_split)->ratio, 0.0f);

	// and the other way: all the free space to the left, the right frame left at its own minimum
	_dock->updateSplitterDrag(_split, Vec2(10'000.0f, 0.0f));
	expectNear("phase3", "ratio clamped at the right floor", tree.get(_split)->ratio, 1.0f);

	// Vertical: dragging the divider DOWN is a negative delta.y and must make the TOP frame taller
	auto vsplitHandle = split->second;
	if (auto vsplit = tree.get(vsplitHandle); vsplit && vsplit->isSplit()) {
		expect(vsplit->ratio == 0.5f, "phase3", "the vertical split should be untouched so far");
		_dock->updateSplitterDrag(vsplitHandle, Vec2(0.0f, -20.0f));
		expect(tree.get(vsplitHandle)->ratio > 0.5f, "phase3",
				"dragging a divider down must grow the top frame");
	}

	log::source().warn("DockSplitterTest", "phase3 done: ", _checks, " checks, ", _failures,
			" failures; trying a frozen divider");

	// a frame with AllowResize cleared freezes the divider beside it
	_frozenSplit = _frozenDock->getRootNode();
	expect(!_frozenDock->canResize(_frozenSplit), "phase3",
			"a frame with AllowResize cleared must freeze its divider");
	_dock->updateSplitterDrag(_frozenSplit, Vec2(50.0f, 0.0f));
}

void DockSplitterLayout::runPhase4() {
	// Now the geometry the ratios of phase 3 produce. The horizontal divider was left clamped all
	// the way right, so the left frame holds everything except the other side's minimum.
	auto &tree = _dock->getTree();
	auto split = tree.get(_split);
	auto leftFrame = tree.get(split->first);
	auto rightSide = tree.get(split->second);
	expectNear("phase4", "left frame after the clamp", leftFrame->rect.size.width,
			UsableH - MinTop.width);
	expectNear("phase4", "the other side is at its own floor", rightSide->rect.size.width,
			MinTop.width);
	expect(rightSide->rect.size.width >= rightSide->minSize.width - 0.5f, "phase4",
			"the far side was pushed below its minimum");

	auto vsplit = tree.get(split->second);
	if (vsplit && vsplit->isSplit()) {
		auto top = tree.get(vsplit->first);
		auto bottom = tree.get(vsplit->second);
		expect(top->rect.getMaxY() > bottom->rect.getMaxY(), "phase4",
				"the first child of a vertical split must stay on top");
		expectNear("phase4", "top frame grew by the drag", top->rect.size.height,
				MinTop.height + (RootHeight - Thickness - MinTop.height - MinBottom.height) * 0.5f
						+ 20.0f);
	}

	auto &frozenTree = _frozenDock->getTree();
	if (auto frozen = frozenTree.get(_frozenSplit)) {
		expectNear("phase4", "a frozen divider did not move", frozen->ratio, 0.5f);
	}

	log::source().warn("DockSplitterTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void DockSplitterLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("drag", "Move a divider: {index, dx, dy} - index is the tree slot of the split",
			[this](Value &&args) {
		ui::DockNodeHandle h;
		h.index = uint32_t(args.getInteger("index"));
		h.generation = uint32_t(args.getInteger("generation", 1));
		_dock->updateSplitterDrag(h,
				Vec2(float(args.getDouble("dx")), float(args.getDouble("dy"))));
		Value ret;
		if (auto n = _dock->getTree().get(h)) {
			ret.setDouble(n->ratio, "ratio");
		}
		return ret;
	});

	addCommand("probe", "World-space centre of a divider and the current ratio: {index}",
			[this](Value &&args) {
		ui::DockNodeHandle h;
		h.index = uint32_t(args.getInteger("index", int64_t(_dock->getRootNode().index)));
		h.generation = uint32_t(args.getInteger("generation", 1));

		Value ret;
		auto n = _dock->getTree().get(h);
		if (!n || !n->isSplit() || !n->node) {
			return ret;
		}
		const auto centre = n->node->convertToWorldSpace(Vec2(
				n->node->getContentSize().width / 2.0f, n->node->getContentSize().height / 2.0f));
		ret.setDouble(centre.x, "x");
		ret.setDouble(centre.y, "y");
		ret.setDouble(n->ratio, "ratio");
		if (auto first = _dock->getTree().get(n->first)) {
			ret.setDouble(first->rect.size.width, "firstWidth");
		}
		return ret;
	});

	addCommand("summary", "Checks and failures so far", [this](Value &&) {
		Value ret;
		ret.setInteger(_checks, "checks");
		ret.setInteger(_failures, "failures");
		return ret;
	});
}

void DockSplitterLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - RootHeight));
	_frozenRoot->setPosition(Vec2(40.0f, getWorkTop() - 60.0f - RootHeight - 80.0f));
}

} // namespace stappler::xenolith::app
