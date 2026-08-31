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

#include "widgets/PanelLayout.h"
#include "XLUiStyleResolver.h"
#include "XLInheritedStyle.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// Every fill below is distinct, so a value that arrives from the wrong rule is still a failure.
static constexpr auto s_css = StringView(R"css(
panel {
	width: 120px;
	height: 90px;
	background-color: #1e88e5;
	border-radius: 16px;
}
checkbox {
	width: 40px;
	height: 40px;
	background-color: #43a047;
	border-radius: 8px;
}
checkbox:checked {
	background-color: #e53935;
}
badge {
	width: 120px;
	height: 40px;
	background-color: #8e24aa;
	border-radius: 12px;
	display: flex;
	align-items: center;
	justify-content: center;
}
badge > label {
	color: #ffe082;
	font-size: 16px;
}
.plain {
	width: 120px;
	height: 90px;
	background-color: #ff8f00;
}
/* the ONLY rule that styles this button: dropping the class leaves it with no styled
   attribute at all, which is what CmdReset has to notice */
button.tinted {
	width: 120px;
	height: 40px;
	background-color: #00897b;
	outline-width: 3px;
	outline-color: #004d40;
}
)css");

static constexpr auto s_panelFill = Color4B(0x1e, 0x88, 0xe5, 255);
static constexpr auto s_checkboxFill = Color4B(0x43, 0xa0, 0x47, 255);
static constexpr auto s_checkedFill = Color4B(0xe5, 0x39, 0x35, 255);
static constexpr auto s_badgeFill = Color4B(0x8e, 0x24, 0xaa, 255);
static constexpr auto s_plainFill = Color4B(0xff, 0x8f, 0x00, 255);
static constexpr auto s_tintedFill = Color4B(0x00, 0x89, 0x7b, 255);

// Nothing in the sheet is anywhere near it, so a button wearing this colour can only have got it
// from the setPathColor call in init() - never from a rule that half-matched.
static constexpr auto s_codeFill = Color4B(0xff, 0x00, 0xff, 255);
static constexpr float s_codeRadius = 6.0f;

} // namespace

bool PanelLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);

	// one recursive resolver covers the atoms and the badge's label alike
	addSystem(Rc<ui::StyleResolver>::create(true));

	_panel = addChild(Rc<ui::Panel>::create(), ZOrder(1));
	_checkbox = addChild(Rc<ui::Checkbox>::create(), ZOrder(1));

	_badge = addChild(Rc<ui::Badge>::create(), ZOrder(1));
	_badge->setText("Installed");

	_plainLayer = addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder(1));
	_plainLayer->addStyleClass("plain");

	_resettable = addChild(Rc<ui::Button>::create(), ZOrder(1));
	_resettable->addStyleClass("tinted");

	// Painted BEFORE the class goes on, which is the order every widget that paints itself uses:
	// the constructor knows its own default, the stylesheet arrives later.
	_painted = addChild(Rc<ui::Button>::create(), ZOrder(1));
	_painted->setPathColor(s_codeFill, true);
	_painted->setBorderRadius(s_codeRadius);
	_painted->addStyleClass("tinted");

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.0f), [this] { runPhase1(); },
			Rc<DelayTime>::create(1.0f), [this] { runPhase2(); }, Rc<DelayTime>::create(1.0f),
			[this] { runPhase3(); }));

	return true;
}

void PanelLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("PanelTest", phase, ": ", what);
	}
}

void PanelLayout::expectColor(StringView phase, StringView what, const Color4B &actual,
		const Color4B &expected) {
	++_checks;
	if (actual != expected) {
		++_failures;
		log::source().error("PanelTest", phase, ": ", what, " is ", actual, ", expected ",
				expected);
	}
}

