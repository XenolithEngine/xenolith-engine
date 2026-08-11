/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "css/CalcLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Layer;

namespace {

static constexpr auto s_css = StringView(R"css(
:root {
	--pad: 4px;
	--step: 10px;
}

/* the fallback every dropped declaration must reveal */
.box { background-color: #616161; width: 40px; }

.sum       { width: calc(8px + 16px); }
.diff      { width: calc(100px - 40px); }
.mul-right { width: calc(16px * 2); }
.mul-left  { width: calc(2 * 16px); }
.div       { width: calc(64px / 4); }
.parens    { width: calc((2 + 1) * 10px); }
.nested    { width: calc(((4 + 1)) * 8px); }
.percent   { width: calc(50% + 10%); }
.with-var  { width: calc(var(--pad) * 3); }

/* every one of these must be DROPPED, leaving `.box`'s 40px */
.mixed-units      { width: calc(100% - 20px); }
.unit-squared     { width: calc(2px * 3px); }
.div-by-zero      { width: calc(10px / 0); }
.unit-plus-number { width: calc(10px + 5); }
.unbalanced       { width: calc(10px + 5; }

/* node-local custom properties: --k is declared in C++, on the node */
.local       { width: calc(var(--k) * var(--step)); }
.local-child { width: calc(var(--k) * 2px); }
.normalized  { width: calc(var(--k) * 1px); }
.removed     { width: calc(var(--k, 7) * 1px); }

/* a sheet rule declaring the same name on the same node: the node-local one must win */
.local-wins { --k: 1; width: calc(var(--k) * 100px); }
)css");

} // namespace

void CalcLayout::expectMetric(StringView what, Node *node, document::ParameterName name,
		float expected) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	document::StyleValue v;
	if (!st.getValue(name, v) || sprt::abs(v.sizeValue.value - expected) > 0.01f) {
		++_failures;
		log::source().error("CalcTest", what, ": expected ", expected, " got ",
				st.has(name) ? v.sizeValue.value : -1.0f);
	}
}

void CalcLayout::expectNoValue(StringView what, Node *node, document::ParameterName name) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	if (st.has(name)) {
		++_failures;
		log::source().error("CalcTest", what, ": expected no value, but one was resolved");
	}
}

void CalcLayout::expectVar(StringView what, Node *node, StringView name, StringView expected) {
	++_checks;
	auto st = ui::StyleResolver::resolveStyleForNode(node);
	auto got = st.getCustomProperty(name);
	if (got != expected) {
		++_failures;
		log::source().error("CalcTest", what, ": expected '", expected, "' got '", got, "'");
	}
}

void CalcLayout::expectAppliedWidth(StringView what, Node *node, float expected) {
	++_checks;
	auto got = node->getContentSize().width;
	if (sprt::abs(got - expected) > 0.01f) {
		++_failures;
		log::source().error("CalcTest", what, ": expected width ", expected, " got ", got);
	}
}

bool CalcLayout::init() {
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

	_sum = makeBox(this, "sum");
	_diff = makeBox(this, "diff");
	_mulRight = makeBox(this, "mul-right");
	_mulLeft = makeBox(this, "mul-left");
	_div = makeBox(this, "div");
	_parens = makeBox(this, "parens");
	_nested = makeBox(this, "nested");
	_percent = makeBox(this, "percent");
	_withVar = makeBox(this, "with-var");

	_mixedUnits = makeBox(this, "mixed-units");
	_unitSquared = makeBox(this, "unit-squared");
	_divByZero = makeBox(this, "div-by-zero");
	_unitPlusNumber = makeBox(this, "unit-plus-number");
	_unbalanced = makeBox(this, "unbalanced");

	// --k lives on the node, not in the sheet. The child gets it by inheritance alone.
	_local = makeBox(this, "local");
	ui::setStyleVariable(_local, "--k", "3");
	_localChild = makeBox(_local, "local-child");

	// the same name declared by a rule that matches this very node: node-local must still win
	_localWins = makeBox(this, "local-wins");
	ui::setStyleVariable(_localWins, "--k", "2");

	// written without the leading dashes and in upper case
	_normalized = makeBox(this, "normalized");
	ui::setStyleVariable(_normalized, "K", "25");

	_removed = makeBox(this, "removed");
	ui::setStyleVariable(_removed, "--k", "30");

	runArithmetic();
	runInvalid();
	runNodeVariables();

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runRuntimeChange(); },
			Rc<DelayTime>::create(0.6f), [this] { report(); }));

	return true;
}

