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

// Verifies the stappler_document CSS semantics that the ui style
// subsystem (xenolith/renderer/ui/style/XLUiStyle*) relies on:
// selector matching order, merge/override behavior, inheritance filtering
// and media query evaluation.

#include "SPCommon.h"
#include "SPMemInterface.h"
#include "SPDocument.h"
#include "SPDocStyleContainer.h"
#include "SPDocNode.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

void performCssTests() {
	using namespace stappler::document;

	sprt::cout << "\n== stappler document css tests ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		auto data = new (pool) DocumentData(pool);
		auto container = new (pool) StyleContainer(data);

		StringViewUtf8 css(R"Css(
			* { opacity: 0.25; }
			label { color: #ff0000; width: 100px; font-size: 14px; }
			.red { color: #00ff00; }
			label.red { height: 40px; }
			#title { color: #0000ff; }
			@media (orientation: portrait) { label { width: 50%; } }
		)Css");
		check(container->readStyle(css), "css: stylesheet parsed");

		MediaParameters media;
		media.surfaceSize = Size2(800.0f, 600.0f);
		media.orientation = Orientation::Landscape;
		auto resolved = media.resolveMediaQueries<memory::PoolInterface>(data->queries);

		Node node("label");
		node.setAttribute("class", "red");
		node.setAttribute("id", "title");

		StyleList style;
		container->resolveNodeStyle(style, node, SpanView<const Node *>(), media, resolved);

		SimpleStyleInterface iface(resolved, data->strings, 1.0f, 1.0f);

		// match+override order: tag -> .class -> tag.class -> #id (last wins)
		auto text = style.compileTextLayout(&iface);
		check(text.color == sprt::geom::Color3B(0, 0, 255), "css: #id overrides .class and tag");

		auto block = style.compileBlockModel(&iface);
		check(block.width.metric == Metric::Units::Px && block.width.value == 100.0f,
				"css: tag width matched (landscape: media rule inactive)");
		check(block.height.metric == Metric::Units::Px && block.height.value == 40.0f,
				"css: tag.class combined selector matched");

		auto font = style.compileFontStyle(&iface);
		check(font.fontSize.get() == 14, "css: font-size compiled");

		// universal selector merges inheritable parameters only (upstream quirk);
		// opacity IS inheritable, so it must be present
		check(!style.get(ParameterName::CssOpacity, &iface).empty(),
				"css: universal selector applied");

		// media query flip: portrait activates the 50% width override
		media.orientation = Orientation::Portrait;
		media.surfaceSize = Size2(600.0f, 800.0f);
		auto resolvedPortrait = media.resolveMediaQueries<memory::PoolInterface>(data->queries);

		StyleList portraitStyle;
		container->resolveNodeStyle(portraitStyle, node, SpanView<const Node *>(), media,
				resolvedPortrait);
		auto portraitBlock = portraitStyle.compileBlockModel(&iface);
		check(portraitBlock.width.metric == Metric::Units::Percent
						&& portraitBlock.width.value == 0.5f,
				"css: @media (orientation) rule active in portrait");
		check(media.computeValueAuto(portraitBlock.width, 600.0f) == 300.0f,
				"css: percent width computes against base");

		// inheritance filter: color inherits, box model does not
		StyleList inherited;
		inherited.merge(style, true);
		check(!inherited.get(ParameterName::CssColor, &iface).empty(),
				"css: color is inheritable");
		check(inherited.get(ParameterName::CssWidth, &iface).empty(),
				"css: width is not inheritable");

		// unmatched identity gets nothing except the universal selector
		Node other("layer");
		StyleList otherStyle;
		container->resolveNodeStyle(otherStyle, other, SpanView<const Node *>(), media, resolved);
		check(otherStyle.get(ParameterName::CssColor, &iface).empty(),
				"css: no tag/class/id match for unrelated node");
		check(!otherStyle.get(ParameterName::CssOpacity, &iface).empty(),
				"css: universal selector still applies");

		// A stylesheet may open with a comment. `*` is a selector start, so the opening `/*` used
		// to be mistaken for a universal selector and the WHOLE file was rejected as ill-formed.
		auto leadingData = new (pool) DocumentData(pool);
		auto leadingContainer = new (pool) StyleContainer(leadingData);
		StringViewUtf8 leadingCss(R"Css(/* a leading comment, with a : and a * inside */
			label { color: #123456; }
		)Css");
		check(leadingContainer->readStyle(leadingCss), "css: stylesheet may start with a comment");

		Node leadingNode("label");
		StyleList leadingStyle;
		leadingContainer->resolveNodeStyle(leadingStyle, leadingNode, SpanView<const Node *>(),
				media, resolved);
		SimpleStyleInterface leadingIface(resolved, leadingData->strings, 1.0f, 1.0f);
		check(!leadingStyle.get(ParameterName::CssColor, &leadingIface).empty(),
				"css: the rule after a leading comment still applies");

		// NB: multi-class compounds (`.a.b`) are matched by the scene-graph path only
		// (StyleSheet::collectMatches / XL_SPECIFICITY_TEST). resolveNodeStyle here is the
		// document renderer's simple-key path, which reads no structured rule at all.

		/* ---- direction, unicode-bidi and the logical box properties -------------------------
		
		The four claims that the whole RTL feature stands on, asserted at the layer that decides
		them. Note especially the LAST one: `direction: rtl` must NOT move `padding-left`. An
		engine where it did would be non-standard, and every stylesheet written against it would be
		wrong on the web and wrong in any other engine. */
		auto dirData = new (pool) DocumentData(pool);
		auto dirContainer = new (pool) StyleContainer(dirData);
		StringViewUtf8 dirCss(R"Css(
			#root { direction: rtl; unicode-bidi: plaintext; text-align: start; }
			label { padding-inline-start: 12px; padding-inline-end: 4px;
			        padding-block-start: 7px; margin-inline: 3px 9px;
			        padding-left: 21px; }
		)Css");
		check(dirContainer->readStyle(dirCss), "css: a sheet declaring direction parses");

		// The media flag the engine seeds from the locale, in the position a real sheet uses it.
		auto optData = new (pool) DocumentData(pool);
		auto optContainer = new (pool) StyleContainer(optData);
		StringViewUtf8 optCss(R"Css(
			label { color: #111111; }
			@media (x-option: rtl) { :root { direction: rtl; } }
			div { color: #222222; }
		)Css");
		check(optContainer->readStyle(optCss),
				"css: `@media (x-option: rtl)` does not reject the sheet");

		Node dirRoot("div");
		dirRoot.setAttribute("id", "root");
		StyleList dirStyle;
		dirContainer->resolveNodeStyle(dirStyle, dirRoot, SpanView<const Node *>(), media,
				resolved);
		SimpleStyleInterface dirIface(resolved, dirData->strings, 1.0f, 1.0f);

		auto readEnum = [&](StyleList &list, ParameterName name, StyleValue &out) {
			auto v = list.get(name, &dirIface);
			if (v.empty()) {
				return false;
			}
			out = v.front().value;
			return true;
		};

		StyleValue v;
		check(readEnum(dirStyle, ParameterName::CssDirection, v)
						&& v.textDirection == TextDirection::RightToLeft,
				"css: `direction: rtl` parses");
		check(readEnum(dirStyle, ParameterName::CssUnicodeBidi, v)
						&& v.bidiMode == BidiMode::Plaintext,
				"css: `unicode-bidi: plaintext` parses");
		check(readEnum(dirStyle, ParameterName::CssTextAlign, v) && v.textAlign == TextAlign::Start,
				"css: `text-align: start` parses - the keyword the formatter always understood");

		Node dirLabel("label");
		StyleList boxStyle;
		dirContainer->resolveNodeStyle(boxStyle, dirLabel, SpanView<const Node *>(), media,
				resolved);
		auto metricOf = [&](ParameterName name, float &out) {
			auto p = boxStyle.get(name, &dirIface);
			if (p.empty()) {
				return false;
			}
			out = p.front().value.sizeValue.value;
			return true;
		};

		float m = 0.0f;
		check(metricOf(ParameterName::CssPaddingInlineStart, m) && m == 12.0f,
				"css: `padding-inline-start` keeps a name of its own");
		check(metricOf(ParameterName::CssPaddingInlineEnd, m) && m == 4.0f,
				"css: ... and so does `padding-inline-end`");
		check(metricOf(ParameterName::CssMarginInlineStart, m) && m == 3.0f,
				"css: the `margin-inline` shorthand fills start from the first value");
		check(metricOf(ParameterName::CssMarginInlineEnd, m) && m == 9.0f,
				"css: ... and end from the second");
		// The block axis has no writing-mode to vary with, so it folds onto the physical side AT
		// PARSE TIME - and the fold is exact, which is why it is allowed here and the inline fold
		// is not.
		check(metricOf(ParameterName::CssPaddingTop, m) && m == 7.0f,
				"css: `padding-block-start` folds onto `padding-top`");
		check(metricOf(ParameterName::CssPaddingLeft, m) && m == 21.0f,
				"css: and `padding-left` survives beside the logical pair, in its own slot");

		/* THE REGRESSION TEST FOR Р1. `direction` is inherited, so it reaches this label - and
		`padding-left` still means the LEFT edge. If somebody ever "helpfully" makes the physical
		properties follow the direction, this is the check that says no. */
		StyleList inheritedDir;
		inheritedDir.merge(dirStyle, true);
		check(readEnum(inheritedDir, ParameterName::CssDirection, v)
						&& v.textDirection == TextDirection::RightToLeft,
				"css: `direction` is inherited");
		check(inheritedDir.get(ParameterName::CssUnicodeBidi, &dirIface).empty(),
				"css: `unicode-bidi` is NOT inherited, as on the web");
		check(inheritedDir.get(ParameterName::CssPaddingInlineStart, &dirIface).empty(),
				"css: nor are the inline-axis box properties");
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
