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
#include "XLUiStyleSystem.h" // setStyleVariable: the per-panel accent colour
#include "XLScene.h" // Scene::getContent, which the inspector commands are registered on
#include "XL2dSceneContent.h" // and the definition of what it answers, so it is a Node here
#include "XLSceneInspector.h"
#include "XL2dIconSprite.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

using basic2d::IconName;

/* The stylesheet.

Everything the demo looks like is here rather than in C++, which is the rule the kit is written to:
the layout builds structure, the sheet decides sizes and colours. Two blocks are worth reading even
if the rest is skimmed:

  * THE TWO KINDS OF TAB. `dock-tab.horizontal` is an icon, a title and a close affordance in a
    row; `dock-tab.vertical` is the same node with the title and the affordance switched off and
    padding that squares it around a bigger icon. Nothing in C++ distinguishes them - DockTabBar
    stamps the class from the side its frame declared, and a tab dragged into the other kind of
    strip is re-stamped on arrival. Its title is not lost with its label: DockTab publishes it as
    the tab's HINT, which is what the rail's icons show on hover;

  * `display: flex` IS WHAT LETS A RULE REACH A WIDGET'S OWN LAYOUT. The strips and the tabs build
    theirs in code, and padding, the gaps and the alignment are read only inside the resolver's
    flex branch - a rule without `display` never enters it and is silently ignored. Which is also
    why `flex-direction` is spelled out for both strips: a rule that opens that branch and says
    nothing about the direction gets the default, `row`, and a vertical strip would come out as a
    row of icons across the top of the sidebar. */
