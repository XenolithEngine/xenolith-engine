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

#include "agentchat/AgentChatLayout.h"

#include "XLUiStyleSystem.h" // the rule-supplying half of the stylesheet pair
#include "XLUiStyleResolver.h" // the half that actually applies them
#include "XLUiDockTypes.h"
#include "XLUiDockFrame.h"
#include "XLScene.h"
#include "XL2dSceneContent.h"
#include "XLAction.h"
#include "XL2dLayer.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* The frame and the dock. Everything else is declared beside the panel it paints.

Three rules here are not decoration:

  * `#dock-root` has no `display`, and that is deliberate. A dock writes the geometry of every frame
    and divider itself; a flex layout on the same node would write the same children's geometry
    too, and the dock asserts about it when it is added;
  * `dock-frame-body` and `dock-tab` must say `display: flex` in the same rule as their padding.
    Padding, gaps and alignment are read only inside the resolver's flex branch, and a rule that
    never declares `display` never enters it - the values are then silently dropped;
  * the tab's icon needs `order: -1` rather than a z-order. Inside a flex run z-order IS the
    placement order, and a button builds its label at a lower z than its icon, so without this the
    glyph lands after the title. */
static constexpr auto s_frameCss = StringView(R"css(
:root {
	--surface:   #15151a;
	--panel:     #1e1e25;
	--control:   #2a2a33;
	--outline:   #43434f;
	--accent:    #3d7ecf;
	--user-bg:   #2f5d9e;
	--agent-bg:  #26262e;
	--text:      #e8e8ec;
	--muted:     #9a9aa5;
	--danger:    #e5534b;
	--bar-h:     42px;
	--field-h:   34px;
}

/* `width`/`height` are not declared: nothing above this node lays it out, so
   handleContentSizeDirty sizes it by hand. */
#dock-root { background-color: var(--surface); }

dock-frame      { background-color: var(--panel); border-radius: 6px; }
dock-frame-body { display: flex; flex-direction: column; padding: 0px; }

/* One tab per frame, always active: this is a column heading, not a switch. */
dock-tab-bar            { background-color: #0f0f14; }
dock-tab-bar.horizontal { display: flex; flex-direction: row; align-items: stretch;
                          column-gap: 4px; padding: 4px 6px; }
dock-tab       { display: flex; flex-direction: row; align-items: center; column-gap: 7px;
                 padding: 5px 10px; border-radius: 4px 4px 0px 0px; }
dock-tab.active{ background-color: var(--panel); }
dock-tab > label        { color: var(--muted); font-size: 13px; white-space: nowrap; }
dock-tab.active > label { color: var(--text); }
dock-tab > icon         { order: -1; width: 16px; height: 16px; color: var(--muted); }
dock-tab.active > icon  { color: var(--accent); }

/* The thickness is a system parameter, not a style; only the colour belongs here. */
dock-splitter          { background-color: #0f0f14; }
dock-splitter:hover,
dock-splitter.dragging { background-color: var(--accent); }

/* ---- the card, shared by both columns --------------------------------- */

/* NOT a flex container, and no padding declared: ChatBubble places its own labels. A flex column
   would hand each label a box measured before the text was wrapped, and the label would then
   report that box instead of the height its text came to - which is the one number the card needs.
   Both sizes are definite and both are published by the card as custom properties: a Panel is a
   sprite, so one sized `auto` collapses to nothing however much text it holds. */
panel.bubble       { width: var(--bubble-width, 200px); height: var(--bubble-height, 40px);
                     flex: 0 0 auto; border-radius: 10px; }
/* The width is the wrap width the card publishes. The COLOUR is here rather than only on the
   per-role rules below so that a card whose own rule is missing still comes out readable: a Label
   with no rule of its own draws in the engine's default ink, which is black, and black on these
   panels is invisible rather than merely wrong. */
panel.bubble > label { width: var(--wrap-width, 300px); color: var(--text); }
panel.bubble.error { background-color: #3a1f20;
                     outline-width: 1px; outline-color: var(--danger); }
)css");

// Wide enough for a note to read as a note, narrow enough to stay a sidebar.
static constexpr float s_savedMinWidth = 260.0f;
static constexpr float s_chatMinWidth = 420.0f;

/* The share of what is left AFTER both minimums, not of the whole width. At 1100pt that leaves
about 414pt to divide, so the saved column opens near 320pt and keeps 15% of whatever the window
gains - it stays a sidebar instead of becoming a second half of the screen. */
static constexpr float s_savedShare = 0.15f;

static constexpr float s_splitterThickness = 6.0f;

// The only flag either frame keeps. No AllowSplit, AllowDrop or AllowClose: the arrangement that
// opens is the arrangement that stays, and Permanent says both places exist unconditionally.
static constexpr auto s_frameFlags =
		ui::DockFrameFlags::AllowResize | ui::DockFrameFlags::Permanent;

// Neither Closable nor Movable: no close affordance is built, and a drag is refused at its start.
static constexpr auto s_panelFlags =
		ui::DockPanelFlags::OpenByDefault | ui::DockPanelFlags::Singleton;

static StringView getFrameKindName(const ui::DockTreeNode &node) {
	return node.isSplit() ? StringView("split") : StringView("frame");
}

} // namespace

StringView getAgentChatStylesheet() {
	/* A function-local static: built on the first call, with no dependency on the order global
	constructors happen to run in. */
	static String s_sheet =
			toString(s_frameCss, getChatPanelStylesheet(), getSavedPanelStylesheet());
	return s_sheet;
}

bool AgentChatLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	_background =
			addChild(Rc<basic2d::Layer>::create(Color4F(0.08f, 0.08f, 0.10f, 1.0f)), ZOrder(0));
	_background->setName("chat-background");

	/* Built here rather than inside the builders. The dock's builder runs at most once and at the
	moment the panel is first shown, which is later than the first inspector command and later than
	the first thing the agent might save - both of which would then find nothing. */
	_saved = Rc<SavedMessages>::create();
	_savedPanel = Rc<SavedPanel>::create(Rc<SavedMessages>(_saved));
	_chatPanel = Rc<ChatPanel>::create(Rc<SavedMessages>(_saved));

	_dockRoot = addChild(Rc<Node>::create(), ZOrder(1));
	_dockRoot->setName("dock-root");

	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(s_splitterThickness);

	registerPanels();
	applyLayout();

	return true;
}

