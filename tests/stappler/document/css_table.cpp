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

// Verifies parsing of the table CSS properties consumed by xenolith::ui::LayoutSystem in Table /
// TableRow mode. These are parse-level assertions on the raw StyleParameter a declaration produces;
// what the layout then does with them is covered by the window-level TableLayout test.
//
// Two of these tests exist because of a specific failure mode rather than to cover a feature:
// - the `css()` round-trip catches a ParameterName added without a case in the printer switch
//   (that switch has no `default:`, so the compiler normally catches it - but only while -Wswitch
//   is fatal);
// - the inheritance block catches the opposite mistake, since StyleList::isInheritable is a
//   BLOCKLIST: a new property inherits into the whole subtree unless it is listed there.

#include "SPCommon.h"
#include "SPMemInterface.h"
#include "SPDocument.h"
#include "SPDocStyle.h"
#include "SPDocStyleContainer.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

void performTableCssTests() {
	using namespace stappler::document;

	sprt::cout << "\n== stappler document table css tests ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		auto data = new (pool) DocumentData(pool);
		auto container = new (pool) StyleContainer(data);

		// Same harness as css_flexgrid: a declaration list parsed into a reusable scratch StyleList
		// (an AllocPool type that must never be copied, so every call clobbers the same one).
		StyleList scratch;
		auto parse = [&](StringView css) -> StyleList & {
			scratch.data.clear();
			StringViewUtf8 r(css.data(), css.size());
			container->readStyle(scratch, r);
			return scratch;
		};

		auto has = [&](const StyleList &style, ParameterName n) { return !style.get(n).empty(); };
		auto val = [&](const StyleList &style, ParameterName n) -> StyleValue {
			auto v = style.get(n);
			return v.empty() ? StyleValue() : v.back().value;
		};

		// ---- display ----
		// Every one of these was silently dropped before the table work: the display parser knew
		// none of the table keywords, even though Display carried most of the values.
		check(val(parse("display: table"), ParameterName::CssDisplay).display == Display::Table,
				"table: display: table");
		check(val(parse("display: table-row"), ParameterName::CssDisplay).display
						== Display::TableRow,
				"table: display: table-row");
		check(val(parse("display: table-cell"), ParameterName::CssDisplay).display
						== Display::TableCell,
				"table: display: table-cell");
		check(val(parse("display: table-column"), ParameterName::CssDisplay).display
						== Display::TableColumn,
				"table: display: table-column");
		check(val(parse("display: table-caption"), ParameterName::CssDisplay).display
						== Display::TableCaption,
				"table: display: table-caption");
		// still rejected - the row-group family is out of scope, and an unparsed value must not
		// silently become some other display
		check(!has(parse("display: table-row-group"), ParameterName::CssDisplay),
				"table: display: table-row-group is rejected, not coerced");

		// ---- table-layout ----
		check(val(parse("table-layout: auto"), ParameterName::CssTableLayout).tableLayout
						== TableLayout::Auto,
				"table: table-layout: auto");
		check(val(parse("table-layout: fixed"), ParameterName::CssTableLayout).tableLayout
						== TableLayout::Fixed,
				"table: table-layout: fixed");
		check(!has(parse("table-layout: bogus"), ParameterName::CssTableLayout),
				"table: table-layout rejects an unknown keyword");

		// ---- border-spacing: one value fills both axes, two are h then v ----
		{
			auto &s = parse("border-spacing: 4px");
			check(val(s, ParameterName::CssBorderSpacingHorizontal).sizeValue.value == 4.0f
							&& val(s, ParameterName::CssBorderSpacingVertical).sizeValue.value
									== 4.0f,
					"table: border-spacing: 4px fills both axes");
		}
		{
			auto &s = parse("border-spacing: 4px 8px");
			check(val(s, ParameterName::CssBorderSpacingHorizontal).sizeValue.value == 4.0f
							&& val(s, ParameterName::CssBorderSpacingVertical).sizeValue.value
									== 8.0f,
					"table: border-spacing: 4px 8px is horizontal then vertical");
		}

		// ---- spans ----
		check(val(parse("-xl-column-span: 2"), ParameterName::CssXlColumnSpan).uintValue == 2,
				"table: -xl-column-span: 2");
		check(val(parse("-xl-row-span: 3"), ParameterName::CssXlRowSpan).uintValue == 3,
				"table: -xl-row-span: 3");
		// a span is a count, so zero and negative are not "clamped to 1" - they are not a span
		check(!has(parse("-xl-column-span: 0"), ParameterName::CssXlColumnSpan),
				"table: -xl-column-span: 0 is rejected");
		check(!has(parse("-xl-row-span: -1"), ParameterName::CssXlRowSpan),
				"table: -xl-row-span: -1 is rejected");

		// ---- border-collapse (pre-existing, but the table is its first real consumer) ----
		check(val(parse("border-collapse: collapse"), ParameterName::CssBorderCollapse)
								.borderCollapse
						== BorderCollapse::Collapse,
				"table: border-collapse: collapse");
		check(val(parse("border-collapse: separate"), ParameterName::CssBorderCollapse)
								.borderCollapse
						== BorderCollapse::Separate,
				"table: border-collapse: separate");

		// ---- per-side borders reach the style list ----
		// They always parsed; nothing in the ui kit read them until table cells did. Assert the
		// whole longhand triple, since border collapse resolution compares all three.
		{
			auto &s = parse("border-bottom: 2px solid #ff0000");
			check(val(s, ParameterName::CssBorderBottomWidth).sizeValue.value == 2.0f
							&& val(s, ParameterName::CssBorderBottomStyle).borderStyle
									== BorderStyle::Solid
							&& val(s, ParameterName::CssBorderBottomColor).color4
									== Color4B(255, 0, 0, 255),
					"table: border-bottom shorthand expands to width/style/color");
		}

		// ---- the column template is grid-template-columns, reused verbatim ----
		check(has(parse("display: table; grid-template-columns: 120px 1fr 2fr"),
					  ParameterName::CssGridTemplateColumns),
				"table: grid-template-columns carries the column track list");

		// ---- printer round-trip: a missing case in StyleList::css() would drop the property ----
		{
			auto &s = parse("table-layout: fixed; border-spacing: 4px 8px; -xl-column-span: 2; "
							"-xl-row-span: 3; display: table-row");
			auto css = s.css();
			auto out = StringView(css);
			check(out.find("table-layout: fixed") != maxOf<size_t>()
							&& out.find("-xl-column-span: 2") != maxOf<size_t>()
							&& out.find("-xl-row-span: 3") != maxOf<size_t>()
							&& out.find("table-row") != maxOf<size_t>(),
					"table: every new property survives the css() round-trip");
		}

		// ---- inheritance ----
		// isInheritable is a blocklist, so an omission here leaks the property into every
		// descendant - a cell would inherit the table's span and spacing.
		{
			auto &s = parse("table-layout: fixed; border-spacing: 4px; -xl-column-span: 2; "
							"-xl-row-span: 3");
			StyleList inherited;
			inherited.merge(s, true);
			check(!has(inherited, ParameterName::CssTableLayout)
							&& !has(inherited, ParameterName::CssBorderSpacingHorizontal)
							&& !has(inherited, ParameterName::CssBorderSpacingVertical)
							&& !has(inherited, ParameterName::CssXlColumnSpan)
							&& !has(inherited, ParameterName::CssXlRowSpan),
					"table: table-layout / border-spacing / spans are not inheritable");
		}
		{
			// ... but border-collapse IS inherited, in CSS and here: a table declares it once and
			// the cells that resolve their own borders against it have to see it.
			auto &s = parse("border-collapse: collapse");
			StyleList inherited;
			inherited.merge(s, true);
			check(has(inherited, ParameterName::CssBorderCollapse),
					"table: border-collapse stays inheritable");
		}
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