static constexpr auto s_css = StringView(R"css(
:root {
	--surface:  #0f0f13;
	--frame:    #24242d;
	--strip:    #1a1a21;
	--tab:      #2e2e38;
	--tab-on:   #3f3f4d;
	--outline:  #3c3c48;
	--accent:   #4d8dd8;
	--text:     #e8e8ee;
	--muted:    #9696a4;
}

/* ---- the frame ------------------------------------------------------- */

/* `#demo-root` has no `height`: nothing above it lays it out, so handleContentSizeDirty sizes it
   by hand. Everything below it is a flex item of something. */
#demo-root { display: flex; flex-direction: column; }
#demo-bar  { order: 0; -xl-z-order: 2; display: flex; flex-direction: row; align-items: center;
             column-gap: 10px; padding: 10px 14px; background-color: #0e0e12; }

/* The dock's owner is a flex ITEM and never a flex container: DockSystem writes every frame's
   geometry itself, and a LayoutSystem beside it would fight for the same children. */
#dock-root { order: 1; -xl-z-order: 1; flex-grow: 1; }

/* ---- the parking places ---------------------------------------------- */

dock-frame          { background-color: var(--frame); border-radius: 6px; }
dock-frame#sidebar  { background-color: #1c1c24; }
dock-frame-body     { display: flex; padding: 10px; }

dock-splitter          { background-color: var(--surface); border-radius: 3px; }
dock-splitter:hover,
dock-splitter.dragging { background-color: var(--accent); }

/* ---- the tab strips -------------------------------------------------- */

dock-tab-bar            { background-color: var(--strip); }
dock-tab-bar.horizontal { display: flex; flex-direction: row; align-items: stretch;
                          column-gap: 5px; padding: 5px; }
dock-tab-bar.vertical   { display: flex; flex-direction: column; align-items: stretch;
                          row-gap: 5px; padding: 5px; }

/* ---- kind one: a labelled tab ---------------------------------------- */

dock-tab                { display: flex; flex-direction: row; align-items: center;
                          justify-content: center; column-gap: 7px; padding: 6px 11px;
                          border-radius: 5px 5px 0px 0px; background-color: var(--tab); }
dock-tab:hover          { background-color: #33333f; }
dock-tab.active         { background-color: var(--tab-on); }
dock-tab > label        { color: var(--muted); font-size: 13px; }
dock-tab.active > label { color: var(--text); }
/* `order`, not `-xl-z-order`: ui::Button builds its label at a lower z than its icon, and z-order
   IS the placement order inside a flex run - so without this the glyph comes out AFTER the title.
   `order` is applied after that sort and moves the item without touching what draws on top. */
dock-tab > icon         { order: -1; width: 16px; height: 16px; color: var(--muted); }
dock-tab.active > icon  { color: var(--accent); }

/* An icon button has no intrinsic size, so without this the close affordance collapses to nothing
   and draws on top of the title; and a Panel with no fill declared is an opaque WHITE block, so
   without `transparent` it is a white square rather than a glyph. */
dock-tab-close       { width: 15px; height: 15px; background-color: transparent; }
dock-tab-close > icon{ width: 15px; height: 15px; color: var(--muted); }
dock-tab-close:hover > icon { color: var(--text); }

/* ---- kind two: a square icon in a rail -------------------------------- */

/* Below the rules above, and deliberately: `dock-tab.vertical > icon` and `dock-tab.active > icon`
   have the same specificity, so the later one wins for a tab that is both. */
dock-tab.vertical                 { padding: 9px; border-radius: 6px; }
dock-tab.vertical > label         { display: none; }
dock-tab.vertical > dock-tab-close{ display: none; }
dock-tab.vertical > icon          { width: 22px; height: 22px; }

/* ---- what a drag draws ------------------------------------------------ */

dock-drop-indicator       { background-color: rgba(77,141,216,.30);
                            outline-color: var(--accent); outline-width: 2px; border-radius: 4px; }
dock-drop-indicator.caret { background-color: var(--accent); outline-width: 0px; }
/* The ghost needs an explicit BOX. Nothing lays a drag decorator out - the drag system only moves
   it under the pointer - so a ghost with no size declared is a surface of zero extent with a
   caption drawn beside it, which is not obviously a bug when you first see it. */
dock-drag-ghost           { width: 150px; height: 32px; background-color: var(--tab-on);
                            border-radius: 6px; outline-color: var(--accent); outline-width: 1px; }
dock-drag-ghost > label   { color: var(--text); font-size: 13px; }
dock-drag-ghost > icon    { width: 16px; height: 16px; color: var(--accent); }

/* ---- a tab's hint ----------------------------------------------------- */

/* The stock ui::TooltipSystem hint, which is what the rail's icons answer with. It is an overlay
   on the SCENE CONTENT, which is why this sheet is installed there and not on the layout. */
tooltip                   { background-color: #2b2b36; outline-color: var(--outline);
                            outline-width: 1px; border-radius: 5px; }
label.xl-ui-tooltip-label { color: var(--text); font-size: 12px; }

/* ---- what a panel puts on screen -------------------------------------- */

/* Each body declares its own `--accent` with ui::setStyleVariable, so one rule paints six panels
   in six colours: a per-node property is inherited by the subtree and beats every rule that
   matched the same node. */
.panel-body       { display: flex; flex-direction: column; row-gap: 8px; }
.panel-head       { display: flex; flex-direction: row; align-items: center; column-gap: 9px; }
.panel-head > icon{ width: 22px; height: 22px; color: var(--accent); }
.panel-title      { color: var(--text); font-size: 15px; }
/* pushed to the end of the row by its own auto margin, which is the one thing `auto` is good for
   on a flex item - `justify-content` would have to move every item to place this one */
.panel-count      { margin-left: auto; color: var(--muted); font-size: 11px; }
/* ONE LINE, and short enough to stay one in the narrowest frame: a Label that wraps is measured
   at the width it had when it was measured, and a frame the user drags narrower re-wraps under a
   box that was sized for fewer lines. */
.panel-hint       { color: var(--muted); font-size: 12px; }
.panel-rule       { height: 3px; border-radius: 2px; background-color: var(--accent); }

/* ---- the chrome ------------------------------------------------------- */

.demo-title { color: var(--text); font-size: 15px; font-weight: bold; }
.status     { flex-grow: 1; color: #9ecbff; font-size: 13px; }

button       { height: 30px; display: flex; flex-direction: row; justify-content: center;
               align-items: center; padding: 0px 14px; border-radius: 5px;
               background-color: var(--tab); outline-color: var(--outline); outline-width: 1px; }
button:hover { background-color: var(--tab-on); }
button > label { color: var(--text); font-size: 13px; }
/* A DEFINITE basis, not `fit-content`. A ui::Button measured on demand answers with its LABEL's
   box - neither the `height` nor the `padding` declared above is in that answer - so a
   fit-content button comes out as a bare caption with no chrome at all. Give the basis and the
   whole rule applies. */
#demo-bar > button { flex: 0 0 108px; }
)css");

// What a panel IS, in one place: the id the layout and every command name it by, what its tab
// shows, and the colour its body is painted in.
struct PanelInfo {
	StringView id;
	StringView title;
	IconName icon;
	StringView accent;
	StringView hint;
	Size2 minSize;
};

static SpanView<PanelInfo> getPanels() {
	static const PanelInfo s_panels[] = {
		{"explorer", "Explorer", IconName::File_folder_solid, "#e0a33e", "Drag me out of the rail.",
			Size2(150.0f, 110.0f)},
		{"search", "Search", IconName::Action_search_solid, "#59b98a",
			"Hover a rail icon for its title.", Size2(150.0f, 110.0f)},
		{"history", "History", IconName::Action_history_solid, "#b98ad9",
			"Drop a tab here to make it an icon.", Size2(150.0f, 110.0f)},
		{"editor", "Editor", IconName::Action_code_solid, "#4d8dd8",
			"Drop a tab on this frame's edge to split it.", Size2(280.0f, 160.0f)},
		{"preview", "Preview", IconName::Action_visibility_solid, "#3fb0c8",
			"Switching tabs never rebuilds a panel.", Size2(240.0f, 140.0f)},
		{"console", "Console", IconName::Action_terminal_solid, "#7f8ea3",
			"Drag a divider to re-proportion two frames.", Size2(220.0f, 90.0f)},
		{"problems", "Problems", IconName::Alert_warning_solid, "#d97b5a",
			"Close me with the x, then press Reopen.", Size2(200.0f, 90.0f)},
	};
	return makeSpanView(s_panels, sizeof(s_panels) / sizeof(PanelInfo));
}

static const PanelInfo *getPanel(StringView id) {
	for (auto &it : getPanels()) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

/* The declared layout, as a spec.

Kept in one place so the Reset button, the self-check and init() agree on what "the demo" is. Three
frames: a sidebar carrying its strip on the LEFT - which is what makes it an icon rail - and beside
it an editor over a bottom band. Both of the others take the default Top, so the two kinds of strip
are on screen from the first frame drawn, with no input at all. */
static ui::DockLayoutSpec makeSpec() {
	using Spec = ui::DockLayoutSpec;

	/* The sidebar's share is deliberately tiny. A split's `ratio` divides what is left AFTER both
	sides got their minimums, so a sidebar declared 250pt wide with a 0.06 share stays a sidebar as
	the window grows instead of taking a quarter of every extra point. */
	return Spec::hsplit(0.06f,
			Spec::leaf({String("explorer"), String("search"), String("history")},
					{
						.name = String("sidebar"),
						.minSize = Size2(250.0f, 0.0f),
						.tabBarSide = ui::DockTabBarSide::Left,
					}),
			Spec::vsplit(0.68f,
					Spec::leaf({String("editor"), String("preview")}, {.name = String("editor")}),
					Spec::leaf({String("console"), String("problems")},
							{.name = String("bottom")})));
}

// A panel's content: the icon and the name it is known by, a rule in its own colour, and one line
// saying what to try with it. Built at most once, on first show, and kept across every move.
static Rc<Node> makePanelBody(const PanelInfo &info, size_t builds) {
	auto body = Rc<Node>::create();
	body->setName(info.id); // its CSS #id, so a rule can address one panel by name
	body->addStyleClass("panel-body");
	body->setAnchorPoint(Anchor::BottomLeft);

	// One declaration on this node paints its icon and its rule; the subtree inherits it, and it
	// beats the `--accent` the sheet declares on `:root`.
	ui::setStyleVariable(body, "--accent", info.accent);

	auto head = body->addChild(Rc<Node>::create(), ZOrder(1));
	head->addStyleClass("panel-head");

	auto icon = head->addChild(Rc<basic2d::IconSprite>::create(info.icon), ZOrder(1));
	icon->setType("icon"); // an IconSprite is not typed for CSS until somebody says so

	auto title = head->addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	title->addStyleClass("panel-title");
	title->setString(info.title);

	// The lazy builder's call count, stamped into what it built: a panel that was ever rebuilt
	// would say so on screen, which is a stronger claim than a number in a log.
	auto count = head->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
	count->addStyleClass("panel-count");
	count->setString(toString("built ", builds, "x"));

	auto rule = body->addChild(Rc<basic2d::Layer>::create(), ZOrder(2));
	rule->addStyleClass("panel-rule");

	auto hint = body->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
	hint->addStyleClass("panel-hint");
	hint->setString(info.hint);

	return body;
}

// How long the self-check waits for the layout pass that follows its reset. Shorter than the
// settle an inspector command waits out, so `dock.selfcheck` answers with its own result.
static constexpr float SelfCheckSettle = 0.05f;

static StringView getSideName(ui::DockTabBarSide side) {
	switch (side) {
	case ui::DockTabBarSide::Top: return StringView("top");
	case ui::DockTabBarSide::Bottom: return StringView("bottom");
	case ui::DockTabBarSide::Left: return StringView("left");
	case ui::DockTabBarSide::Right: return StringView("right");
	}
	return StringView("top");
}

static bool readSide(StringView name, ui::DockTabBarSide &out) {
	if (name == "top") {
		out = ui::DockTabBarSide::Top;
	} else if (name == "bottom") {
		out = ui::DockTabBarSide::Bottom;
	} else if (name == "left") {
		out = ui::DockTabBarSide::Left;
	} else if (name == "right") {
		out = ui::DockTabBarSide::Right;
	} else {
		return false;
	}
	return true;
}

} // namespace

StringView getDockDemoStylesheet() { return s_css; }

// ---- construction ----------------------------------------------------------------------------

bool DockDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	// The stylesheet and its resolver are NOT installed here - they are on the SceneContent, so
	// that they reach the hints a rail's icons pop out. See getDockDemoStylesheet().

	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.075f, 0.075f, 0.09f, 1.0f)), ZOrder(0));
	_background->setName("demo-background");

	// The flex column is a CHILD and not this node: a recursive StyleResolver re-resolves its
	// descendants and never its own owner, so a `display: flex` written for the node carrying it
	// is a rule that never runs.
	_root = addChild(Rc<Node>::create(), ZOrder(1));
	_root->setName("demo-root");

	buildControlBar();

	_dockRoot = _root->addChild(Rc<Node>::create(), ZOrder(1));
	_dockRoot->setName("dock-root");

	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(6.0f);

	// A panel that was closed is not gone: its node is kept, and Reopen brings back exactly what
	// was there. Recording the id here is what lets one button stand for any tab's x.
	_dock->setPanelClosedCallback([this](StringView id) {
		_lastClosed = id.str<Interface>();
		refreshStatus(toString("'", id, "' closed - Reopen brings it back"));
	});
	_dock->setPanelActivatedCallback(
			[this](StringView id) { refreshStatus(toString("'", id, "' activated")); });

	registerPanels();
	applyLayout();

	refreshStatus("drag a tab onto another frame, or onto its edge to split it");

	return true;
}

