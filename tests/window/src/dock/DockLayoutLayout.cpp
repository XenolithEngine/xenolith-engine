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

#include "dock/DockLayoutLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiLayoutSystem.h"
#include "XLUiDockTabBar.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float RootWidth = 800.0f;
static constexpr float RootHeight = 400.0f;
static constexpr float Thickness = 6.0f;

// declared minimums; the arithmetic below is written out from these
static constexpr Size2 MinExplorer = Size2(180.0f, 120.0f);
static constexpr Size2 MinEditor = Size2(320.0f, 200.0f);
static constexpr Size2 MinConsole = Size2(240.0f, 100.0f);
static constexpr Size2 MinProblems = Size2(260.0f, 80.0f);
static constexpr Size2 MinHuge = Size2(400.0f, 300.0f);

static constexpr float SidebarRatio = 0.25f;
static constexpr float MainRatio = 0.7f;

// A width on a frame is the trap this test also covers: without the SystemManagedLayout claim the
// style resolver would commit it with setContentSize and fight the placement pass every frame.
static constexpr auto s_css = StringView(R"css(
dock-frame { background-color: #232323; width: 300px; }
dock-frame#sidebar { background-color: #1b3a5c; }
dock-frame-body { padding: 4px; }
)css");

} // namespace

bool DockLayoutLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_root = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(1));
	_root->setAnchorPoint(Anchor::BottomLeft);
	_root->setContentSize(Size2(RootWidth, RootHeight));

	_dock = _root->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(Thickness);

	auto makePanel = [](StringView id, StringView title, Size2 minSize) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = title.str<Interface>();
		desc.minSize = minSize;
		desc.builder = [] { return Rc<basic2d::Layer>::create(Color::Teal_700); };
		return desc;
	};

	_dock->registerPanel(makePanel("explorer", "Explorer", MinExplorer));
	_dock->registerPanel(makePanel("editor", "Editor", MinEditor));
	_dock->registerPanel(makePanel("console", "Console", MinConsole));
	_dock->registerPanel(makePanel("problems", "Problems", MinProblems));
	_dock->registerPanel(makePanel("huge", "Huge", MinHuge));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(SidebarRatio,
			Spec::leaf({String("explorer")},
					{.name = String("sidebar"), .minSize = Size2(160.0f, 0.0f)}),
			Spec::vsplit(MainRatio, Spec::leaf({String("editor")}, {.name = String("main")}),
					Spec::leaf({String("console"), String("problems")},
							{.name = String("bottom"), .minSize = Size2(0.0f, 96.0f)}))));

	_sidebar = _dock->findFrameByName("sidebar");
	_main = _dock->findFrameByName("main");
	_bottom = _dock->findFrameByName("bottom");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.4f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.4f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.4f), [this] { runPhase4(); }));
	return true;
}

float DockLayoutLayout::stripHeight(ui::DockNodeHandle h) const {
	auto frame = _dock->getFrameNode(h);
	return (frame && frame->getTabBar()) ? frame->getTabBar()->getContentSize().height : 0.0f;
}

void DockLayoutLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DockLayoutTest", phase, ": ", what);
	}
}