void PanelLayout::runPhase1() {
	// the control first: if this one is wrong the stylesheet never reached the subtree and the
	// per-type results below say nothing about the appliers
	expectColor("initial", "plain Layer fill", Color4B(_plainLayer->getColor()), s_plainFill);

	expectColor("initial", "panel fill", _panel->getPathColor(), s_panelFill);
	expectColor("initial", "checkbox fill", _checkbox->getPathColor(), s_checkboxFill);
	expectColor("initial", "badge fill", _badge->getPathColor(), s_badgeFill);

	// border-radius rides the same applier; a rounded box shrinks its own painted corners, which
	// the layer reports back through the radius it actually realized
	expect(_panel->getBorderRadius() > 0.0f, "initial", "panel border-radius was not applied");
	expect(_checkbox->getBorderRadius() > 0.0f, "initial",
			"checkbox border-radius was not applied");

	// A type applier consumes background-color for its own node; the child Label must still be
	// styled by the generic mapping of the same recursive resolver. `color` is an INHERITED
	// property, so it lands in a component rather than on the node's colour (see XLInheritedStyle.h)
	// - the label's own setColor stays where Badge::init left it either way.
	if (auto label = dynamic_cast<basic2d::Label *>(_badge->getChildren().front().get())) {
		auto style = accumulateInheritedStyle<InheritedColorStyle>(label);
		if ((style.defined & InheritedColorStyle::DefinedColor) != 0) {
			expectColor("initial", "badge label colour", Color4B(style.color, 255),
					Color4B(0xff, 0xe0, 0x82, 255));
		} else {
			expect(false, "initial", "badge label did not receive the CSS colour");
		}
	} else {
		expect(false, "initial", "badge has no label child");
	}

	// The stylesheet is the upper layer: while `.tinted` matches, its fill covers the one the
	// button painted on itself. (A code paint that beat CSS would be the opposite bug, and just as
	// wrong - it would make a stylesheet unable to restyle anything a widget draws by default.)
	expectColor("initial", "code-painted button under a matching rule", _painted->getPathColor(),
			s_tintedFill);

	// The rule says nothing about corners, so this one is the widget's own the whole way through.
	expect(_painted->getBorderRadius() == s_codeRadius, "initial",
			"code-painted button lost its border-radius to a rule that never mentioned one");

	log::source().warn("PanelTest", "initial done: ", _checks, " checks, ", _failures,
			" failures; checking the checkbox");

	// flips the "checked" class -> the resolver must restyle the node from the more specific rule
	_checkbox->setChecked(true);
}

void PanelLayout::runPhase2() {
	expectColor("checked", "checkbox fill", _checkbox->getPathColor(), s_checkedFill);
	expect(_checkbox->isChecked(), "checked", "checkbox did not take the checked state");

	// CmdReset: `button.tinted` is the only rule that styles this button, so dropping the class
	// leaves NOT A SINGLE styled attribute in the resolved style. A style pass driven by the
	// present parameters alone could never undo the old paint - only the reset command can.
	expect(_resettable->getComponent<ui::PanelStyleComponent>() != nullptr, "checked",
			"button has no style component while `.tinted` is applied");

	_resettable->removeStyleClass("tinted");
	_painted->removeStyleClass("tinted");
}

void PanelLayout::runPhase3() {
	expect(_resettable->getComponent<ui::PanelStyleComponent>() == nullptr, "reset",
			"CmdReset did not drop the button's style component when its rule stopped matching");

	// The same pass, the same rule, the same reset - and the opposite outcome, because this button
	// painted itself. White here is the failure the whole distinction exists to prevent.
	expectColor("reset", "code-painted button after its rule stopped matching",
			_painted->getPathColor(), s_codeFill);
	expect(_painted->getBorderRadius() == s_codeRadius, "reset",
			"CmdReset took the code-painted button's border-radius with the rule");

	// ... and it keeps a component, because it has paint to keep. The two assertions above and this
	// one are one statement: the reset separates styling from paint rather than clearing the node.
	expect(_painted->getComponent<ui::PanelStyleComponent>() != nullptr, "reset",
			"a code-painted widget was left with no style component to hold its own paint");

	log::source().warn("PanelTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void PanelLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;

	Node *nodes[] = {_panel, _checkbox, _badge, _plainLayer, _resettable, _painted};
	float x = 48.0f;
	for (auto n : nodes) {
		if (n) {
			n->setAnchorPoint(Vec2(0.0f, 1.0f));
			n->setPosition(Vec2(x, top));
			x += 160.0f;
		}
	}
}

} // namespace stappler::xenolith::app