void DockDemoLayout::registerPanels() {
	for (auto &info : getPanels()) {
		ui::DockPanelDescriptor desc;
		desc.id = info.id.str<Interface>();
		desc.title = info.title.str<Interface>();
		desc.icon = info.icon;
		// The panel's floor, which is what strengthens the constraint of whatever frame it is
		// parked in - and through that of every split above it.
		desc.minSize = info.minSize;

		/* The builder takes no arguments and runs at most ONCE, on first show. Its node is then
		kept alive across every move, which is the point: a panel dragged into another frame - or
		into a strip of the other kind - arrives as the same node, with its state intact. The
		count is stamped into the body it builds, so a rebuild would be visible on screen. */
		desc.builder = [this, id = desc.id]() -> Rc<Node> {
			auto it = _builds.find(id);
			size_t next = (it != _builds.end()) ? it->second + 1 : 1;
			_builds.insert_or_assign(id, next);
			return makePanelBody(*getPanel(id), next);
		};

		_dock->registerPanel(sp::move(desc));
	}
}

void DockDemoLayout::applyLayout() { _dock->setLayout(makeSpec()); }

void DockDemoLayout::buildControlBar() {
	_controlBar = _root->addChild(Rc<Node>::create(), ZOrder(2));
	_controlBar->setName("demo-bar");

	auto title = _controlBar->addChild(Rc<basic2d::Label>::create(), ZOrder(0));
	title->addStyleClass("demo-title");
	title->setString("Dock");

	_statusLabel = _controlBar->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_statusLabel->addStyleClass("status");

	// Every control is the programmatic twin of something the pointer can do, and goes through the
	// same public operation. The frame is resolved BY NAME at click time, not captured here: a
	// Reset rebuilds the tree and every handle taken earlier dies with it.
	makeControl("Rail ⇄ tabs", [this] {
		auto frame = _dock->findFrameByName("sidebar");
		auto node = _dock->getFrameNode(frame);
		if (!node) {
			refreshStatus("no 'sidebar' frame - Reset first");
			return;
		}
		const bool rail = node->getTabBar()->isVertical();
		setFrameSide("sidebar", rail ? ui::DockTabBarSide::Top : ui::DockTabBarSide::Left);
		refreshStatus(rail ? "sidebar strip: labelled tabs on top"
						   : "sidebar strip: an icon rail on the left");
	});

	makeControl("Split editor", [this] {
		auto frame = _dock->findFrameByName("editor");
		// splitFrameWithPanel is exactly what a drop on an edge band commits, panel and all
		if (_dock->splitFrameWithPanel(frame, ui::DockAxis::Horizontal, false, "preview").empty()) {
			refreshStatus("could not split 'editor' - Reset first");
		} else {
			refreshStatus("'preview' carved a frame out of 'editor'");
		}
	});

	makeControl("Reopen", [this] {
		if (_lastClosed.empty()) {
			refreshStatus("nothing has been closed yet - try a tab's x");
			return;
		}
		if (_dock->openPanel(_lastClosed)) {
			refreshStatus(toString("'", _lastClosed, "' reopened - and never rebuilt"));
			_lastClosed.clear();
		} else {
			refreshStatus("could not reopen - Reset first");
		}
	});

	makeControl("Reset", [this] {
		applyLayout();
		_lastClosed.clear();
		refreshStatus("layout reset");
	});

	makeControl("Self-check", [this] { runSelfCheck(); });
}

