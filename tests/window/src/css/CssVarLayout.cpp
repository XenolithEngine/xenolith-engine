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

#include "css/CssVarLayout.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr auto s_css = StringView(R"css(
:root {
	--brand: #3949ab;
	--pad: 24px;
	--accent-base: #00897b;
	--accent: var(--accent-base);
	/* a reference cycle: neither may ever resolve */
	--loop-a: var(--loop-b);
	--loop-b: var(--loop-a);
}

.box { background-color: #616161; width: 40px; }

.plain    { background-color: var(--brand); }
.sized    { width: var(--pad); }
.fallback { background-color: var(--nope, #e53935); }
.nested   { background-color: var(--accent); }
.cycle    { background-color: var(--loop-a); }

/* the variable is declared by a MORE specific rule than the one using it: CSS resolves a
   custom property on the element, so the use below must still see #fb8c00 */
.late            { background-color: var(--late-brand); }
.late-holder .late { --late-brand: #fb8c00; }

/* the substituted declaration must not win just by being expanded last */
.beaten          { background-color: var(--brand); }
.beaten-holder .beaten { background-color: #d81b60; }

/* the themed subtree overrides the inherited variable */
.dark { --brand: #fdd835; }
.themed { background-color: var(--brand); }
)css");

static const Color4B s_brand(0x39, 0x49, 0xab, 0xff);
static const Color4B s_fallback(0xe5, 0x39, 0x35, 0xff);
static const Color4B s_accent(0x00, 0x89, 0x7b, 0xff);
static const Color4B s_late(0xfb, 0x8c, 0x00, 0xff);
static const Color4B s_pink(0xd8, 0x1b, 0x60, 0xff);
static const Color4B s_themed(0xfd, 0xd8, 0x35, 0xff);
static const Color4B s_base(0x61, 0x61, 0x61, 0xff);

} // namespace

void CssVarLayout::expectColor(StringView what, Node *node, const Color4B &c) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	auto got = st.background().backgroundColor;
	if (!st.valid() || got.r != c.r || got.g != c.g || got.b != c.b) {
		++_failures;
		log::source().error("CssVarTest", what, ": expected rgb(", int(c.r), ",", int(c.g), ",",
				int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")",
				st.valid() ? "" : " (invalid)");
	}
}

void CssVarLayout::expectAppliedColor(StringView what, Layer *node, const Color4B &c) {
	++_checks;
	auto got = Color4B(node->getColor());
	if (got.r != c.r || got.g != c.g || got.b != c.b) {
		++_failures;
		log::source().error("CssVarTest", what, ": expected rgb(", int(c.r), ",", int(c.g), ",",
				int(c.b), ") got rgb(", int(got.r), ",", int(got.g), ",", int(got.b), ")");
	}
}

void CssVarLayout::expectMetric(StringView what, Node *node, document::ParameterName name,
		float expected) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	document::StyleValue v;
	if (!st.getValue(name, v) || sprt::abs(v.sizeValue.value - expected) > 0.01f) {
		++_failures;
		log::source().error("CssVarTest", what, ": expected ", expected, " got ",
				st.has(name) ? v.sizeValue.value : -1.0f);
	}
}

void CssVarLayout::expectVar(StringView what, Node *node, StringView name, StringView expected) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	auto got = st.getCustomProperty(name);
	if (got != expected) {
		++_failures;
		log::source().error("CssVarTest", what, ": expected '", expected, "' got '", got, "'");
	}
}

void CssVarLayout::expectNoValue(StringView what, Node *node, document::ParameterName name) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	if (st.has(name)) {
		++_failures;
		log::source().error("CssVarTest", what, ": expected no value, but one was resolved");
	}
}

bool CssVarLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeBox = [](Node *parent, StringView cls) {
		auto box = parent->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
		box->addStyleClass("box");
		box->addStyleClass(cls);
		return box;
	};

	_plain = makeBox(this, "plain");
	_sized = makeBox(this, "sized");
	_fallback = makeBox(this, "fallback");
	_nested = makeBox(this, "nested");
	_cycle = makeBox(this, "cycle");

	{
		auto holder = addChild(Rc<Node>::create(), ZOrder(1));
		holder->addStyleClass("late-holder");
		_late = makeBox(holder, "late");
	}
	{
		auto holder = addChild(Rc<Node>::create(), ZOrder(1));
		holder->addStyleClass("beaten-holder");
		_beaten = makeBox(holder, "beaten");
	}

	// the themed subtree: `.dark` is added at runtime, so the box below must repaint even
	// though nothing about it moved, resized or changed class
	_theme = addChild(Rc<Node>::create(), ZOrder(1));
	_themed = makeBox(_theme, "themed");

	runStatic();

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runThemeSwitch(); },
			Rc<DelayTime>::create(0.4f), [this] { report(); }));

	return true;
}

void CssVarLayout::runStatic() {
	using document::ParameterName;

	// the plain case: a colour taken from :root
	expectColor("plain, colour from :root", _plain, s_brand);
	expectVar("plain, raw variable text", _plain, "--brand", "#3949ab");

	// a length: the variable carries the unit, and the box is 24px, not the 40px of `.box`
	expectMetric("sized, length from a variable", _sized, ParameterName::CssWidth, 24.0f);
	expectMetric("unsized, plain literal width", _plain, ParameterName::CssWidth, 40.0f);

	// an undeclared variable falls back
	expectColor("fallback, undeclared name", _fallback, s_fallback);

	// a variable defined in terms of another
	expectColor("nested, variable of a variable", _nested, s_accent);
	expectVar("nested, raw text is NOT pre-expanded", _nested, "--accent", "var(--accent-base)");

	// a reference cycle makes the declaration invalid: it must be dropped, not hang, and the
	// `.box` colour must stand
	expectColor("cycle, declaration dropped", _cycle, s_base);

	// a variable declared by a MORE specific rule is still visible to a less specific use
	expectColor("late, variable from a more specific rule", _late, s_late);

	// ...but the substituted declaration itself does NOT win by being expanded last
	expectColor("beaten, literal from a more specific rule wins", _beaten, s_pink);

	// inheritance: before the switch the themed box sees :root's value
	expectColor("themed, inherited before switch", _themed, s_brand);
	expectVar("themed, variable is inherited", _themed, "--brand", "#3949ab");

	// a node outside any var() use still has no background of its own
	expectNoValue("holder, untouched by var()", _theme, ParameterName::CssBackgroundColor);
}

void CssVarLayout::runThemeSwitch() {
	// the applied colour before the switch
	expectAppliedColor("themed, applied before switch", _themed, s_brand);

	// flip the variable on the ANCESTOR: the box itself does not change class, size or
	// position, so only the custom-property invalidation can repaint it
	_theme->addStyleClass("dark");
}

void CssVarLayout::report() {
	expectVar("themed, variable after switch", _themed, "--brand", "#fdd835");
	expectAppliedColor("themed, applied after switch", _themed, s_themed);

	log::source().warn("CssVarTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void CssVarLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float rowH = 44.0f;
	const float top = getWorkTop() - 24.0f;

	Layer *boxes[] = {_plain, _sized, _fallback, _nested, _cycle, _late, _beaten, _themed};
	for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
		boxes[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		// the box is a child of a holder node for some cases, so position in the layout's space
		boxes[i]->setPosition(boxes[i]->getParent()->convertToNodeSpace(
				convertToWorldSpace(Vec2(40.0f, top - float(i) * rowH))));
		boxes[i]->setContentSize(Size2(boxes[i]->getContentSize().width, 30.0f));
	}
}

} // namespace stappler::xenolith::app
