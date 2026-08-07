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

#include "css/InheritedStyleLayout.h"
#include "XLInheritedStyle.h"
#include "XLUiStyleResolver.h"
#include "XLAction.h"
#include "SPFilesystem.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;
using basic2d::Layer;

namespace {

static constexpr auto s_styledCss = StringView(R"css(
.box { color: #43a047; font-size: 28px; font-weight: bold; text-transform: uppercase; }
)css");

// the `.box` rule is gone: the resolver must REMOVE the inherited components
static constexpr auto s_plainCss = StringView(R"css(
.unused { background-color: #000000; }
)css");

static const Color3B s_green(0x43, 0xa0, 0x47);

} // namespace

void InheritedStyleLayout::writeCss(bool styled) {
	auto text = styled ? s_styledCss : s_plainCss;
	filesystem::write(FileInfo(_cssPath, FileCategory::Custom),
			BytesView((const uint8_t *)text.data(), text.size()));
}

// The ancestor-walk label has NO reactivity to ancestor component changes by design (see
// XLInheritedStyle.h) - the test plays the user's role and forces a re-layout after each
// CSS change (any dirtying setter works; text-indent is visually negligible)
void InheritedStyleLayout::nudgeAncestorLabel() {
	++_nudges;
	_labelAncestor->setTextIndent(0.01f * float(_nudges));
}

bool InheritedStyleLayout::init() {
	_cssPath = ::getenv("XL_INHERITED_CSS_FILE") ? String(::getenv("XL_INHERITED_CSS_FILE"))
												 : String("/tmp/xl-inherited-style-test.css");

	// seed the file BEFORE the StyleSystem reads it
	writeCss(true);

	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(FileInfo(_cssPath, FileCategory::Custom));

	auto makeLabel = [](Node *parent, StringView text) {
		auto label = parent->addChild(Rc<Label>::create(), ZOrder(1));
		label->setFontSize(14);
		label->setColor(Color::Black, false);
		label->setString(text);
		label->setAnchorPoint(Vec2(0.0f, 0.0f));
		label->setPosition(Vec2(12.0f, 12.0f));
		return label;
	};

	// 1. recursive resolver: the label itself is resolved too, so it carries its OWN
	// inherited components (cascade brings the .box values down at resolve time)
	_containerRecursive = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_containerRecursive->addStyleClass("box");
	_containerRecursive->addSystem(Rc<ui::StyleResolver>::create(true));
	_labelRecursive = makeLabel(_containerRecursive, "recursive resolver");

	// 2. non-recursive resolver: only the container is styled; the label must find the
	// components by walking the parent chain (accumulateInheritedStyle)
	_containerAncestor = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));
	_containerAncestor->addStyleClass("box");
	_containerAncestor->addSystem(Rc<ui::StyleResolver>::create());
	_labelAncestor = makeLabel(_containerAncestor, "ancestor walk");

	// 3. reference: same explicit style, no styled ancestor
	_labelReference = makeLabel(this, "reference label");

	// phase 1 checks run after the initial style application settles, then the CSS is
	// rewritten without the .box rule; phase 2 verifies the revert to explicit values
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.2f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.6f),
			[this] {
		// the CSS reload has settled by now; play the user's role for the ancestor-walk
		// label (no reactivity to ancestor component changes by design)
		nudgeAncestorLabel();
	}, Rc<DelayTime>::create(0.6f), [this] { runPhase2(); }));

	return true;
}

