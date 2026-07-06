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

// Verifies parsing of the flexbox & grid CSS properties added to the
// stappler_document CSS engine (SPDocStyleCss.cc). These properties are parsed
// and stored but not yet consumed by any layout, so the tests assert on the raw
// StyleParameter values a stylesheet declaration produces.

#include "SPCommon.h"
#include "SPMemInterface.h"
#include "SPDocument.h"
#include "SPDocStyle.h"
#include "SPDocStyleContainer.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

void performFlexboxGridCssTests() {
	using namespace stappler::document;

	sprt::cout << "\n== stappler document flexbox/grid css tests ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		auto data = new (pool) DocumentData(pool);
		auto container = new (pool) StyleContainer(data);

		// Parse a plain declaration list (no selector) into a reusable StyleList. It is
		// returned by reference: StyleList is an AllocPool type and must never be copied,
		// so every call clobbers the same scratch (fine - each assertion reads it at once).
		StyleList scratch;
		auto parse = [&](StringView css) -> StyleList & {
			scratch.data.clear();
			StringViewUtf8 r(css.data(), css.size());
			container->readStyle(scratch, r);
			return scratch;
		};

		// last matching value for a parameter (declarations carry MediaQueryIdNone)
		auto has = [&](const StyleList &style, ParameterName n) {
			return !style.get(n).empty();
		};
		auto val = [&](const StyleList &style, ParameterName n) -> StyleValue {
			auto v = style.get(n);
			return v.empty() ? StyleValue() : v.back().value;
		};
		// resolve an interned string id against the live string table (it grows on every
		// addString, so read it directly rather than through a cached SpanView)
		auto str = [&](const StyleList &style, ParameterName n) -> StringView {
			auto v = style.get(n);
			if (v.empty()) {
				return StringView();
			}
			auto id = v.back().value.stringId;
			return id < data->strings.size() ? data->strings[id] : StringView();
		};

		// ---- display ----
		check(val(parse("display: flex"), ParameterName::CssDisplay).display == Display::Flex,
				"flexgrid: display: flex");
		check(val(parse("display: inline-flex"), ParameterName::CssDisplay).display
						== Display::InlineFlex,
				"flexgrid: display: inline-flex");
		check(val(parse("display: grid"), ParameterName::CssDisplay).display == Display::Grid,
				"flexgrid: display: grid");
		check(val(parse("display: inline-grid"), ParameterName::CssDisplay).display
						== Display::InlineGrid,
				"flexgrid: display: inline-grid");

		// ---- flex container longhands ----
		check(val(parse("flex-direction: column-reverse"), ParameterName::CssFlexDirection)
						.flexDirection
						== FlexDirection::ColumnReverse,
				"flexgrid: flex-direction: column-reverse");
		check(val(parse("flex-wrap: wrap-reverse"), ParameterName::CssFlexWrap).flexWrap
						== FlexWrap::WrapReverse,
				"flexgrid: flex-wrap: wrap-reverse");

		// ---- flex-flow shorthand ----
		{
			auto &s = parse("flex-flow: row-reverse wrap");
			check(val(s, ParameterName::CssFlexDirection).flexDirection == FlexDirection::RowReverse
							&& val(s, ParameterName::CssFlexWrap).flexWrap == FlexWrap::Wrap,
					"flexgrid: flex-flow expands to direction + wrap");
		}

		// ---- box alignment (flex + grid) ----
		check(val(parse("justify-content: space-between"), ParameterName::CssJustifyContent).align
						== Align::SpaceBetween,
				"flexgrid: justify-content: space-between");
		check(val(parse("align-content: flex-start"), ParameterName::CssAlignContent).align
						== Align::FlexStart,
				"flexgrid: align-content: flex-start");
		check(val(parse("align-items: center"), ParameterName::CssAlignItems).align == Align::Center,
				"flexgrid: align-items: center");
		check(val(parse("justify-items: stretch"), ParameterName::CssJustifyItems).align
						== Align::Stretch,
				"flexgrid: justify-items: stretch");
		check(val(parse("align-self: baseline"), ParameterName::CssAlignSelf).align
						== Align::Baseline,
				"flexgrid: align-self: baseline");
		check(val(parse("justify-self: self-end"), ParameterName::CssJustifySelf).align
						== Align::SelfEnd,
				"flexgrid: justify-self: self-end");
		// `safe`/`unsafe` overflow prefix is dropped
		check(val(parse("justify-content: safe center"), ParameterName::CssJustifyContent).align
						== Align::Center,
				"flexgrid: justify-content: safe center -> center");

		// ---- place-* shorthands ----
		{
			auto &s = parse("place-items: center start");
			check(val(s, ParameterName::CssAlignItems).align == Align::Center
							&& val(s, ParameterName::CssJustifyItems).align == Align::Start,
					"flexgrid: place-items expands to align + justify");
		}
		{
			auto &s = parse("place-content: stretch");
			check(val(s, ParameterName::CssAlignContent).align == Align::Stretch
							&& val(s, ParameterName::CssJustifyContent).align == Align::Stretch,
					"flexgrid: place-content single value applies to both");
		}

		// ---- gaps ----
		{
			auto &s = parse("gap: 10px 20px");
			auto row = val(s, ParameterName::CssRowGap).sizeValue;
			auto col = val(s, ParameterName::CssColumnGap).sizeValue;
			check(row.metric == Metric::Units::Px && row.value == 10.0f
							&& col.metric == Metric::Units::Px && col.value == 20.0f,
					"flexgrid: gap: 10px 20px -> row + column");
		}
		{
			auto &s = parse("gap: 5px");
			check(val(s, ParameterName::CssRowGap).sizeValue.value == 5.0f
							&& val(s, ParameterName::CssColumnGap).sizeValue.value == 5.0f,
					"flexgrid: gap single value applies to both axes");
		}
		check(val(parse("row-gap: normal"), ParameterName::CssRowGap).sizeValue.metric
						== Metric::Units::Auto,
				"flexgrid: row-gap: normal -> auto");
		check(val(parse("column-gap: 2em"), ParameterName::CssColumnGap).sizeValue.metric
						== Metric::Units::Em,
				"flexgrid: column-gap: 2em");

		// ---- flex item longhands ----
		check(val(parse("order: -2"), ParameterName::CssOrder).intValue == -2,
				"flexgrid: order: -2 (signed)");
		check(val(parse("flex-grow: 2.5"), ParameterName::CssFlexGrow).floatValue == 2.5f,
				"flexgrid: flex-grow: 2.5");
		check(val(parse("flex-shrink: 0"), ParameterName::CssFlexShrink).floatValue == 0.0f,
				"flexgrid: flex-shrink: 0");
		{
			auto basis = val(parse("flex-basis: 30%"), ParameterName::CssFlexBasis).sizeValue;
			check(basis.metric == Metric::Units::Percent && basis.value == 0.3f,
					"flexgrid: flex-basis: 30%");
		}
		check(val(parse("flex-basis: content"), ParameterName::CssFlexBasis).sizeValue.metric
						== Metric::Units::Auto,
				"flexgrid: flex-basis: content -> auto");

		// ---- flex shorthand ----
		{
			auto &s = parse("flex: none"); // 0 0 auto
			check(val(s, ParameterName::CssFlexGrow).floatValue == 0.0f
							&& val(s, ParameterName::CssFlexShrink).floatValue == 0.0f
							&& val(s, ParameterName::CssFlexBasis).sizeValue.metric
									== Metric::Units::Auto,
					"flexgrid: flex: none -> 0 0 auto");
		}
		{
			auto &s = parse("flex: 1"); // 1 1 0px
			check(val(s, ParameterName::CssFlexGrow).floatValue == 1.0f
							&& val(s, ParameterName::CssFlexShrink).floatValue == 1.0f
							&& val(s, ParameterName::CssFlexBasis).sizeValue.metric
									== Metric::Units::Px
							&& val(s, ParameterName::CssFlexBasis).sizeValue.value == 0.0f,
					"flexgrid: flex: 1 -> 1 1 0");
		}
		{
			auto &s = parse("flex: 2 3 40px");
			check(val(s, ParameterName::CssFlexGrow).floatValue == 2.0f
							&& val(s, ParameterName::CssFlexShrink).floatValue == 3.0f
							&& val(s, ParameterName::CssFlexBasis).sizeValue.value == 40.0f,
					"flexgrid: flex: 2 3 40px");
		}
		{
			auto &s = parse("flex: auto"); // 1 1 auto
			check(val(s, ParameterName::CssFlexGrow).floatValue == 1.0f
							&& val(s, ParameterName::CssFlexShrink).floatValue == 1.0f
							&& val(s, ParameterName::CssFlexBasis).sizeValue.metric
									== Metric::Units::Auto,
					"flexgrid: flex: auto -> 1 1 auto");
		}

		// ---- grid container ----
		check(str(parse("grid-template-columns: 1fr 2fr 100px"),
					   ParameterName::CssGridTemplateColumns)
						== "1fr 2fr 100px",
				"flexgrid: grid-template-columns stored verbatim");
		check(str(parse("grid-template-areas: \"a b\" \"c d\""),
					   ParameterName::CssGridTemplateAreas)
						== "\"a b\" \"c d\"",
				"flexgrid: grid-template-areas stored verbatim (case preserved)");
		check(val(parse("grid-auto-flow: column dense"), ParameterName::CssGridAutoFlow).gridAutoFlow
						== GridAutoFlow::ColumnDense,
				"flexgrid: grid-auto-flow: column dense");
		check(val(parse("grid-auto-flow: dense"), ParameterName::CssGridAutoFlow).gridAutoFlow
						== GridAutoFlow::RowDense,
				"flexgrid: grid-auto-flow: dense -> row dense");

		// ---- grid-template shorthand (rows / columns form) ----
		{
			auto &s = parse("grid-template: auto 1fr / 100px 1fr");
			check(str(s, ParameterName::CssGridTemplateRows) == "auto 1fr"
							&& str(s, ParameterName::CssGridTemplateColumns) == "100px 1fr",
					"flexgrid: grid-template expands rows / columns");
		}

		// ---- grid item ----
		{
			auto &s = parse("grid-column: 1 / 3");
			check(str(s, ParameterName::CssGridColumnStart) == "1"
							&& str(s, ParameterName::CssGridColumnEnd) == "3",
					"flexgrid: grid-column: 1 / 3");
		}
		{
			auto &s = parse("grid-row: 2 / span 2");
			check(str(s, ParameterName::CssGridRowStart) == "2"
							&& str(s, ParameterName::CssGridRowEnd) == "span 2",
					"flexgrid: grid-row: 2 / span 2");
		}
		{
			auto &s = parse("grid-area: 1 / 2 / 3 / 4");
			check(str(s, ParameterName::CssGridRowStart) == "1"
							&& str(s, ParameterName::CssGridColumnStart) == "2"
							&& str(s, ParameterName::CssGridRowEnd) == "3"
							&& str(s, ParameterName::CssGridColumnEnd) == "4",
					"flexgrid: grid-area: 1 / 2 / 3 / 4");
		}

		// ---- inheritance: none of the flex/grid props inherit ----
		{
			auto &s = parse("order: 5; align-self: center; gap: 4px; grid-column: 1 / 2");
			StyleList inherited;
			inherited.merge(s, true);
			check(!has(inherited, ParameterName::CssOrder)
							&& !has(inherited, ParameterName::CssAlignSelf)
							&& !has(inherited, ParameterName::CssRowGap)
							&& !has(inherited, ParameterName::CssGridColumnStart),
					"flexgrid: flex/grid properties are not inheritable");
		}
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