void AgentChatLayout::registerPanels() {
	ui::DockPanelDescriptor saved;
	saved.id = String("saved");
	saved.title = String("Saved");
	saved.icon = basic2d::IconName::Action_bookmark_solid;
	saved.minSize = Size2(s_savedMinWidth, 160.0f);
	saved.flags = s_panelFlags;
	saved.builder = [panel = _savedPanel]() -> Rc<Node> { return panel; };
	_dock->registerPanel(sp::move(saved));

	ui::DockPanelDescriptor chat;
	chat.id = String("chat");
	chat.title = String("Chat");
	chat.icon = basic2d::IconName::Action_question_answer_solid;
	chat.minSize = Size2(s_chatMinWidth, 200.0f);
	chat.flags = s_panelFlags;
	chat.builder = [panel = _chatPanel]() -> Rc<Node> { return panel; };
	_dock->registerPanel(sp::move(chat));
}

void AgentChatLayout::applyLayout() {
	using Spec = ui::DockLayoutSpec;

	_dock->setLayout(Spec::hsplit(s_savedShare,
			Spec::leaf({String("saved")},
					{
						.name = String("saved"),
						.minSize = Size2(s_savedMinWidth, 0.0f),
						.flags = s_frameFlags,
					}),
			Spec::leaf({String("chat")},
					{
						.name = String("chat"),
						.minSize = Size2(s_chatMinWidth, 0.0f),
						.flags = s_frameFlags,
					})));
}

void AgentChatLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	_commands.attach(scene);
	registerCommands();
}

void AgentChatLayout::handleExit() {
	_commands.detach();

	basic2d::SceneLayout2d::handleExit();
}

void AgentChatLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	for (auto node : {_background, _dockRoot}) {
		if (node) {
			node->setAnchorPoint(Anchor::BottomLeft);
			node->setPosition(Vec2::ZERO);
			node->setContentSize(getContentSize());
		}
	}
}

// ---- inspector ---------------------------------------------------------------------------

Value AgentChatLayout::encodeTree() const {
	Value nodes;

	_dock->getTree().eachInOrder([&](const ui::DockTreeNode &node) {
		Value entry;
		entry.setInteger(node.self.index, "index");
		entry.setString(getFrameKindName(node), "kind");
		entry.setValue(Value{Value(node.rect.origin.x), Value(node.rect.origin.y),
						   Value(node.rect.size.width), Value(node.rect.size.height)},
				"rect");
		entry.setValue(Value{Value(node.minSize.width), Value(node.minSize.height)}, "min");

		if (node.isSplit()) {
			entry.setString(node.axis == ui::DockAxis::Horizontal ? "h" : "v", "axis");
			entry.setDouble(node.ratio, "ratio");
			nodes.addValue(sp::move(entry));
			return;
		}

		entry.setString(node.params.name, "name");
		entry.setInteger(int64_t(toInt(node.params.flags)), "flags");

		Value panels;
		for (auto &it : node.panels) { panels.addString(it); }
		entry.setValue(sp::move(panels), "panels");

		nodes.addValue(sp::move(entry));
	});

	return nodes;
}

void AgentChatLayout::registerCommands() {
	_commands.add("dock.state", "The split tree: frames, their rects, minimums and shares",
			[this](const Value &, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(true, "ok");
		result.setDouble(_dock->getSplitterThickness(), "splitter");
		result.setValue(encodeTree(), "nodes");
		done(sp::move(result));
	});

	_commands.add("dock.ratio", "Move the divider: {\"value\": 0..1}",
			[this](const Value &args, Function<void(Value &&)> &&done) {
		auto root = _dock->getRootNode();
		auto ok = _dock->setSplitRatio(root, float(args.getDouble("value")));

		/* Answered a frame later on purpose: a mutation writes the tree at once, and the RECTS
		follow on the next layout pass. Answering immediately would report the previous ones. */
		runAction(Rc<Sequence>::create(0.1f,
				Function<void()>([this, ok, done = sp::move(done)]() mutable {
			Value result;
			result.setBool(ok, "ok");
			result.setValue(encodeTree(), "nodes");
			done(sp::move(result));
		})));
	});

	_commands.add("dock.check", "Assert that the two columns really are fixed",
			[this](const Value &, Function<void(Value &&)> &&done) {
		size_t checks = 0;
		size_t failures = 0;

		auto expect = [&](bool condition, StringView message) {
			++checks;
			if (!condition) {
				++failures;
				log::source().error("AgentChatExample", "dock check FAILED: ", message);
			}
		};

		auto saved = _dock->findFrameByName("saved");
		auto chat = _dock->findFrameByName("chat");

		expect(!saved.empty(), "the saved frame exists");
		expect(!chat.empty(), "the chat frame exists");

		for (auto handle : {saved, chat}) {
			if (handle.empty()) {
				continue;
			}

			auto &node = _dock->getTree().at(handle);
			expect(!hasFlag(node.params.flags, ui::DockFrameFlags::AllowSplit),
					"a fixed frame cannot be split");
			expect(!hasFlag(node.params.flags, ui::DockFrameFlags::AllowDrop),
					"a fixed frame takes no drops");
			expect(!hasFlag(node.params.flags, ui::DockFrameFlags::AllowClose),
					"a fixed frame cannot be closed");
			expect(hasFlag(node.params.flags, ui::DockFrameFlags::AllowResize),
					"the divider between fixed frames still moves");
		}

		expect(_dock->canResize(_dock->getRootNode()), "the root split is resizable");
		expect(_dock->getTree().getLeafCount() == 2, "there are exactly two columns");

		Value result;
		result.setBool(failures == 0, "ok");
		result.setInteger(int64_t(checks), "checks");
		result.setInteger(int64_t(failures), "failures");
		done(sp::move(result));
	});
}

} // namespace stappler::xenolith::examples