void CalcLayout::runArithmetic() {
	using document::ParameterName;

	expectMetric("sum", _sum, ParameterName::CssWidth, 24.0f);
	expectMetric("difference", _diff, ParameterName::CssWidth, 60.0f);
	expectMetric("dimension * number", _mulRight, ParameterName::CssWidth, 32.0f);
	expectMetric("number * dimension", _mulLeft, ParameterName::CssWidth, 32.0f);
	expectMetric("dimension / number", _div, ParameterName::CssWidth, 16.0f);
	expectMetric("parenthesised sub-expression", _parens, ParameterName::CssWidth, 30.0f);
	expectMetric("nested parentheses", _nested, ParameterName::CssWidth, 40.0f);

	// percent is stored as a fraction, so 50% + 10% is 0.6
	expectMetric("percent + percent", _percent, ParameterName::CssWidth, 0.6f);

	// var() is substituted before the value is parsed, so calc() sees plain text by then
	expectMetric("calc over a variable", _withVar, ParameterName::CssWidth, 12.0f);
}

void CalcLayout::runInvalid() {
	using document::ParameterName;

	// each of these must lose its own declaration and fall back to `.box { width: 40px }` -
	// never to a half-folded value
	expectMetric("mixed units are not representable", _mixedUnits, ParameterName::CssWidth, 40.0f);
	expectMetric("dimension * dimension", _unitSquared, ParameterName::CssWidth, 40.0f);
	expectMetric("division by zero", _divByZero, ParameterName::CssWidth, 40.0f);
	expectMetric("dimension + number", _unitPlusNumber, ParameterName::CssWidth, 40.0f);
	expectMetric("unbalanced parentheses", _unbalanced, ParameterName::CssWidth, 40.0f);
}

void CalcLayout::runNodeVariables() {
	using document::ParameterName;

	// the node's own declaration, multiplied by one from the sheet
	expectVar("node-local variable is visible", _local, "--k", "3");
	expectMetric("node-local variable in calc", _local, ParameterName::CssWidth, 30.0f);

	// ...and inherited by the subtree, exactly like a sheet-declared one
	expectVar("node-local variable is inherited", _localChild, "--k", "3");
	expectMetric("inherited node-local variable", _localChild, ParameterName::CssWidth, 6.0f);

	// a rule matching this node declares --k: 1; the node-local declaration is more specific
	expectVar("node-local beats a matching rule", _localWins, "--k", "2");
	expectMetric("node-local beats a matching rule (used)", _localWins, ParameterName::CssWidth,
			200.0f);

	// names are normalised: "K" is the same property as "--k"
	expectVar("name normalised", _normalized, "--k", "25");
	expectMetric("name normalised (used)", _normalized, ParameterName::CssWidth, 25.0f);

	// a node that declares nothing sees nothing of its siblings' properties
	expectVar("not leaked to a sibling", _sum, "--k", StringView());
	expectNoValue("sibling keeps no width of its own beyond .box", _localWins,
			ParameterName::CssColor);
}

void CalcLayout::runRuntimeChange() {
	using document::ParameterName;

	// what the layout actually committed from the initial value
	expectAppliedWidth("applied before change", _local, 30.0f);
	expectAppliedWidth("applied to the inheriting child", _localChild, 6.0f);
	expectAppliedWidth("applied before removal", _removed, 30.0f);

	// Change the variable on the node. Nothing moves, no class flips, no rule starts or stops
	// matching - only the custom-property invalidation can repaint this.
	ui::setStyleVariable(_local, "--k", "5");

	// ...and drop one entirely: the use must fall back to the var() default
	ui::removeStyleVariable(_removed, "--k");
}

void CalcLayout::report() {
	using document::ParameterName;

	expectVar("variable after change", _local, "--k", "5");
	expectMetric("resolved after change", _local, ParameterName::CssWidth, 50.0f);
	expectAppliedWidth("applied after change", _local, 50.0f);

	// the change reached the inheriting child too
	expectMetric("inherited value after change", _localChild, ParameterName::CssWidth, 10.0f);
	expectAppliedWidth("applied to the child after change", _localChild, 10.0f);

	// removal falls back to the var() default written in the sheet
	expectVar("variable after removal", _removed, "--k", StringView());
	expectAppliedWidth("applied after removal", _removed, 7.0f);

	log::source().warn("CalcTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

void CalcLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float rowH = 34.0f;
	const float top = getWorkTop() - 16.0f;

	Layer *boxes[] = {_sum, _diff, _mulRight, _mulLeft, _div, _parens, _nested, _percent, _withVar,
		_mixedUnits, _unitSquared, _divByZero, _unitPlusNumber, _unbalanced, _local, _localWins,
		_normalized, _removed};
	for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
		boxes[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		boxes[i]->setPosition(boxes[i]->getParent()->convertToNodeSpace(
				convertToWorldSpace(Vec2(40.0f, top - float(i) * rowH))));
		boxes[i]->setContentSize(Size2(boxes[i]->getContentSize().width, 24.0f));
	}

	// the inheriting child sits beside its parent rather than inside it
	_localChild->setAnchorPoint(Vec2(0.0f, 1.0f));
	_localChild->setPosition(Vec2(240.0f, 24.0f));
	_localChild->setContentSize(Size2(_localChild->getContentSize().width, 24.0f));
}

} // namespace stappler::xenolith::app
