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

#include "dock/DockPersistLayout.h"
#include "XLUiStyleResolver.h"
#include "XL2dLayer.h"
#include "XLAction.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float RootWidth = 800.0f;
static constexpr float RootHeight = 400.0f;

static constexpr auto s_css = StringView(R"css(
dock-frame { background-color: #232323; }
dock-tab-bar { background-color: #171717; }
dock-tab { background-color: #2b2b2b; }
dock-tab > label { color: #c8c8c8; font-size: 13px; }
)css");

} // namespace

bool DockPersistLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_root = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(1));
	_root->setAnchorPoint(Anchor::BottomLeft);
	_root->setContentSize(Size2(RootWidth, RootHeight));

	_dock = _root->addSystem(Rc<ui::DockSystem>::create());

	auto makePanel = [](StringView id, ui::DockPanelFlags flags = ui::DockPanelFlags::Default) {
		ui::DockPanelDescriptor desc;
		desc.id = id.str<Interface>();
		desc.title = id.str<Interface>();
		desc.minSize = Size2(80.0f, 40.0f);
		desc.flags = flags;
		desc.builder = [] { return Rc<basic2d::Layer>::create(Color::Teal_700); };
		return desc;
	};

	_dock->registerPanel(makePanel("alpha"));
	_dock->registerPanel(makePanel("beta"));
	_dock->registerPanel(makePanel("gamma"));
	// registered but NOT in any saved layout below: it must stay closed
	_dock->registerPanel(makePanel("orphan"));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(0.4f, Spec::leaf({String("alpha")}, {.name = String("left")}),
			Spec::leaf({String("beta"), String("gamma")}, {.name = String("right")})));

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); }));
	return true;
}

void DockPersistLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DockPersistTest", phase, ": ", what);
	}
}

void DockPersistLayout::runPhase1() {
	_saved = _dock->save();

	expect(_saved.getInteger("version") == ui::DockTree::SaveVersion, "phase1",
			"the save carries no version");
	expect(_saved.getValue("root").getString("type") == "split", "phase1",
			"the root of the save is not the split that was built");

	// nothing derived may be written: those come back from the registry and from the layout pass
	const auto &leftNode = _saved.getValue("root").getValue("first");
	expect(!leftNode.hasValue("rect") && !leftNode.hasValue("title") && !leftNode.hasValue("icon"),
			"phase1", "the save carries derived data");
	expect(leftNode.getString("name") == "left", "phase1", "a frame lost its name");

	// a CBOR round trip must survive, since that is how an application would store it
	auto bytes = data::write<Interface>(_saved, data::EncodeFormat::Cbor);
	auto reread = data::read<Interface>(bytes);
	expect(data::toString(reread, false) == data::toString(_saved, false), "phase1",
			"the layout did not survive a CBOR round trip");

	log::source().warn("DockPersistTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; rearranging the dock");

	// now move things about, exactly as a user would
	auto right = _dock->findFrameByName("right");
	_dock->splitFrame(right, ui::DockAxis::Vertical, false, {.name = String("bottom")});
	_dock->movePanel("gamma", _dock->findFrameByName("bottom"));
	_dock->setSplitRatio(_dock->getRootNode(), 0.7f);
}

void DockPersistLayout::runPhase2() {
	const auto rearranged = _dock->save();
	expect(data::toString(rearranged, false) != data::toString(_saved, false), "phase2",
			"the rearrangement did not change the saved layout at all");

	expect(_dock->restore(_saved), "phase2", "restore refused a layout it had just written");
}

void DockPersistLayout::runPhase3() {
	// the round trip: save -> rearrange -> restore must reproduce the ORIGINAL exactly
	const auto restored = _dock->save();
	expect(data::toString(restored, false) == data::toString(_saved, false), "phase3",
			toString("the restored layout differs:\n  was ", data::toString(_saved, false),
					"\n  now ", data::toString(restored, false)));

	// a registered panel the file never mentioned stays closed - that is the point of persisting
	expect(!_dock->isPanelOpen("orphan"), "phase3",
			"a panel absent from the save must not be opened");

	// a layout naming a panel this build no longer has: dropped, not fatal
	Value stale = _saved;
	stale.getValue("root").getValue("first").getValue("panels").addString("ghost");
	expect(_dock->restore(stale), "phase3", "an unknown panel must not fail the restore");
	expect(!_dock->findFrameForPanel("ghost"), "phase3", "an unknown panel was parked anyway");
	expect(_dock->isPanelOpen("alpha"), "phase3", "the known panels went missing with it");

	log::source().warn("DockPersistTest", "phase3 done: ", _checks, " checks, ", _failures,
			" failures; feeding it a broken file");
}

void DockPersistLayout::runPhase4() {
	const auto before = data::toString(_dock->save(), false);

	// A split with only one child, a node of an unknown type, and a version from the future: each
	// must be refused outright, leaving the live layout exactly as it was.
	//
	// Every sub-value is built standalone and then assigned. Reaching for `v.getValue("root")` on
	// a key that is not there hands back the shared read-only Null, and writing through THAT is
	// the bug, not the test.
	Value onlyChild;
	onlyChild.setString("frame", "type");

	Value halfSplitRoot;
	halfSplitRoot.setString("split", "type");
	halfSplitRoot.setValue(sp::move(onlyChild), "first");

	Value halfSplit;
	halfSplit.setInteger(ui::DockTree::SaveVersion, "version");
	halfSplit.setValue(sp::move(halfSplitRoot), "root");
	expect(!_dock->restore(halfSplit), "phase4", "a split with one child was accepted");

	Value unknownRoot;
	unknownRoot.setString("wardrobe", "type");

	Value unknownType;
	unknownType.setInteger(ui::DockTree::SaveVersion, "version");
	unknownType.setValue(sp::move(unknownRoot), "root");
	expect(!_dock->restore(unknownType), "phase4", "an unknown node type was accepted");

	Value future = _saved;
	future.setInteger(ui::DockTree::SaveVersion + 1, "version");
	expect(!_dock->restore(future), "phase4", "a layout from the future was accepted");

	Value empty;
	expect(!_dock->restore(empty), "phase4", "an empty value was accepted");

	expect(data::toString(_dock->save(), false) == before, "phase4",
			"a refused restore still changed the live layout");

	log::source().warn("DockPersistTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void DockPersistLayout::registerCommands() {
	TestLayout::registerCommands();

	addCommand("save", "The current layout as a value", [this](Value &&) { return _dock->save(); });

	addCommand("restore", "Restore a layout: the value itself",
			[this](Value &&args) { return Value(_dock->restore(args)); });

	addCommand("summary", "Checks and failures so far", [this](Value &&) {
		Value ret;
		ret.setInteger(_checks, "checks");
		ret.setInteger(_failures, "failures");
		return ret;
	});
}

void DockPersistLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_root->setPosition(Vec2(40.0f, getWorkTop() - 40.0f - RootHeight));
}

} // namespace stappler::xenolith::app