ui::Button *DockDemoLayout::makeControl(StringView label, Function<void()> &&action) {
	// The z-order counts up from the title so the buttons keep the order they were declared in:
	// inside a flex row the child order IS the placement order.
	auto button = _controlBar->addChild(Rc<ui::Button>::create(sp::move(action)),
			ZOrder(int32_t(_controlBar->getChildren().size())));
	button->setString(label);
	return button;
}

void DockDemoLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	// The only two nodes placed by hand, and both for the same reason: SceneLayout2d carries no
	// LayoutSystem, so nothing above them lays them out. Everything below `#demo-root` is a flex
	// item of something, and hand-placing one of those would fight the layout pass.
	for (auto node : {_background, _root}) {
		if (node) {
			node->setAnchorPoint(Anchor::BottomLeft);
			node->setPosition(Vec2::ZERO);
			node->setContentSize(getContentSize());
		}
	}
}

// ---- operations ------------------------------------------------------------------------------

bool DockDemoLayout::setFrameSide(StringView frameName, ui::DockTabBarSide side) {
	auto frame = _dock->findFrameByName(frameName);
	auto node = _dock->getFrameNode(frame);
	if (!node) {
		return false;
	}

	// The whole params block, with one field changed: the tree is the source of truth for a
	// parking place's name, floor and flags as well, and dropping them here would silently
	// un-declare them.
	auto params = node->getParams();
	params.tabBarSide = side;
	return _dock->setFrameParams(frame, params);
}

