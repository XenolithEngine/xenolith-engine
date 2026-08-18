/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "dock/DockDemoLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h" // StyleSystem: the rule-supplying half of the stylesheet pair
#include "XLUiButton.h"
#include "XLUiLayoutSystem.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The stylesheet. It is the whole reason a frame's `name` exists: it becomes the node's CSS #id,
// so `dock-frame#sidebar` below addresses one parking place by name and gives it its own fill.
// Everything else the dock paints (tab strip, tabs, splitter) takes a rule from the same sheet.
static constexpr auto s_css = StringView(R"css(
/* every frame gets a base fill; the named ones override it */
dock-frame         { background-color: #232323; }

/* one parking place addressed by its declared name -> its CSS id */
dock-frame#sidebar  { background-color: #1b3a5c; }
dock-frame#problems { background-color: #3a2417; }

/* the strip is self-sized from its tabs (flex-basis: fit-content), so it must be a flex item of
   a run that has one - DockFrame builds that run itself, and this rule only refines padding. */
dock-frame-body    { display: flex; padding: 6px; }

/* the tab strip, horizontal and vertical alike (the `.vertical` class is set by the frame) */
dock-tab-bar       { background-color: #171717; }
dock-tab           { display: flex; padding: 4px 10px; background-color: #2b2b2b; }
dock-tab.active    { background-color: #383838; }
dock-tab > label   { color: #c8c8c8; font-size: 13px; }

/* the divider between two frames, and the drop indicator a drag paints */
dock-splitter      { background-color: #2a2a2a; }
dock-drop-indicator{ background-color: #3d7ecf; }

/* the demo's own chrome (control bar, status strip) - not dock types at all */
.demo-bar          { display: flex; align-items: center; column-gap: 10px; padding: 8px 12px;
                     background-color: #141414; }
button             { width: 150px; height: 34px; background-color: #1e88e5; border-radius: 6px;
                     display: flex; justify-content: center; align-items: center; }
button > label     { color: #ffffff; font-size: 13px; }
.status           { color: #9ecbff; font-size: 14px; }
)css");

// The declared layout, as a spec. Kept in one place so buildDock() and the reset button agree on
// what "the demo" is - the self-check restores exactly this after it has exercised moves/splits.
static ui::DockLayoutSpec s_spec() {
	using Spec = ui::DockLayoutSpec;

	// A left sidebar (its tab strip on the LEFT edge), and to its right an editor over a bottom
	// band that already carries two tabs. Three frames, one of them with two panels: every feature
	// of DockFrame has something to show before any input at all.
	return Spec::hsplit(0.24f,
			Spec::leaf({String("explorer")}, {.name = String("sidebar"), .tabBarSide = ui::DockTabBarSide::Left}),
			Spec::vsplit(0.72f,
					Spec::leaf({String("editor")}, {.name = String("main")}),
					Spec::leaf({String("console"), String("problems")}, {.name = String("bottom")})));
}

// The lazy panel content: a coloured swatch and the panel's own name. The node is built at most
// once (on first show) and then kept across every move, which is what DockSystem promises - the
// self-check counts the builds to prove it holds here too.
static Rc<Node> makePanelBody(StringView title, Color4F color) {
	auto body = Rc<Node>::create();
	body->setAnchorPoint(Anchor::BottomLeft);

	auto swatch = body->addChild(Rc<basic2d::Layer>::create(color), ZOrder(1));
	swatch->setName("panel-swatch");

	auto caption = body->addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	caption->setString(title);
	return body;
}

} // namespace

bool DockDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	// The stylesheet is a system that only SUPPLIES rules; the recursive resolver below is what
	// actually applies them to every node in this subtree (frames, tabs, buttons and their labels).
	addSystem(Rc<ui::StyleSystem>::create(s_css));
	addSystem(Rc<ui::StyleResolver>::create(true));

	_background = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(0));
	_background->setAnchorPoint(Anchor::BottomLeft);

	buildDock(); // registers the panels, builds the split tree, the control bar and runs the self-check

	// The engine renders on demand: with nothing dirty it stops producing frames, and a callback
	// that changes state off-screen is only picked up the next time something wakes the loop. This
	// demo is meant to be watched (buttons, drags), so hold the loop open like every other test
	// layout does - RenderContinuously draws nothing and damages nothing, it only keeps frames coming.
	runAction(Rc<RenderContinuously>::create());

	return true;
}

void DockDemoLayout::buildDock() {
	if (_dock) {
		_dock->handleRemoved();
		_dock = nullptr;
	}

	// The dock's owner must NOT carry a LayoutSystem of its own - the system writes every frame's
	// geometry directly, and two writers would fight (handleAdded asserts it). A plain Node is
	// exactly that: no layout of its own, just a parent for the flat frames. DockSystem distributes
	// over _owner->getContentSize(), so this node must be sized in handleContentSizeDirty().
	if (!_dockRoot) {
		_dockRoot = addChild(Rc<Node>::create(), ZOrder(1));
		_dockRoot->setAnchorPoint(Anchor::BottomLeft);
	}

	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(6.0f);

	// --- the panel registry --------------------------------------------------------------
	// id, title, icon and minimum are declared once; the builder runs at most once on first show.
	auto registerPanel = [this](StringView id, StringView title, basic2d::IconName icon, Size2 minSize, Color4F color) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = title.str<Interface>();
		desc.icon = icon;
		desc.minSize = minSize;
		// the builder takes no arguments: it is called at most once on first show and its node
		// is then kept alive across every move, so it must not depend on where the panel sits.
		desc.builder = [this, key = id.str<Interface>(), title = title.str<Interface>(), color]() -> Rc<Node> {
			auto it = _builds.find(key);
			size_t next = (it != _builds.end()) ? it->second + 1 : 1;
			_builds.insert_or_assign(key, next);
			return makePanelBody(title, color);
		};
		_dock->registerPanel(sp::move(desc));
	};

	registerPanel("explorer", "Explorer", basic2d::IconName::File_folder_solid, Size2(180.0f, 120.0f), Color4F(0.16f, 0.55f, 0.75f, 1.0f));
	registerPanel("editor", "Editor", basic2d::IconName::Action_code_solid, Size2(320.0f, 200.0f), Color4F(0.20f, 0.60f, 0.40f, 1.0f));
	registerPanel("console", "Console", basic2d::IconName::Action_list_solid, Size2(240.0f, 100.0f), Color4F(0.55f, 0.35f, 0.75f, 1.0f));
	registerPanel("problems", "Problems", basic2d::IconName::Action_info_solid, Size2(200.0f, 90.0f), Color4F(0.80f, 0.55f, 0.15f, 1.0f));

	// --- the initial split tree ----------------------------------------------------------
	bool ok = _dock->setLayout(s_spec());
	if (ok) {
		runSelfCheck();
	}

	makeControlBar();
	refreshStatus("layout set");
}

void DockDemoLayout::makeControlBar() {
	// The row is a flex container: it sizes itself from its items, so nothing here has to know
	// how many buttons there are or how wide they turned out.
	if (!_controlBarRow) {
		_controlBarRow = addChild(Rc<Node>::create(), ZOrder(2));
		_controlBarRow->setAnchorPoint(Anchor::TopLeft);
		_controlBarRow->addStyleClass("demo-bar");
		_controlBarRow->addSystem(Rc<ui::LayoutSystem>::create(ui::FlexLayoutInfo{
				.direction = ui::FlexDirection::Row,
				.alignItems = ui::FlexAlign::Center,
				.columnGap = 10.0f,
		}));

		_statusLabel = _controlBarRow->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		_statusLabel->addStyleClass("status");
	}

	// The frame handles are resolved at CLICK time, not here: a Reset rebuilds the tree and every
	// handle captured earlier dies with it - by name survives that.
	makeControl("Activate editor", [this] {
		if (_dock->activatePanel("editor")) {
			refreshStatus("'editor' activated");
		} else {
			refreshStatus("'editor' is not open");
		}
	});
	makeControl("Move explorer -> main", [this] {
		auto main = _dock->findFrameByName("main");
		if (main.empty()) {
			refreshStatus("no 'main' frame to move into - Reset first");
			return;
		}
		if (_dock->movePanel("explorer", main)) {
			refreshStatus("'explorer' moved into 'main'");
		} else {
			refreshStatus("could not move 'explorer'");
		}
	});
	makeControl("Split main", [this] {
		auto main = _dock->findFrameByName("main");
		if (auto created = _dock->splitFrame(main, ui::DockAxis::Horizontal, true); !created.empty()) {
			refreshStatus("a new frame carved out of 'main'");
		} else {
			refreshStatus("could not split 'main' - Reset first if it is gone");
		}
	});
	makeControl("Close / reopen", [this] {
		if (_lastClosed.empty()) {
			// first click: close the demo's hidden tab - the one thing that is NOT showing, so a
			// closed panel and its folded frame are both visible in one motion.
			if (!_dock->closePanel("problems")) {
				refreshStatus("'problems' was not open");
				return;
			}
			_lastClosed = "problems";
			refreshStatus(toString("'", _lastClosed, "' closed (its empty frame folded) - click again to reopen"));
			return;
		}

		if (_dock->isPanelOpen(_lastClosed)) {
			// still parked somewhere: just bring it forward.
			_dock->activatePanel(_lastClosed);
			refreshStatus(toString("'", _lastClosed, "' activated"));
			return;
		}

		// closed AND its frame folded away: open it back into whatever room is left - the system
		// resolves an empty target to the largest frame.
		if (_dock->openPanel(_lastClosed)) {
			String reopened = _lastClosed;
			_lastClosed.clear();
			refreshStatus(toString("'", reopened, "' reopened"));
		} else {
			refreshStatus("could not reopen - Reset first");
		}
	});
	makeControl("Reset layout", [this] { buildDock(); });

	if (_selfCheckDone && _failures == 0) {
		refreshStatus(toString("self-check passed: ", toInt(_checks), " checks, 0 failures"));
	} else if (_selfCheckDone) {
		refreshStatus(toString("self-check: ", toInt(_checks), " checks, ", toInt(_failures),
				" FAILURES - see the log"));
	} else {
		refreshStatus("layout set");
	}
}

ui::Button *DockDemoLayout::makeControl(StringView label, Function<void()> &&action) {
	auto button = _controlBarRow->addChild(Rc<ui::Button>::create(sp::move(action)), ZOrder(3));
	button->setString(label);
	return button;
}

void DockDemoLayout::refreshStatus(StringView lastAction) {
	if (!_statusLabel || !_dock) {
		return;
	}

	size_t frames = _dock->getTree().getLeafCount();
	auto out = toString("frames: ", toInt(frames), "   |   ", (lastAction.empty() ? StringView("idle") : lastAction));
	_statusLabel->setString(out);
}

size_t DockDemoLayout::buildCount(StringView id) const {
	auto it = _builds.find(id.str<Interface>());
	return (it != _builds.end()) ? it->second : 0;
}

void DockDemoLayout::runSelfCheck() {
	if (_selfCheckDone || !_dock) {
		return;
	}

	auto expect = [this](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("DockFrameExample", "self-check: ", what);
		}
	};

	auto panelsIn = [this](ui::DockNodeHandle h) {
		auto span = _dock->getPanelsInFrame(h);
		return Vector<String>(span.begin(), span.end());
	};

	// 1. The declared frames exist and were found by name - the whole point of giving them one at all.
	auto sidebar = _dock->findFrameByName("sidebar");
	auto main = _dock->findFrameByName("main");
	auto bottom = _dock->findFrameByName("bottom");
	// DockNodeHandle converts to bool only EXPLICITLY, so the check reads a negated empty().
	expect(!sidebar.empty(), "the 'sidebar' frame was not found by name");
	expect(!main.empty(), "the 'main' frame was not found by name");
	expect(!bottom.empty(), "the 'bottom' frame was not found by name");

	// 2. Each panel is parked where setLayout put it - the membership of every leaf.
	if (sidebar && main && bottom) {
		auto side = panelsIn(sidebar);
		expect(side.size() == 1 && side[0] == "explorer", "'explorer' is not the only panel in 'sidebar'");

		auto m = panelsIn(main);
		expect(m.size() == 1 && m[0] == "editor", "'editor' is not the only panel in 'main'");

		auto b = panelsIn(bottom);
		expect(b.size() == 2, "'bottom' does not hold two tabs");
		if (b.size() == 2) {
			expect(b[0] == "console" && b[1] == "problems", "'bottom' is in the wrong tab order");
		}
	}

	// 3. Only what is actually showing was built: 'explorer', 'editor' and the ACTIVE panel of the
	//    two-tab frame ('console'). 'problems' sits behind it and must not exist yet - that is the
	//    lazy builder, and rebuilding it later would be caught by its count.
	expect(buildCount("explorer") == 1, "'explorer' was not built exactly once");
	expect(buildCount("editor") == 1, "'editor' was not built exactly once");
	expect(buildCount("console") == 1, "'console' was not built exactly once");
	expect(buildCount("problems") == 0, "the hidden tab 'problems' must not be built yet");

	// 4. The active panel of a multi-tab frame is the one named by `active` in the spec: index 0.
	if (bottom) {
		auto node = _dock->getTree().get(bottom);
		expect(node && node->isLeaf() && node->active == 0, "'bottom' did not keep its active tab");
	}

	// 5. Moving a panel carries its node with it - the builder must NOT run again for 'explorer'.
	if (sidebar && main) {
		bool moved = _dock->movePanel("explorer", main);
		expect(moved, "moving 'explorer' into 'main' failed");

		if (moved) {
			auto afterMove = panelsIn(main);
			expect(afterMove.size() == 2 && afterMove[0] == "editor" && afterMove[1] == "explorer",
					"'explorer' did not land at the end of 'main's tab strip");

			// still exactly one build: a rebuild would make this two.
			expect(buildCount("explorer") == 1, "moving 'explorer' rebuilt it - lazy content is broken");

			// restore the declared layout so the demo starts where it was meant to start.
			_dock->setLayout(s_spec());
			expect(buildCount("explorer") == 1, "restoring the layout rebuilt 'explorer'");
		}
	}

	// 6. Splitting a frame adds exactly one leaf and never shrinks the dock's propagated minimum.
	//    `main` is re-resolved: check 5 ended with setLayout(), which rebuilds the tree, so every
	//    handle captured in step 1 is stale by now - by name stays valid across that.
	if (auto main = _dock->findFrameByName("main")) {
		auto before = _dock->getTree().getRootMinSize();
		auto newLeaf = _dock->splitFrame(main, ui::DockAxis::Horizontal, true);
		expect(newLeaf && _dock->getTree().isValid(newLeaf), "splitting 'main' produced no frame");

		if (newLeaf) {
			auto after = _dock->getTree().getRootMinSize();
			expect(after.width >= before.width - 0.5f, "splitting a frame shrank the dock minimum");
			expect(_dock->getTree().getLeafCount() == 4, "split did not add exactly one leaf");

			_dock->setLayout(s_spec());
			expect(_dock->getTree().getLeafCount() == 3, "restore after split left the wrong leaf count");
		}
	}

	// 7. save/restore round-trips shape and membership (the registry's titles/icons never travel).
	auto saved = _dock->save();
	bool restored = _dock->restore(saved);
	expect(restored, "restoring a freshly-saved layout failed");

	if (restored) {
		auto sidebar2 = _dock->findFrameByName("sidebar");
		auto bottom2 = _dock->findFrameByName("bottom");
		expect(sidebar2 && panelsIn(sidebar2).size() == 1 && panelsIn(sidebar2)[0] == "explorer",
				"restore lost 'explorer' in 'sidebar'");
		if (bottom2) {
			auto b = panelsIn(bottom2);
			expect(b.size() == 2, "restore did not keep both tabs of 'bottom'");
		}
	}

	_selfCheckDone = true;
	log::source().warn("DockFrameExample", "self-check: ", _checks, " checks, ", _failures, " failures");
}

void DockDemoLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	auto size = getContentSize();
	if (_background) {
		_background->setContentSize(size);
	}

	// The control row is a self-managed flex container, but nothing above it lays out its owner -
	// SceneLayout2d has no LayoutSystem of its own - so the strip gets an explicit size here:
	// full width at the top, and the dock takes whatever is left below.
	constexpr float kBarHeight = 52.0f;
	if (_controlBarRow) {
		_controlBarRow->setAnchorPoint(Anchor::TopLeft);
		_controlBarRow->setPosition(Vec2(16.0f, size.height - 16.0f));
		_controlBarRow->setContentSize(Size2(sprt::max(0.0f, size.width - 32.0f), kBarHeight));
	}

	if (_dockRoot) {
		auto dockSize = Size2(size.width, sprt::max(0.0f, size.height - kBarHeight));
		_dockRoot->setContentSize(dockSize);
	}

	refreshStatus();
}

} // namespace stappler::xenolith::examples
