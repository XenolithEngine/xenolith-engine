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

#include "css/CalcLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h"

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

/* min()/max()/clamp(): a value on their own, and a factor inside calc() */
.min-basic   { width: min(30px, 12px, 21px); }
.max-basic   { width: max(30px, 12px, 21px); }
.clamp-low   { width: clamp(20px, 8px, 60px); }
.clamp-mid   { width: clamp(20px, 35px, 60px); }
.clamp-high  { width: clamp(20px, 90px, 60px); }
.min-in-calc { width: calc(min(10px, 25px) * 3); }
.min-of-calc { width: min(calc(2 * 30px), 45px); }
.min-var     { width: min(var(--step), 4px); }

/* every one of these must be DROPPED, leaving `.box`'s 40px */
.mixed-units      { width: calc(100% - 20px); }
.unit-squared     { width: calc(2px * 3px); }
.div-by-zero      { width: calc(10px / 0); }
.unit-plus-number { width: calc(10px + 5); }
.unbalanced       { width: calc(10px + 5; }
.min-one          { width: min(10px); }
.min-mixed        { width: min(10px, 50%); }
.clamp-short      { width: clamp(10px, 20px); }
.clamp-mixed      { width: clamp(10px, 50%, 30px); }

/* node-local custom properties: --k is declared in C++, on the node */
.local       { width: calc(var(--k) * var(--step)); }
.local-child { width: calc(var(--k) * 2px); }
.normalized  { width: calc(var(--k) * 1px); }
.removed     { width: calc(var(--k, 7) * 1px); }

/* a sheet rule declaring the same name on the same node: the node-local one must win */
.local-wins { --k: 1; width: calc(var(--k) * 100px); }
)css");

} // namespace

Layer *CalcLayout::addBox(Node *parent, StringView cls) {
	auto box = parent->addChild(Rc<Layer>::create(Color::Black), ZOrder(1));
	box->addStyleClass("box");
	box->addStyleClass(cls);
	_samples.emplace_back(Sample{cls.str<Interface>(), box});
	return box;
}

Value CalcLayout::encodeSample(const Sample &sample) const {
	Value ret;

	auto st = ui::StyleResolver::resolveStyleForNode(sample.node);
	document::StyleValue v;
	if (st.getValue(document::ParameterName::CssWidth, v)) {
		ret.setBool(true, "has");
		// Hundredths: a percent is stored as a fraction (50% is 0.5), and the check compares
		// integers so that an expectation never depends on float formatting
		ret.setInteger(int64_t(std::lround(v.sizeValue.value * 100.0f)), "width");
		ret.setInteger(int64_t(toInt(v.sizeValue.metric)), "units");
	} else {
		ret.setBool(false, "has");
	}

	// What the layout COMMITTED. The runtime half of the test turns on this and not on the
	// resolved value: a custom property that changed moves nothing and matches no new rule, so
	// only invalidation can carry it into the applied size.
	ret.setInteger(int64_t(std::lround(sample.node->getContentSize().width * 100.0f)), "applied");

	ret.setString(st.getCustomProperty("--k"), "k");
	return ret;
}

bool CalcLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	for (auto &cls : {"sum", "diff", "mul-right", "mul-left", "div", "parens", "nested", "percent",
			 "with-var", "min-basic", "max-basic", "clamp-low", "clamp-mid", "clamp-high",
			 "min-in-calc", "min-of-calc", "min-var", "mixed-units", "unit-squared", "div-by-zero",
			 "unit-plus-number", "unbalanced", "min-one", "min-mixed", "clamp-short",
			 "clamp-mixed"}) {
		addBox(this, StringView(cls));
	}

	// --k lives on the node, not in the sheet. The child gets it by inheritance alone.
	_local = addBox(this, "local");
	ui::setStyleVariable(_local, "--k", "3");
	addBox(_local, "local-child");

	// the same name declared by a rule that matches this very node: node-local must still win
	auto localWins = addBox(this, "local-wins");
	ui::setStyleVariable(localWins, "--k", "2");

	// written without the leading dashes and in upper case
	auto normalized = addBox(this, "normalized");
	ui::setStyleVariable(normalized, "K", "25");

	_removed = addBox(this, "removed");
	ui::setStyleVariable(_removed, "--k", "30");

	return true;
}

void CalcLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float rowH = 22.0f;
	const float top = getWorkTop() - 16.0f;

	// Only the boxes owned directly by this layout are placed; `local-child` is laid out by its
	// parent, which is the point of it.
	size_t row = 0;
	for (auto &it : _samples) {
		if (it.node->getParent() != this) {
			continue;
		}
		it.node->setAnchorPoint(Vec2(0.0f, 1.0f));
		it.node->setPosition(Vec2(48.0f, top - float(row) * rowH));
		++row;
	}
}

Node *CalcLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	for (auto &it : _samples) {
		if (it.name == name) {
			return it.node;
		}
	}
	return nullptr;
}

void CalcLayout::registerCommands() {
	addCommand("state", "Every box: the width that resolved, the width applied, and --k",
			[this](Value &&) {
		Value ret;
		for (auto &it : _samples) { ret.setValue(encodeSample(it), it.name); }
		return ret;
	});

	addCommand("set-var", "Declare a custom property on a node: {target, name, value}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto node = getTarget(a)) {
			ui::setStyleVariable(node, a.getString("name"), a.getString("value"));
			ret.setBool(true, "applied");
		}
		return ret;
	});

	addCommand("remove-var", "Take one away: {target, name}", [this](Value &&args) {
		const Value &a = args;
		Value ret;
		if (auto node = getTarget(a)) {
			ui::removeStyleVariable(node, a.getString("name"));
			ret.setBool(true, "applied");
		}
		return ret;
	});
}

} // namespace stappler::xenolith::app