bool DockDemoLayout::commitDrop(StringView panelId, const ui::DockDropTarget &target) {
	using Kind = ui::DockDropTarget::Kind;

	switch (target.kind) {
	case Kind::None: return false;
	case Kind::Center: return _dock->movePanel(panelId, target.frame);
	case Kind::TabStrip: return _dock->movePanel(panelId, target.frame, target.tabIndex);
	default:
		// every split zone: the frame is subdivided and the dragged panel takes the side that was
		// dropped on. `isFirst` is the low side of the axis - left, or TOP.
		auto created = _dock->splitFrameWithPanel(target.frame, target.getAxis(), target.isFirst(),
				panelId);
		return !created.empty();
	}
}

void DockDemoLayout::refreshStatus(StringView lastAction) {
	if (!_statusLabel || !_dock) {
		return;
	}

	if (!lastAction.empty()) {
		_lastAction = lastAction.str<Interface>();
	}

	_statusLabel->setString(toString("frames: ", _dock->getTree().getLeafCount(), "   |   ",
			_lastAction.empty() ? StringView("idle") : StringView(_lastAction)));
}

size_t DockDemoLayout::buildCount(StringView id) const {
	auto it = _builds.find(id.str<Interface>());
	return (it != _builds.end()) ? it->second : 0;
}

// ---- the inspector ---------------------------------------------------------------------------

void DockDemoLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	_inspectorScene = scene;
	registerCommands();

	runSelfCheck();
}

void DockDemoLayout::handleExit() {
	// A lambda that captured a destroyed layout is a dangling call from the inspector socket, so
	// the commands go down with the layout rather than with the scene.
	if (!_inspectorCommands.empty()) {
		if (_inspectorScene) {
			if (auto i = inspector::get(_inspectorScene->getContent())) {
				for (auto &it : _inspectorCommands) { i->removeCommand(it); }
			}
		}
		_inspectorCommands.clear();
	}
	_inspectorScene = nullptr;

	basic2d::SceneLayout2d::handleExit();
}

void DockDemoLayout::addCommand(StringView name, StringView description,
		Function<Value(const Value &)> &&handler) {
	if (!_inspectorScene || !handler) {
		return;
	}

	auto full = toString("dock.", name);
	if (!inspector::addCommand(_inspectorScene->getContent(), full, description,
				[this, handler = sp::move(handler)](Value &&args,
						Function<void(Value &&)> &&done) mutable {
		// CONST, and that is what keeps a missing argument from being a crash: the non-const
		// data::Value getters assert in debug on a key that is not there, where a read through a
		// const reference answers an empty value.
		const Value &in = args;
		auto result = handler(in);

		/* Answer only once the change has been on screen for a moment. Every action here moves
		geometry, and a mutation writes the tree immediately while the RECTS follow on the next
		layout pass - so a reply sent now would be read against the previous frame's arrangement.
		This layout holds a RenderContinuously, so the delay is real rendering time. */
		runAction(Rc<Sequence>::create(0.15f,
				Function<void()>([done = sp::move(done), result = sp::move(result)]() mutable {
			done(sp::move(result));
		})));
	})) {
		return; // no inspector on this scene - the app was not built with one
	}

	_inspectorCommands.emplace_back(sp::move(full));
}

ui::DockNodeHandle DockDemoLayout::resolveFrame(const Value &args) const {
	if (StringView name = args.getString("frame"); !name.empty()) {
		return _dock->findFrameByName(name);
	}
	if (StringView panel = args.getString("panel"); !panel.empty()) {
		return _dock->findFrameForPanel(panel);
	}
	return ui::DockNodeHandle();
}