void InheritedStyleLayout::runPhase1() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("InheritedStyleTest", "phase1: ", what);
		}
	};

	auto fontComponent = _containerRecursive->getComponent<InheritedFontStyle>();
	expect(fontComponent != nullptr, "recursive container has no InheritedFontStyle");
	if (fontComponent) {
		expect((fontComponent->defined & InheritedFontStyle::DefinedFontSize)
						&& fontComponent->fontSize == font::FontSize(28),
				"recursive container font-size != 28");
		expect((fontComponent->defined & InheritedFontStyle::DefinedFontWeight)
						&& fontComponent->fontWeight == font::FontWeight::Bold,
				"recursive container font-weight != bold");
	}
	auto colorComponent = _containerRecursive->getComponent<InheritedColorStyle>();
	expect(colorComponent != nullptr
					&& (colorComponent->defined & InheritedColorStyle::DefinedColor)
					&& colorComponent->color == s_green,
			"recursive container color != green");

	// stored explicit values must never be overwritten by inheritance
	expect(_labelRecursive->getFontSize() == font::FontSize(14),
			"recursive label stored font size was overwritten");
	expect(_labelAncestor->getFontSize() == font::FontSize(14),
			"ancestor label stored font size was overwritten");

	// the recursive resolver styles the label itself (cascade brings the inherited values)
	expect(_labelRecursive->getComponent<InheritedFontStyle>() != nullptr,
			"recursive label did not receive its own InheritedFontStyle");

	// non-recursive resolver never touches the label; the values come from the parent chain
	expect(_labelAncestor->getComponent<InheritedFontStyle>() == nullptr,
			"ancestor label unexpectedly carries its own InheritedFontStyle");
	auto accumulated = accumulateInheritedStyle<InheritedFontStyle>(_labelAncestor);
	expect((accumulated.defined & InheritedFontStyle::DefinedFontSize)
					&& accumulated.fontSize == font::FontSize(28),
			"ancestor walk did not accumulate font-size 28");

	// rendered layout must follow the inherited 28px (reference renders explicit 14px)
	const float ref = _labelReference->getContentSize().height;
	expect(_labelRecursive->getContentSize().height > ref + 4.0f,
			"recursive label does not render with inherited font size");
	expect(_labelAncestor->getContentSize().height > ref + 4.0f,
			"ancestor label does not render with inherited font size");

	log::source().warn("InheritedStyleTest", "phase1 done: ", _checks, " checks, ", _failures,
			" failures; rewriting css without .box");

	writeCss(false);
}

void InheritedStyleLayout::runPhase2() {
	auto expect = [&](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("InheritedStyleTest", "phase2: ", what);
		}
	};

	// nothing inheritable remains anywhere up the recursive label's chain
	expect(accumulateInheritedStyle<InheritedFontStyle>(_labelRecursive).defined == 0,
			"recursive label still accumulates inherited font values");

	// the .box rule is gone - the resolver must have removed the components
	expect(_containerRecursive->getComponent<InheritedFontStyle>() == nullptr,
			"recursive container still carries InheritedFontStyle");
	expect(_containerRecursive->getComponent<InheritedColorStyle>() == nullptr,
			"recursive container still carries InheritedColorStyle");
	expect(_containerAncestor->getComponent<InheritedFontStyle>() == nullptr,
			"ancestor container still carries InheritedFontStyle");

	expect(_labelRecursive->getComponent<InheritedFontStyle>() == nullptr,
			"recursive label still carries its own InheritedFontStyle");

	// with the components gone, the explicit 14px/black is in effect again
	const float ref = _labelReference->getContentSize().height;
	expect(sprt::abs(_labelRecursive->getContentSize().height - ref) < 2.0f,
			"recursive label did not revert to explicit font size");
	expect(sprt::abs(_labelAncestor->getContentSize().height - ref) < 2.0f,
			"ancestor label did not revert to explicit font size");

	log::source().warn("InheritedStyleTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

void InheritedStyleLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();
	const float rowH = 120.0f;
	const float top = getWorkTop() - 160.0f;

	Layer *containers[] = {_containerRecursive, _containerAncestor};
	for (size_t i = 0; i < 2; ++i) {
		containers[i]->setAnchorPoint(Vec2(0.0f, 0.0f));
		containers[i]->setContentSize(Size2(560.0f, 96.0f));
		containers[i]->setPosition(Vec2(24.0f, top - float(i) * rowH));
	}

	_labelReference->setPosition(Vec2(36.0f, top - 2.0f * rowH + 12.0f));
}

} // namespace stappler::xenolith::app
