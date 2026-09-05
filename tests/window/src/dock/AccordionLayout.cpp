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

#include "dock/AccordionLayout.h"
#include "XLUiStyleResolver.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float DockWidth = 420.0f;
static constexpr float SideWidth = 260.0f;
static constexpr float RootHeight = 420.0f;

static constexpr auto s_accordionCss = StringView(R"css(
dock-frame          { background-color: #232323; }
dock-frame-body     { display: flex; padding: 4px; }
dock-tab-bar        { background-color: #171717; }
dock-tab            { display: flex; padding: 4px 10px; background-color: #2b2b2b; }
dock-tab.active     { background-color: #383838; }
dock-tab > label    { color: #c8c8c8; font-size: 13px; }
dock-splitter       { background-color: #2a2a2a; }
dock-drop-indicator { background-color: #3d7ecf; }
dock-drag-ghost     { background-color: #2c2c2c; }

accordion-view            { background-color: #1e1e1e; }
accordion-section         { background-color: #232323; }
accordion-body            { display: flex; padding: 4px; background-color: #1e1e1e; }
accordion-header          { display: flex; align-items: center; column-gap: 6px;
                            padding: 4px 8px; background-color: #2b2b2b; }
accordion-header:hover    { background-color: #333333; }
accordion-header.expanded { background-color: #383838; }
accordion-header > label  { color: #c8c8c8; font-size: 13px; }
.accordion-chevron        { width: 16px; height: 16px; }
.accordion-grip           { width: 16px; height: 16px; }
accordion-close           { width: 16px; height: 16px; }
accordion-drop-indicator  { background-color: #3d7ecf; }
)css");

} // namespace

bool AccordionLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_accordionCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_root = addChild(Rc<Node>::create(), ZOrder(1));
	_root->setAnchorPoint(Anchor::BottomLeft);
	_root->setContentSize(Size2(DockWidth + SideWidth, RootHeight));

	// ONE registry, two containers. Everything this stand asserts follows from that single line.
	_registry = Rc<ui::PanelRegistry>::create();

	auto makePanel = [this](StringView id, Size2 minSize) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = id.str<Interface>();
		desc.minSize = minSize;
		// Counts every build and remembers the node, so a rebuild shows up twice over: as a bumped
		// counter and as a different node than the one recorded here.
		desc.builder = [this, key = id.str<Interface>()]() -> Rc<Node> {
			_builds.insert_or_assign(key, buildCount(key) + 1);
			auto node = Rc<basic2d::Layer>::create(Color::Teal_700);
			_built.insert_or_assign(key, node);
			return node;
		};
		_registry->registerPanel(sp::move(desc));
	};

	makePanel("alpha", Size2(120.0f, 50.0f));
	makePanel("beta", Size2(120.0f, 50.0f));
	makePanel("gamma", Size2(120.0f, 50.0f));
	makePanel("delta", Size2(120.0f, 50.0f));

	// The dock owns no registry of its own: it takes the shared one.
	_dockRoot = _root->addChild(Rc<Node>::create(), ZOrder(1));
	_dockRoot->setAnchorPoint(Anchor::BottomLeft);
	_dockRoot->setPosition(Vec2::ZERO);
	_dockRoot->setContentSize(Size2(DockWidth, RootHeight));
	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create(Rc<ui::PanelRegistry>(_registry)));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::leaf({String("alpha"), String("beta")}, {.name = String("main")}));
	_main = _dock->findFrameByName("main");

	_accordion = _root->addChild(Rc<ui::AccordionView>::create(Rc<ui::PanelRegistry>(_registry)),
			ZOrder(1));
	_accordion->setAnchorPoint(Anchor::BottomLeft);
	_accordion->setPosition(Vec2(DockWidth, 0.0f));
	_accordion->setContentSize(Size2(SideWidth, RootHeight));
	_accordion->setSections({String("gamma"), String("delta")});

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase5(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase6(); }));
	return true;
}

void AccordionLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - RootHeight));
}

void AccordionLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("AccordionTest", phase, ": ", what);
	}
}

size_t AccordionLayout::buildCount(StringView id) const {
	auto it = _builds.find(id.str<Interface>());
	return (it != _builds.end()) ? it->second : 0;
}

// P1. What is built, and what the registry says holds it.
void AccordionLayout::runPhase1() {
	// Only what is actually showing has been built. `beta` sits behind `alpha` in the dock frame and
	// must not exist; both accordion sections start expanded in Multi mode, so both must.
	expect(buildCount("alpha") == 1, "phase1", "the dock's visible panel was not built once");
	expect(buildCount("beta") == 0, "phase1", "a panel hidden behind a tab must not be built");
	expect(buildCount("gamma") == 1, "phase1", "an expanded section's panel was not built");
	expect(buildCount("delta") == 1, "phase1", "an expanded section's panel was not built");

	expect(_registry->getHost("alpha") == _dock, "phase1", "the dock does not hold 'alpha'");
	expect(_registry->getHost("gamma") == _accordion, "phase1",
			"the accordion does not hold 'gamma'");

	expect(_accordion->getSections().size() == 2, "phase1", "the accordion has the wrong sections");
	expect(_accordion->getSectionIndex("gamma") == 0, "phase1", "'gamma' is not the first section");
	expect(_accordion->isPanelExpanded("gamma"), "phase1", "a Multi section did not start open");
}

// P2. A panel is parked in exactly one place: opening it here takes it off there.
void AccordionLayout::runPhase2() {
	auto before = _built.find(String("alpha"));
	Node *node = (before != _built.end()) ? before->second.get() : nullptr;

	expect(_accordion->openPanel("alpha", 0), "phase2", "the accordion refused to take 'alpha'");

	expect(!_dock->isPanelOpen("alpha"), "phase2",
			"the dock still holds a panel the accordion took");
	expect(_accordion->isPanelOpen("alpha"), "phase2", "the accordion did not take 'alpha'");
	expect(_registry->getHost("alpha") == _accordion, "phase2",
			"the registry still names the dock as the host");

	// The whole point: the SAME node, not a rebuilt one.
	expect(buildCount("alpha") == 1, "phase2", "moving a panel rebuilt it");
	auto after = _built.find(String("alpha"));
	expect(after != _built.end() && after->second.get() == node, "phase2",
			"the panel's node was replaced by the move");

	// The dock's frame did not empty - `beta` is still in it - so it must now be showing that one,
	// which is the first time `beta` has had to exist.
	expect(_dock->isPanelOpen("beta"), "phase2", "the dock lost the panel it kept");
	expect(buildCount("beta") == 1, "phase2", "the newly shown panel was not built");
}

// P3. The zone rules of both sides, with no drag in flight.
void AccordionLayout::runPhase3() {
	// The accordion resolves an insertion index and nothing else - a section holds one panel, so
	// there is no "into" and nothing to subdivide.
	const auto size = _accordion->getContentSize();
	expect(_accordion->getDropIndexAt(Vec2(size.width / 2.0f, size.height - 1.0f)) == 0, "phase3",
			"a point at the very top did not resolve to the first slot");
	expect(_accordion->getDropIndexAt(Vec2(size.width / 2.0f, 1.0f))
					== _accordion->getSections().size(),
			"phase3", "a point at the very bottom did not resolve to the last slot");

	// The dock must offer real zones for a panel it does NOT hold. The lone-occupant suppression
	// keys off "this frame holds the dragged panel", and a foreign id must never match it.
	const auto rect = _dock->getTree().get(_main)->rect;
	auto t = _dock->hitTest(Vec2(rect.getMidX(), rect.getMidY()), StringView("alpha"));
	expect(t.kind != ui::DockDropTarget::Kind::None, "phase3",
			"the dock offered no zone for a panel held elsewhere");
}

// P4. Collapsing gives the panel back; the sizing policies both hold the floor.
void AccordionLayout::runPhase4() {
	expect(_accordion->collapsePanel("delta"), "phase4", "a section refused to collapse");
	expect(!_accordion->isPanelExpanded("delta"), "phase4", "the section is still expanded");

	// The node is not destroyed by a collapse - it goes back to the registry intact, so re-opening
	// must not rebuild it.
	expect(_accordion->expandPanel("delta"), "phase4", "a section refused to re-open");
	expect(buildCount("delta") == 1, "phase4", "re-opening a section rebuilt its panel");

	// Single mode reduces an arrangement to exactly one open section - never to none, which is the
	// state the mode exists to rule out.
	_accordion->setExpansion(ui::AccordionExpansion::Single);
	size_t open = 0;
	for (auto &id : _accordion->getSections()) {
		if (_accordion->isPanelExpanded(id)) {
			++open;
		}
	}
	expect(open == 1, "phase4", "Single mode did not leave exactly one section open");

	_accordion->setExpansion(ui::AccordionExpansion::Multi);
	_accordion->setSizing(ui::AccordionSizing::Fill);
	_accordion->setSizing(ui::AccordionSizing::Fit);

	// A floor made of every header plus the minimum of every OPEN panel; it can never be nothing
	// while there are sections.
	expect(_accordion->getNaturalMinSize().height > 0.0f, "phase4",
			"the natural minimum of a populated stack is zero");
}

// P5. A save/restore round trip over the pair.
void AccordionLayout::runPhase5() {
	const auto sections = Vector<String>(_accordion->getSections().begin(),
			_accordion->getSections().end());
	auto saved = _accordion->save();

	// Rearrange, then put it back. The order is what has to come back; the titles and minimums come
	// from the registry and were never in the file.
	expect(_accordion->movePanel("alpha", 2), "phase5", "a reorder was refused");
	expect(_accordion->getSectionIndex("alpha") != 0, "phase5", "the reorder did nothing");

	expect(_accordion->restore(saved), "phase5", "restore rejected its own save");
	const auto restored = Vector<String>(_accordion->getSections().begin(),
			_accordion->getSections().end());
	expect(restored == sections, "phase5", "the restored order is not the saved one");

	// Nothing was rebuilt by any of it.
	expect(buildCount("alpha") == 1, "phase5", "a save/restore round trip rebuilt a panel");

	// A file of the wrong version must leave what is on screen untouched.
	Value bogus;
	bogus.setInteger(999, "version");
	expect(!_accordion->restore(bogus), "phase5", "restore accepted an unknown version");
	expect(Vector<String>(_accordion->getSections().begin(), _accordion->getSections().end())
					== sections,
			"phase5", "a rejected restore changed the arrangement");
}

// P6. Teardown: removing the dock must not touch the accordion's panels.
void AccordionLayout::runPhase6() {
	// Give the dock something to hold, so the teardown has a subtree of its own to sweep.
	expect(_dock->openPanel("beta", _main), "phase6", "the dock refused to take 'beta' back");

	auto held = Vector<String>(_accordion->getSections().begin(), _accordion->getSections().end());

	_dockRoot->removeSystem(_dock);
	_dock = nullptr;

	// The accordion's panels are still parented under it. Before the detach was scoped to the dock's
	// own subtree, this is exactly what came apart - and silently: the section would still be there,
	// just empty, with nothing on screen to connect it to the dock going away.
	for (auto &id : held) {
		if (!_accordion->isPanelExpanded(id)) {
			continue;
		}
		auto section = _accordion->getSection(id);
		auto it = _built.find(id);
		expect(section && it != _built.end() && it->second->getParent() == section->getBody(),
				"phase6", "removing the dock detached a panel the accordion was holding");
	}
	expect(_registry->getHost("beta") == nullptr, "phase6",
			"the removed dock still claims a panel");

	// Stand a FRESH dock up over the same registry. Two things at once: a second host on a registry
	// that has already been used must work (nothing in it is bound to the first one), and the stand
	// ends with both containers live - which is what lets a driver script exercise the real input
	// path, dragging a panel from one into the other by its grip.
	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create(Rc<ui::PanelRegistry>(_registry)));
	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::leaf({String("beta")}, {.name = String("main")}));
	_main = _dock->findFrameByName("main");

	expect(_dock->isPanelOpen("beta"), "phase6", "a fresh dock did not take a panel back");
	expect(_registry->getHost("beta") == _dock, "phase6", "the registry does not name the new dock");
	expect(buildCount("beta") == 1, "phase6", "a second dock rebuilt a panel it was handed");

	log::source().warn("AccordionTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void AccordionLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("sections", "The stack: id, expanded, index and rect per section", [this](Value &&) {
		Value ret;
		size_t i = 0;
		for (auto &id : _accordion->getSections()) {
			Value entry;
			entry.setString(id, "id");
			entry.setInteger(i, "index");
			entry.setBool(_accordion->isPanelExpanded(id), "expanded");
			if (auto section = _accordion->getSection(id)) {
				entry.setValue(Value{Value(section->getPosition().x),
								   Value(section->getPosition().y),
								   Value(section->getContentSize().width),
								   Value(section->getContentSize().height)},
						"rect");
			}
			ret.addValue(sp::move(entry));
			++i;
		}
		return ret;
	});

	addCommand("drop-index", "Insertion index for a point in the view: {x, y}", [this](Value &&args) {
		const Value &req = args;
		Value ret;
		ret.setInteger(
				_accordion->getDropIndexAt(Vec2(float(req.getDouble("x")), float(req.getDouble("y")))),
				"index");
		return ret;
	});

	addCommand("builds", "How many times each panel's builder ran", [this](Value &&) {
		Value ret;
		for (auto &it : _builds) { ret.setInteger(it.second, it.first); }
		return ret;
	});

	addCommand("host", "Which container holds a panel: {panel}", [this](Value &&args) {
		auto panel = static_cast<const Value &>(args).getString("panel");
		auto host = _registry->getHost(panel);
		Value ret;
		if (!host) {
			ret.setString("none", "host");
		} else if (host == static_cast<ui::PanelHost *>(_accordion)) {
			ret.setString("accordion", "host");
		} else {
			ret.setString("dock", "host");
		}
		return ret;
	});

	addCommand("probe", "World-space centre of a section's GRIP and of its header body: {panel}",
			[this](Value &&args) {
		Value ret;
		auto panel = static_cast<const Value &>(args).getString("panel");
		auto section = _accordion->getSection(panel);
		if (!section || !section->getHeader()) {
			return ret;
		}
		auto header = section->getHeader();

		// The grip is the ONLY place a drag may start. A press on the header body must toggle the
		// section instead - which is why both points are reported: one drives the positive case and
		// the other the negative one.
		if (auto grip = header->getGrip()) {
			const auto centre = grip->convertToWorldSpace(Vec2(grip->getContentSize().width / 2.0f,
					grip->getContentSize().height / 2.0f));
			ret.setDouble(centre.x, "gripX");
			ret.setDouble(centre.y, "gripY");
		}
		const auto body = header->convertToWorldSpace(
				Vec2(header->getContentSize().width * 0.75f, header->getContentSize().height / 2.0f));
		ret.setDouble(body.x, "headerX");
		ret.setDouble(body.y, "headerY");
		return ret;
	});

	addCommand("summary", "Checks and failures so far", [this](Value &&) {
		Value ret;
		ret.setInteger(_checks, "checks");
		ret.setInteger(_failures, "failures");
		return ret;
	});
}

} // namespace stappler::xenolith::app