Value DockDemoLayout::encodeTree() const {
	Value slots(Value::Type::ARRAY);

	// Depth-first from the root, a split before its children: the order a dump wants, and the
	// only order in which the arrangement can be read back. The nodes' order among the dock root's
	// CHILDREN means nothing - sortAllChildren is unstable.
	_dock->getTree().eachInOrder([&](const ui::DockTreeNode &n) {
		Value slot;
		slot.setInteger(n.self.index, "index");
		slot.setValue(Value{Value(n.rect.origin.x), Value(n.rect.origin.y),
						  Value(n.rect.size.width), Value(n.rect.size.height)},
				"rect");
		slot.setValue(Value{Value(n.minSize.width), Value(n.minSize.height)}, "min");

		if (n.isSplit()) {
			slot.setString("split", "kind");
			slot.setString(n.axis == ui::DockAxis::Horizontal ? "h" : "v", "axis");
			slot.setDouble(n.ratio, "ratio");
			slots.addValue(sp::move(slot));
			return;
		}

		slot.setString("frame", "kind");
		slot.setString(n.params.name, "name");
		slot.setString(getSideName(n.params.tabBarSide), "side");
		slot.setInteger(int64_t(n.active), "active");

		// The tabs AS RENDERED, not as the tree describes them: the class is what the stylesheet
		// keys the two kinds off, and the size is what a square icon proves itself by.
		Value tabs(Value::Type::ARRAY);
		if (auto frame = _dock->getFrameNode(n.self); frame && frame->getTabBar()) {
			for (auto &tab : frame->getTabBar()->getTabs()) {
				Value entry;
				entry.setString(tab->getPanelId(), "id");
				entry.setString(tab->getString(), "title");
				entry.setBool(tab->isActive(), "active");
				entry.setString(tab->hasStyleClass("vertical") ? "vertical" : "horizontal", "kind");
				entry.setValue(Value{Value(tab->getContentSize().width),
								   Value(tab->getContentSize().height)},
						"size");
				tabs.addValue(sp::move(entry));
			}
		}
		slot.setValue(sp::move(tabs), "tabs");
		slots.addValue(sp::move(slot));
	});

	return slots;
}

