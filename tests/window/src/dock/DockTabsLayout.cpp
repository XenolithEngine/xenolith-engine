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

#include "dock/DockTabsLayout.h"
#include "XLUiDockTabBar.h"
#include "XLUiStyleResolver.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float RootWidth = 800.0f;
static constexpr float RootHeight = 400.0f;
static constexpr float Thickness = 6.0f;
static constexpr float EdgeBand = 40.0f;

static constexpr auto s_css = StringView(R"css(
dock-frame { background-color: #232323; }
dock-frame-body { padding: 4px; }
dock-tab-bar { background-color: #171717; }
dock-tab { padding: 4px 10px; background-color: #2b2b2b; }
dock-tab.active { background-color: #232323; }
dock-tab > label { color: #c8c8c8; font-size: 13px; }
dock-splitter { background-color: #2a2a2a; }
dock-drop-indicator { background-color: #3d7ecf; }
)css");

} // namespace

bool DockTabsLayout::init() {
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
	_dock->setEdgeDropBand(EdgeBand);

	auto makePanel = [this](StringView id, Size2 minSize) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = id.str<Interface>();
		desc.minSize = minSize;
		// Counts every build and hands back the SAME node each time, so a rebuild would show up
		// twice over: as a bumped counter and as a different node in the frame body.
		desc.builder = [this, key = id.str<Interface>()]() -> Rc<Node> {
			// NOT `_builds[key] += 1`: reading a missing key through the map's proxy is a hard
			// trap by design here - insertion only ever happens through operator=
			_builds.insert_or_assign(key, buildCount(key) + 1);
			auto node = Rc<basic2d::Layer>::create(Color::Teal_700);
			_built.insert_or_assign(key, node);
			return node;
		};
		return desc;
	};

	_dock->registerPanel(makePanel("alpha", Size2(120.0f, 60.0f)));
	_dock->registerPanel(makePanel("beta", Size2(120.0f, 60.0f)));
	_dock->registerPanel(makePanel("gamma", Size2(120.0f, 60.0f)));
	_dock->registerPanel(makePanel("delta", Size2(120.0f, 60.0f)));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(0.5f,
			Spec::leaf({String("alpha"), String("beta")}, {.name = String("left")}),
			Spec::leaf({String("gamma")}, {.name = String("right")})));

	_left = _dock->findFrameByName("left");
	_right = _dock->findFrameByName("right");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase5(); }));
	return true;
}

void DockTabsLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DockTabsTest", phase, ": ", what);
	}
}