void DockLayoutLayout::expectNear(StringView phase, StringView what, float actual, float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 0.5f) {
		++_failures;
		log::source().error("DockLayoutTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

void DockLayoutLayout::expectTiling(StringView phase) {
	// Sum of every leaf rect and every divider band must be the root's area exactly, and no two
	// leaves may overlap. Together those two say the rects tile the root with no gaps.
	auto &tree = _dock->getTree();
	const Size2 rootSize = _root->getContentSize();

	float area = 0.0f;
	Vector<Rect> leaves;
	tree.each([&](const ui::DockTreeNode &n) {
		if (n.isLeaf()) {
			area += n.rect.size.width * n.rect.size.height;
			leaves.emplace_back(n.rect);
		} else {
			area += n.splitterRect.size.width * n.splitterRect.size.height;
		}
	});

	expectNear(phase, "tiled area", area, rootSize.width * rootSize.height);

	bool overlap = false;
	for (size_t i = 0; i < leaves.size(); ++i) {
		for (size_t j = i + 1; j < leaves.size(); ++j) {
			if (leaves[i].intersectsRect(leaves[j])) {
				overlap = true;
			}
		}
	}
	expect(!overlap, phase, "leaf rects overlap");

	for (auto &r : leaves) {
		expect(r.origin.x >= -0.5f && r.origin.y >= -0.5f && r.getMaxX() <= rootSize.width + 0.5f
						&& r.getMaxY() <= rootSize.height + 0.5f,
				phase, "a leaf escaped the root");
	}
}

void DockLayoutLayout::runPhase1() {
	auto &tree = _dock->getTree();
	auto sidebar = tree.get(_sidebar);
	auto main = tree.get(_main);
	auto bottom = tree.get(_bottom);
	if (!sidebar || !main || !bottom) {
		expect(false, "phase1", "the layout did not build");
		return;
	}

	// The tab strip eats height off every frame, so each expectation below is "what the panel
	// needs, plus what the strip took". Reading the strip rather than assuming its size is what
	// keeps this test honest when the CSS behind the tabs changes.
	const float sidebarStrip = stripHeight(_sidebar);
	const float mainStrip = stripHeight(_main);
	const float bottomStrip = stripHeight(_bottom);
	expect(sidebarStrip > 0.0f, "phase1", "the tab strip has no height");

	// A panel's minimum raises the frame's: the sidebar declared 160 but parks a 180-wide panel
	expectNear("phase1", "sidebar min width", sidebar->minSize.width, MinExplorer.width);
	expectNear("phase1", "sidebar min height", sidebar->minSize.height,
			MinExplorer.height + sidebarStrip);
	// and a frame with two panels must fit the LARGEST of them, not their sum
	expectNear("phase1", "bottom min width", bottom->minSize.width, MinProblems.width);
	expectNear("phase1", "bottom min height", bottom->minSize.height,
			MinConsole.height + bottomStrip);

	// ... and that raise propagates through every split above
	const float minMainHeight = MinEditor.height + mainStrip;
	const float minBottomHeight = sprt::max(96.0f, MinConsole.height + bottomStrip);
	const float treeMinWidth = MinExplorer.width + MinEditor.width + Thickness;
	const float treeMinHeight = minMainHeight + minBottomHeight + Thickness;
	Size2 measured;
	expect(ui::LayoutSystem::measureNode(_root, MeasureConstraints{MeasureMode::MaxContent})
					!= Size2::ZERO,
			"phase1", "the dock did not answer the measurement protocol");
	measured = ui::LayoutSystem::measureNode(_root, MeasureConstraints{MeasureMode::MaxContent});
	expectNear("phase1", "measured tree min width", measured.width, treeMinWidth);
	expectNear("phase1", "measured tree min height", measured.height, treeMinHeight);

	// the ratio divides the space left AFTER both minimums are satisfied
	const float usable = RootWidth - Thickness;
	const float sidebarWidth =
			MinExplorer.width + (usable - MinExplorer.width - MinEditor.width) * SidebarRatio;
	expectNear("phase1", "sidebar width", sidebar->rect.size.width, sidebarWidth);
	expectNear("phase1", "sidebar height", sidebar->rect.size.height, RootHeight);
	expectNear("phase1", "sidebar x", sidebar->rect.origin.x, 0.0f);

	// Y points up: `first` of a vertical split is the TOP child
	expect(main->rect.origin.y > bottom->rect.origin.y, "phase1",
			"the first child of a vertical split must be on top");
	expectNear("phase1", "main top edge", main->rect.getMaxY(), RootHeight);
	expectNear("phase1", "bottom bottom edge", bottom->rect.origin.y, 0.0f);

	const float usableV = RootHeight - Thickness;
	const float mainHeight =
			minMainHeight + (usableV - minMainHeight - minBottomHeight) * MainRatio;
	expectNear("phase1", "main height", main->rect.size.height, mainHeight);

	// The CSS `width: 300px` on dock-frame must NOT have been committed onto the frames. Proving
	// that needs both halves: that the rule REACHED the frame at all (otherwise the check below is
	// vacuous), and that it landed in a MeasureComponent - an intrinsic hint the dock is free to
	// ignore - instead of in ContentSize, where it would fight the placement pass every frame.
	if (auto node = _dock->getFrameNode(_sidebar)) {
		auto measure = node->getComponent<MeasureComponent>();
		expect(measure != nullptr, "phase1", "the stylesheet never reached the frame");
		if (measure) {
			expectNear("phase1", "CSS width routed to MeasureComponent", measure->normal.width,
					300.0f);
		}
		expectNear("phase1", "sidebar node width", node->getContentSize().width, sidebarWidth);
		expectNear("phase1", "sidebar node x", node->getPosition().x, 0.0f);
	} else {
		expect(false, "phase1", "the sidebar has no scene node");
	}

	expectTiling("phase1");

	log::source().warn("DockLayoutTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; shrinking the root below the tree minimum");
	_root->setContentSize(Size2(400.0f, 200.0f));
}

void DockLayoutLayout::runPhase2() {
	// The root is now well below the tree's minimum. Under the Scale policy every minimum shrinks
	// proportionally, so nothing overlaps and nothing escapes - which is exactly what the tiling
	// check asserts against the NEW root size.
	expectTiling("phase2");

	auto &tree = _dock->getTree();
	auto sidebar = tree.get(_sidebar);
	auto main = tree.get(_main);
	if (sidebar && main) {
		expect(sidebar->rect.size.width > 0.0f && main->rect.size.height > 0.0f, "phase2",
				"a scaled-down frame collapsed to nothing");
		expect(sidebar->rect.size.width < sidebar->minSize.width, "phase2",
				"scaling must actually go below the minimum");
	}

	log::source().warn("DockLayoutTest", "phase2 done: ", _checks, " checks, ", _failures,
			" failures; restoring the root size");
	_root->setContentSize(Size2(RootWidth, RootHeight));
}

void DockLayoutLayout::runPhase3() {
	// Growing back must reproduce phase 1 exactly: the placement pass is a pure function of the
	// tree and the root size, so nothing may have been remembered from the scaled state.
	auto &tree = _dock->getTree();
	if (auto sidebar = tree.get(_sidebar)) {
		const float usable = RootWidth - Thickness;
		const float sidebarWidth =
				MinExplorer.width + (usable - MinExplorer.width - MinEditor.width) * SidebarRatio;
		expectNear("phase3", "sidebar width after regrow", sidebar->rect.size.width, sidebarWidth);
	}
	expectTiling("phase3");

	log::source().warn("DockLayoutTest", "phase3 done: ", _checks, " checks, ", _failures,
			" failures; parking a panel that needs more room");
	_dock->openPanel("huge", _sidebar);
}

void DockLayoutLayout::runPhase4() {
	auto &tree = _dock->getTree();
	auto sidebar = tree.get(_sidebar);
	if (!sidebar) {
		expect(false, "phase4", "the sidebar is gone");
		return;
	}

	// the newly parked panel is the largest one here now, so it sets the frame's floor
	const float sidebarStrip = stripHeight(_sidebar);
	expectNear("phase4", "sidebar min width", sidebar->minSize.width, MinHuge.width);
	expectNear("phase4", "sidebar min height", sidebar->minSize.height,
			MinHuge.height + sidebarStrip);

	// and that floor is now visible at the top of the tree
	const auto measured =
			ui::LayoutSystem::measureNode(_root, MeasureConstraints{MeasureMode::MaxContent});
	expectNear("phase4", "tree min width", measured.width,
			MinHuge.width + MinEditor.width + Thickness);

	// the frame can no longer be narrower than the panel it holds
	expect(sidebar->rect.size.width >= MinHuge.width - 0.5f, "phase4",
			"the frame is narrower than the panel parked in it");

	expectTiling("phase4");

	log::source().warn("DockLayoutTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

Value DockLayoutLayout::dumpTree() const {
	Value ret;
	auto &tree = _dock->getTree();
	tree.eachInOrder([&](const ui::DockTreeNode &n) {
		Value item;
		item.setInteger(n.self.index, "index");
		item.setInteger(n.self.generation, "generation");
		item.setString(n.isLeaf() ? "frame" : "split", "type");
		if (n.isSplit()) {
			item.setString(n.axis == ui::DockAxis::Horizontal ? "h" : "v", "axis");
			item.setDouble(n.ratio, "ratio");
			item.setValue(Value{Value(n.splitterRect.origin.x), Value(n.splitterRect.origin.y),
							  Value(n.splitterRect.size.width), Value(n.splitterRect.size.height)},
					"splitter");
		} else {
			item.setString(n.params.name, "name");
			Value panels;
			for (auto &id : n.panels) { panels.addString(id); }
			item.setValue(sp::move(panels), "panels");
			item.setInteger(n.active, "active");
		}
		item.setValue(Value{Value(n.minSize.width), Value(n.minSize.height)}, "min");
		item.setValue(Value{Value(n.rect.origin.x), Value(n.rect.origin.y),
						  Value(n.rect.size.width), Value(n.rect.size.height)},
				"rect");
		ret.addValue(sp::move(item));
	});
	return ret;
}

void DockLayoutLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("layout", "Dump the dock tree: kind, axis, ratio, minimum and rect per node",
			[this](Value &&) { return dumpTree(); });

	addCommand("resize", "Resize the dock root: {width, height}", [this](Value &&args) {
		_root->setContentSize(
				Size2(float(args.getDouble("width")), float(args.getDouble("height"))));
		return Value(true);
	});

	addCommand("summary", "Checks and failures so far", [this](Value &&) {
		Value ret;
		ret.setInteger(_checks, "checks");
		ret.setInteger(_failures, "failures");
		return ret;
	});
}

void DockLayoutLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - _root->getContentSize().height));
}

} // namespace stappler::xenolith::app