void DockDemoLayout::registerCommands() {
	addCommand("state", "The whole tree: every slot, and a frame's tabs as they are rendered",
			[this](const Value &) {
		Value ret;
		ret.setValue(encodeTree(), "slots");
		ret.setInteger(int64_t(_dock->getTree().getLeafCount()), "frames");
		ret.setString(_lastAction, "lastAction");
		ret.setString(_lastClosed, "lastClosed");

		Value builds;
		for (auto &it : _builds) { builds.setInteger(int64_t(it.second), it.first); }
		ret.setValue(sp::move(builds), "builds");

		ret.setInteger(int64_t(_checks), "checks");
		ret.setInteger(int64_t(_failures), "failures");
		return ret;
	});

	addCommand("activate", "Bring one panel forward: {panel}", [this](const Value &args) {
		Value ret;
		ret.setBool(_dock->activatePanel(args.getString("panel")), "ok");
		return ret;
	});

	addCommand("move", "Park a panel in a frame: {panel, frame, index}", [this](const Value &args) {
		Value ret;
		auto target = _dock->findFrameByName(args.getString("frame"));
		if (target.empty()) {
			ret.setString("no such frame", "error");
			return ret;
		}
		auto index = args.hasValue("index") ? size_t(args.getInteger("index")) : maxOf<size_t>();
		ret.setBool(_dock->movePanel(args.getString("panel"), target, index), "ok");
		refreshStatus(toString("'", args.getString("panel"), "' moved into '",
				args.getString("frame"), "'"));
		return ret;
	});

	addCommand("split", "Subdivide a frame, optionally taking a panel with it: "
					"{frame, axis: h|v, first, panel}",
			[this](const Value &args) {
		Value ret;
		auto frame = resolveFrame(args);
		if (frame.empty()) {
			ret.setString("no such frame", "error");
			return ret;
		}
		auto axis = (args.getString("axis") == "v") ? ui::DockAxis::Vertical
													: ui::DockAxis::Horizontal;
		auto created = _dock->splitFrameWithPanel(frame, axis, args.getBool("first"),
				args.getString("panel"));
		ret.setBool(!created.empty(), "ok");
		ret.setInteger(created.index, "frame");
		refreshStatus("frame split");
		return ret;
	});

	addCommand("side", "Turn a frame's strip: {frame, side: top|bottom|left|right}",
			[this](const Value &args) {
		Value ret;
		ui::DockTabBarSide side = ui::DockTabBarSide::Top;
		if (!readSide(args.getString("side"), side)) {
			ret.setString("side must be top, bottom, left or right", "error");
			return ret;
		}
		StringView name = args.getString("frame");
		ret.setBool(setFrameSide(name, side), "ok");
		refreshStatus(toString("'", name, "' strip -> ", getSideName(side)));
		return ret;
	});

	addCommand("close", "Close one panel: {panel}", [this](const Value &args) {
		Value ret;
		ret.setBool(_dock->closePanel(args.getString("panel")), "ok");
		return ret;
	});

	addCommand("open", "Open a closed panel, optionally into a named frame: {panel, frame}",
			[this](const Value &args) {
		Value ret;
		ret.setBool(_dock->openPanel(args.getString("panel"), resolveFrame(args)), "ok");
		return ret;
	});

	/* The two that make drag and drop scriptable. `hittest` is the pure question a drag asks on
	every pointer move - it resolves a point in the DOCK ROOT's own coordinate space against the
	tree, without a scene hit test and without an input event; `drop` commits what it answered,
	through the same public operations the dock's own drop slot uses. Together they exercise the
	edge-band split that is otherwise reachable only by dragging. */
	addCommand("hittest", "What a drop at a dock-root point would do: {x, y, panel}",
			[this](const Value &args) {
		auto t = _dock->hitTest(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))),
				args.getString("panel"));
		Value ret;
		ret.setInteger(toInt(t.kind), "kind");
		ret.setBool(t.isSplit(), "isSplit");
		ret.setInteger(t.frame.index, "frame");
		ret.setInteger(int64_t(t.tabIndex), "tabIndex");
		ret.setValue(Value{Value(t.highlight.origin.x), Value(t.highlight.origin.y),
						 Value(t.highlight.size.width), Value(t.highlight.size.height)},
				"highlight");
		return ret;
	});

	addCommand("drop", "Commit that drop: {panel, x, y}", [this](const Value &args) {
		Value ret;
		StringView panel = args.getString("panel");
		auto t =
				_dock->hitTest(Vec2(float(args.getDouble("x")), float(args.getDouble("y"))), panel);
		ret.setInteger(toInt(t.kind), "kind");
		ret.setBool(commitDrop(panel, t), "ok");
		refreshStatus(toString("'", panel, "' dropped"));
		return ret;
	});

	addCommand("reset", "Restore the declared layout", [this](const Value &) {
		applyLayout();
		_lastClosed.clear();
		refreshStatus("layout reset");
		return Value(true);
	});

	addCommand("selfcheck", "Re-run the structural checks", [this](const Value &) {
		runSelfCheck();
		Value ret;
		ret.setInteger(int64_t(_checks), "checks");
		ret.setInteger(int64_t(_failures), "failures");
		return ret;
	});
}

// ---- the self-check --------------------------------------------------------------------------

void DockDemoLayout::expect(bool condition, StringView message) {
	++_checks;
	if (!condition) {
		++_failures;
		log::source().warn("DockExample", "FAILED: ", message);
	}
}

void DockDemoLayout::runSelfCheck() {
	applyLayout();

	/* ONE PASS LATER. setLayout writes the tree immediately, but the RECTS follow on the next
	layout pass - and check 6 asks the hit test where the editor frame's left edge is, which before
	that pass is (0,0,0,0) for every slot. This layout holds a RenderContinuously, so the delay is
	real rendering time; it is also shorter than the settle an inspector command waits out, so
	`dock.selfcheck` still answers with the result of the run it started. */
	runAction(Rc<Sequence>::create(SelfCheckSettle, Function<void()>([this] { runChecks(); })));
}