void DockTabsLayout::expectNear(StringView phase, StringView what, float actual, float expected) {
	++_checks;
	if (sprt::abs(actual - expected) > 0.5f) {
		++_failures;
		log::source().error("DockTabsTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

size_t DockTabsLayout::buildCount(StringView id) const {
	auto it = _builds.find(id.str<Interface>());
	return (it != _builds.end()) ? it->second : 0;
}

Vector<String> DockTabsLayout::panelsOf(ui::DockNodeHandle h) const {
	auto span = _dock->getPanelsInFrame(h);
	return Vector<String>(span.begin(), span.end());
}

void DockTabsLayout::runPhase1() {
	// only what is actually showing has been built; `beta` sits behind `alpha` and must not exist
	expect(buildCount("alpha") == 1, "phase1", "the visible panel was not built exactly once");
	expect(buildCount("beta") == 0, "phase1", "a hidden panel must not be built");
	expect(buildCount("gamma") == 1, "phase1", "the other frame's panel was not built");
	expect(buildCount("delta") == 0, "phase1", "an unopened panel must not be built");

	auto frame = _dock->getFrameNode(_left);
	if (!frame || !frame->getTabBar()) {
		expect(false, "phase1", "the left frame has no tab strip");
		return;
	}
	auto bar = frame->getTabBar();
	expect(bar->getTabs().size() == 2, "phase1", "expected two tabs in the left frame");

	// exactly one tab carries `active`, and it is the one whose panel is showing
	size_t active = 0;
	for (auto &tab : bar->getTabs()) {
		if (tab->isActive()) {
			++active;
			expect(tab->getPanelId() == "alpha", "phase1", "the wrong tab is marked active");
		}
	}
	expect(active == 1, "phase1", "exactly one tab must be active");

	// the strip took height off the body, and the frame's minimum grew with it
	expect(bar->getContentSize().height > 0.0f, "phase1", "the tab strip has no height");
	auto leaf = _dock->getTree().get(_left);
	expect(leaf->minSize.height > 60.0f, "phase1",
			"the strip must raise the frame's minimum above the panel's own");

	log::source().warn("DockTabsTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; switching tabs");
	_dock->activatePanel("beta");
}

void DockTabsLayout::runPhase2() {
	expect(buildCount("beta") == 1, "phase2", "the panel was not built on first show");
	expect(buildCount("alpha") == 1, "phase2", "switching away must not rebuild anything");

	auto frame = _dock->getFrameNode(_left);
	auto body = frame->getBody();
	expect(body->getChildren().size() == 1, "phase2", "the body must host exactly one panel");
	expect(body->getChildren().at(0) == _built["beta"].get(), "phase2",
			"the body hosts the wrong panel");

	// switching back must reuse the node built in phase 1, not make another
	_dock->activatePanel("alpha");
}

void DockTabsLayout::runPhase3() {
	expect(buildCount("alpha") == 1, "phase3", "switching back rebuilt the panel");

	auto frame = _dock->getFrameNode(_left);
	expect(frame->getBody()->getChildren().at(0) == _built["alpha"].get(), "phase3",
			"the panel node was replaced instead of reused");

	// --- the drop zones, without synthesizing a drag ---------------------------------------
	auto &tree = _dock->getTree();
	auto rightRect = tree.get(_right)->rect;
	auto rightFrame = _dock->getFrameNode(_right);
	const float stripHeight = rightFrame->getTabBar()->getContentSize().height;

	// the middle of the body: park it here as another tab
	const Vec2 centre(rightRect.getMidX(),
			rightRect.origin.y + (rightRect.size.height - stripHeight) / 2.0f);
	auto target = _dock->hitTest(centre, "alpha");
	expect(target.kind == ui::DockDropTarget::Kind::Center, "phase3",
			"the middle of a frame must be a Center drop");
	expect(target.frame == _right, "phase3", "the hit test found the wrong frame");

	// each of the four edge bands, and the side each of them means
	struct Probe {
		Vec2 point;
		ui::DockDropTarget::Kind kind;
		StringView what;
	};
	const float inset = EdgeBand / 2.0f;
	const float bodyTop = rightRect.getMaxY() - stripHeight;
	const Probe probes[] = {
		{Vec2(rightRect.origin.x + inset,
				 rightRect.origin.y + rightRect.size.height / 2.0f - stripHeight / 2.0f),
			ui::DockDropTarget::Kind::SplitLeft, StringView("left band")},
		{Vec2(rightRect.getMaxX() - inset,
				 rightRect.origin.y + rightRect.size.height / 2.0f - stripHeight / 2.0f),
			ui::DockDropTarget::Kind::SplitRight, StringView("right band")},
		{Vec2(rightRect.getMidX(), bodyTop - inset), ui::DockDropTarget::Kind::SplitTop,
			StringView("top band")},
		{Vec2(rightRect.getMidX(), rightRect.origin.y + inset),
			ui::DockDropTarget::Kind::SplitBottom, StringView("bottom band")},
	};
	for (auto &p : probes) {
		auto t = _dock->hitTest(p.point, "alpha");
		expect(t.kind == p.kind, "phase3", toString(p.what, " resolved to zone ", toInt(t.kind)));
		expect(t.highlight.size.width > 0.0f && t.highlight.size.height > 0.0f, "phase3",
				"a split zone must highlight something");
	}

	// the tab strip beats the body
	const Vec2 onStrip(rightRect.origin.x + 8.0f, rightRect.getMaxY() - stripHeight / 2.0f);
	auto stripTarget = _dock->hitTest(onStrip, "alpha");
	expect(stripTarget.kind == ui::DockDropTarget::Kind::TabStrip, "phase3",
			"the tab strip must win over the body");

	// outside the dock: nowhere at all
	expect(_dock->hitTest(Vec2(-50.0f, -50.0f), "alpha").kind == ui::DockDropTarget::Kind::None,
			"phase3", "a point outside the dock must resolve to nothing");

	log::source().warn("DockTabsTest", "phase3 done: ", _checks, " checks, ", _failures,
			" failures; moving a panel between frames");

	// move `alpha` into the right frame, at the front of its strip
	_dock->movePanel("alpha", _right, 0);
}

void DockTabsLayout::runPhase4() {
	// it arrived where it was asked to, and the node came with it - not a copy of it
	auto right = panelsOf(_right);
	expect(right.size() == 2 && right[0] == "alpha", "phase4",
			"the panel did not land at the requested index");
	expect(buildCount("alpha") == 1, "phase4", "moving a panel rebuilt it");
	expect(_dock->getFrameNode(_right)->getBody()->getChildren().at(0) == _built["alpha"].get(),
			"phase4", "the moved panel is a different node");

	auto left = panelsOf(_left);
	expect(left.size() == 1 && left[0] == "beta", "phase4", "the source frame was not updated");

	// a lone panel dropped back into its own frame is not a drop at all
	expect(_dock->hitTest(_dock->getTree().get(_left)->rect.origin
								+ Vec2(_dock->getTree().get(_left)->rect.size.width / 2.0f, 20.0f),
						"beta")
							.kind
					== ui::DockDropTarget::Kind::None,
			"phase4", "dropping a lone panel into its own frame must be a no-op");

	log::source().warn("DockTabsTest", "phase4 done: ", _checks, " checks, ", _failures,
			" failures; emptying a frame");

	// closing the last panel of the left frame must collapse it and give the space to its sibling
	_dock->closePanel("beta");
}

void DockTabsLayout::runPhase5() {
	auto &tree = _dock->getTree();
	expect(!tree.isValid(_left), "phase5", "an emptied frame must collapse");
	expect(tree.getLeafCount() == 1, "phase5", "the sibling should now be the whole tree");

	// ...and it must be gone from the SCENE too, not merely from the tree. A node whose slot died
	// keeps whatever rect it had and nothing in the tree would ever mention it again, so it would
	// sit there drawn and hit-testable forever.
	size_t frames = 0;
	size_t splitters = 0;
	for (auto &child : _root->getChildren()) {
		if (child->getComponent<ui::DockFrameComponent>()) {
			++frames;
		} else if (child->getType() == "dock-splitter") {
			++splitters;
		}
	}
	expect(frames == 1, "phase5", "a collapsed frame is still in the scene");
	expect(splitters == 0, "phase5", "the divider of a merged split is still in the scene");

	auto right = tree.get(_right);
	if (right) {
		expectNear("phase5", "the sibling took the whole width", right->rect.size.width, RootWidth);
		expectNear("phase5", "the sibling took the whole height", right->rect.size.height,
				RootHeight);
	} else {
		expect(false, "phase5", "the surviving frame is gone too");
	}

	// the closed panel's node is kept: reopening it must not rebuild
	_dock->openPanel("beta", _right);
	expect(buildCount("beta") == 1, "phase5", "reopening a closed panel rebuilt it");

	log::source().warn("DockTabsTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void DockTabsLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("hit-test", "Resolve a drop point: {x, y, panel}", [this](Value &&args) {
		auto t = _dock->hitTest(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))),
				args.getString("panel"));
		Value ret;
		ret.setInteger(toInt(t.kind), "kind");
		ret.setInteger(t.frame.index, "frame");
		ret.setInteger(t.tabIndex, "tabIndex");
		ret.setValue(Value{Value(t.highlight.origin.x), Value(t.highlight.origin.y),
						 Value(t.highlight.size.width), Value(t.highlight.size.height)},
				"highlight");
		return ret;
	});

	addCommand("tabs", "Panels of a frame: {frame} (its tree index)", [this](Value &&args) {
		ui::DockNodeHandle h;
		h.index = uint32_t(args.getInteger("frame"));
		h.generation = uint32_t(args.getInteger("generation", 1));
		Value ret;
		for (auto &id : _dock->getPanelsInFrame(h)) { ret.addString(id); }
		return ret;
	});

	addCommand("probe", "World-space centre of a panel's tab, and of a point in its frame: {panel}",
			[this](Value &&args) {
		Value ret;
		auto panel = args.getString("panel");
		auto h = _dock->findFrameForPanel(panel);
		auto frame = _dock->getFrameNode(h);
		if (!frame || !frame->getTabBar()) {
			return ret;
		}
		for (auto &tab : frame->getTabBar()->getTabs()) {
			if (tab->getPanelId() != panel) {
				continue;
			}
			const auto centre = tab->convertToWorldSpace(
					Vec2(tab->getContentSize().width / 2.0f, tab->getContentSize().height / 2.0f));
			ret.setDouble(centre.x, "x");
			ret.setDouble(centre.y, "y");
			ret.setInteger(h.index, "frame");
		}
		// the world-space centre of the frame's body, and a point deep in its left edge band
		const auto rect = _dock->getTree().get(h)->rect;
		const auto rootPos = _root->convertToWorldSpace(Vec2::ZERO);
		ret.setDouble(rootPos.x + rect.getMidX(), "bodyX");
		ret.setDouble(rootPos.y + rect.origin.y + rect.size.height / 3.0f, "bodyY");
		ret.setDouble(rootPos.x + rect.origin.x + 12.0f, "leftBandX");
		return ret;
	});

	addCommand("builds", "How many times each panel's builder ran", [this](Value &&) {
		Value ret;
		for (auto &it : _builds) { ret.setInteger(it.second, it.first); }
		return ret;
	});

	addCommand("summary", "Checks and failures so far", [this](Value &&) {
		Value ret;
		ret.setInteger(_checks, "checks");
		ret.setInteger(_failures, "failures");
		return ret;
	});
}

void DockTabsLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - RootHeight));
}

} // namespace stappler::xenolith::app
