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

// Verifies the stappler_document CSS semantics that the simpleui style
// subsystem (xenolith/renderer/simpleui/XLSimpleStyle*) relies on:
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
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