void DockDemoLayout::runChecks() {
	_checks = 0;
	_failures = 0;

	auto panelsIn = [this](ui::DockNodeHandle h) {
		auto span = _dock->getPanelsInFrame(h);
		return Vector<String>(span.begin(), span.end());
	};

	// 1. The declared frames exist and are found BY NAME - which is also their CSS #id, and the
	//    only identity that survives a Reset.
	auto sidebar = _dock->findFrameByName("sidebar");
	auto editor = _dock->findFrameByName("editor");
	auto bottom = _dock->findFrameByName("bottom");
	// DockNodeHandle converts to bool only EXPLICITLY, so the check reads as a negated empty().
	expect(!sidebar.empty(), "the 'sidebar' frame was not found by name");
	expect(!editor.empty(), "the 'editor' frame was not found by name");
	expect(!bottom.empty(), "the 'bottom' frame was not found by name");
	if (sidebar.empty() || editor.empty() || bottom.empty()) {
		return;
	}

	// 2. Every panel is parked where the spec put it, in the order the spec gave.
	expect(panelsIn(sidebar) == Vector<String>{"explorer", "search", "history"},
			"'sidebar' does not hold the three rail panels in order");
	expect(panelsIn(editor) == Vector<String>{"editor", "preview"},
			"'editor' does not hold its two tabs in order");
	expect(panelsIn(bottom) == Vector<String>{"console", "problems"},
			"'bottom' does not hold its two tabs in order");

	// 3. THE TWO KINDS OF STRIP, which is what this demo is about. The sidebar's is vertical and
	//    every tab in it is marked so - the class is what the stylesheet narrows to an icon.
	auto sidebarNode = _dock->getFrameNode(sidebar);
	auto editorNode = _dock->getFrameNode(editor);
	expect(sidebarNode && sidebarNode->getTabBar()->isVertical(),
			"the 'sidebar' strip is not vertical");
	expect(editorNode && !editorNode->getTabBar()->isVertical(),
			"the 'editor' strip is not horizontal");

	if (sidebarNode && editorNode) {
		for (auto &tab : sidebarNode->getTabBar()->getTabs()) {
			expect(tab->hasStyleClass("vertical"),
					toString("rail tab '", tab->getPanelId(), "' is not marked vertical"));
		}
		for (auto &tab : editorNode->getTabBar()->getTabs()) {
			expect(tab->hasStyleClass("horizontal"),
					toString("tab '", tab->getPanelId(), "' is not marked horizontal"));
		}
	}

	// 4. THE LAZY BUILDER, in its two halves. "Never twice" is the promise that holds for the life
	//    of the application, whatever has been dragged where; "not yet" is only true of a panel
	//    nothing has shown, so it is asserted on the first run and never again.
	for (auto &info : getPanels()) {
		expect(buildCount(info.id) <= 1,
				toString("'", info.id, "' was built more than once - lazy content is broken"));
	}
	expect(buildCount("explorer") == 1, "'explorer' is showing but was not built");
	expect(buildCount("editor") == 1, "'editor' is showing but was not built");
	expect(buildCount("console") == 1, "'console' is showing but was not built");
	if (!_checked) {
		expect(buildCount("search") == 0, "the hidden rail tab 'search' must not be built yet");
		expect(buildCount("preview") == 0, "the hidden tab 'preview' must not be built yet");
	}
	_checked = true;

	// 5. A PANEL CARRIED BETWEEN THE TWO KINDS OF STRIP IS NOT REBUILT. Its tab is a different
	//    node in the other strip - tabs belong to a frame - but the panel's content is the one it
	//    already had, which is the promise the whole registry exists for.
	expect(_dock->movePanel("explorer", editor), "moving 'explorer' into 'editor' failed");
	expect(buildCount("explorer") == 1, "moving 'explorer' rebuilt it - lazy content is broken");

	// and the tab it arrived as is one of the receiving strip's kind, not the rail's
	if (auto tabs = editorNode ? editorNode->getTabBar() : nullptr) {
		bool marked = false;
		for (auto &tab : tabs->getTabs()) {
			if (tab->getPanelId() == "explorer") {
				marked = tab->hasStyleClass("horizontal");
			}
		}
		expect(marked, "'explorer' did not arrive as a horizontal tab");
	}

	// 6. AN EDGE DROP IS A SPLIT. The hit test answers a point in the dock root's own space; the
	//    left edge band of the editor frame must resolve to SplitLeft, and committing it adds
	//    exactly one frame. This is the drag-to-split path, minus the dragging.
	const auto rect = _dock->getTree().get(editor)->rect;
	const auto edge = Vec2(rect.origin.x + 12.0f, rect.getMidY());
	auto target = _dock->hitTest(edge, "console");
	expect(target.kind == ui::DockDropTarget::Kind::SplitLeft,
			"a drop on the editor's left edge does not resolve to a split");

	const auto before = _dock->getTree().getLeafCount();
	expect(commitDrop("console", target), "committing the edge drop failed");
	expect(_dock->getTree().getLeafCount() == before + 1, "the edge drop did not add one frame");
	expect(buildCount("console") == 1, "the edge drop rebuilt 'console'");

	// 7. save/restore round-trips the shape and the membership - and the tab bar side with them,
	//    which is what keeps the rail a rail across a restart.
	auto saved = _dock->save();
	expect(_dock->restore(saved), "restoring a freshly-saved layout failed");
	if (auto restored = _dock->getFrameNode(_dock->findFrameByName("sidebar"))) {
		expect(restored->getTabBar()->isVertical(), "restore lost the sidebar's vertical strip");
	} else {
		expect(false, "restore lost the 'sidebar' frame");
	}

	// Put the demo back where it was meant to start.
	applyLayout();

	if (_failures == 0) {
		log::source().warn("DockExample", "self-check: ", _checks, " checks, 0 failures");
		refreshStatus(toString("self-check passed: ", _checks, " checks"));
	} else {
		log::source().error("DockExample", "self-check: ", _checks, " checks, ", _failures,
				" failures");
		refreshStatus(toString("self-check: ", _failures, " FAILURES - see the log"));
	}
}

} // namespace stappler::xenolith::examples
