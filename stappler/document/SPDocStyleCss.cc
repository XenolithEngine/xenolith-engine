/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPDocParser.h"
#include "SPDocStyleContainer.h"

namespace STAPPLER_VERSIONIZED stappler::document {

static bool css_readListStyleType(const StringView &value, const StyleCallback &cb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::None));
	} else if (value.equals("circle")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Circle));
	} else if (value.equals("disc")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Disc));
	} else if (value.equals("square")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Square));
	} else if (value.equals("x-mdash")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::XMdash));
	} else if (value.equals("decimal")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Decimal));
	} else if (value.equals("decimal-leading-zero")) {
		return cb(StyleParameter::create<ParameterName::CssListStyleType>(
				ListStyleType::DecimalLeadingZero));
	} else if (value.equals("lower-alpha") || value.equals("lower-latin")) {
		return cb(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::LowerAlpha));
	} else if (value.equals("lower-greek")) {
		return cb(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::LowerGreek));
	} else if (value.equals("lower-roman")) {
		return cb(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::LowerRoman));
	} else if (value.equals("upper-alpha") || value.equals("upper-latin")) {
		return cb(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::UpperAlpha));
	} else if (value.equals("upper-roman")) {
		return cb(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::UpperRoman));
	}
	return false;
}

template <ParameterName Name>
static bool css_readBorderStyle(const StringView &value, const StyleCallback &cb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<Name>(BorderStyle::None));
	} else if (value.equals("solid")) {
		return cb(StyleParameter::create<Name>(BorderStyle::Solid));
	} else if (value.equals("dotted")) {
		return cb(StyleParameter::create<Name>(BorderStyle::Dotted));
	} else if (value.equals("dashed")) {
		return cb(StyleParameter::create<Name>(BorderStyle::Dashed));
	}
	return false;
}

template <ParameterName Name>
static bool css_readBorderColor(const StringView &value, const StyleCallback &cb) {
	if (value.equals("transparent")) {
		return cb(StyleParameter::create<Name>(Color4B(255, 255, 255, 0)));
	}

	Color4B color;
	if (sprt::geom::readColor(value, color)) {
		return cb(StyleParameter::create<Name>(color));
	}
	return false;
}

template <ParameterName Name>
static bool css_readBorderWidth(const StringView &value, const StyleCallback &cb) {
	if (value.equals("thin")) {
		return cb(StyleParameter::create<Name>(Metric(2.0f, Metric::Units::Px)));
	} else if (value.equals("medium")) {
		return cb(StyleParameter::create<Name>(Metric(4.0f, Metric::Units::Px)));
	} else if (value.equals("thick")) {
		return cb(StyleParameter::create<Name>(Metric(6.0f, Metric::Units::Px)));
	}

	Metric v;
	if (parser::readStyleMetric(value, v)) {
		return cb(StyleParameter::create<Name>(v));
	}
	return false;
}

template <ParameterName Style, ParameterName Color, ParameterName Width>
static bool css_readBorder(const StringView &value, const StyleCallback &cb) {
	bool ret = true;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &str) {
		if (!css_readBorderStyle<Style>(str, cb)) {
			if (!css_readBorderColor<Color>(str, cb)) {
				if (!css_readBorderWidth<Width>(str, cb)) {
					ret = false;
				}
			}
		}
	});
	return ret;
}

template <typename T, typename Getter>
static void css_readQuadValue(const StringView &value, T &top, T &right, T &bottom, T &left,
		const Getter &g) {
	int count = 0;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		count++;
		if (count == 1) {
			top = right = bottom = left = g(r);
		} else if (count == 2) {
			right = left = g(r);
		} else if (count == 3) {
			bottom = g(r);
		} else if (count == 4) {
			left = g(r);
		}
	});
}

bool css_readAspectRatioValue(StringView str, float &value) {
	float first, second;

	if (!str.readFloat().grab(first)) {
		return false;
	}

	if (str.empty()) {
		value = first;
		return true;
	}

	str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (str.is('/')) {
		++str;
		str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

		if (!str.readFloat().grab(second)) {
			return false;
		} else {
			value = first / second;
			return true;
		}
	}

	return false;
}

// CSS Box Alignment keyword; accepts an optional `safe`/`unsafe` overflow-alignment
// prefix (dropped - it is an overflow strategy, not a distinct alignment)
static bool css_readAlignValue(StringView value, Align &out) {
	value.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (value.starts_with("safe ")) {
		value += "safe "_len;
		value.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	} else if (value.starts_with("unsafe ")) {
		value += "unsafe "_len;
		value.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	}

	if (value.equals("auto")) {
		out = Align::Auto;
	} else if (value.equals("normal")) {
		out = Align::Normal;
	} else if (value.equals("stretch")) {
		out = Align::Stretch;
	} else if (value.equals("baseline")) {
		out = Align::Baseline;
	} else if (value.equals("first baseline")) {
		out = Align::FirstBaseline;
	} else if (value.equals("last baseline")) {
		out = Align::LastBaseline;
	} else if (value.equals("center")) {
		out = Align::Center;
	} else if (value.equals("start")) {
		out = Align::Start;
	} else if (value.equals("end")) {
		out = Align::End;
	} else if (value.equals("self-start")) {
		out = Align::SelfStart;
	} else if (value.equals("self-end")) {
		out = Align::SelfEnd;
	} else if (value.equals("flex-start")) {
		out = Align::FlexStart;
	} else if (value.equals("flex-end")) {
		out = Align::FlexEnd;
	} else if (value.equals("left")) {
		out = Align::Left;
	} else if (value.equals("right")) {
		out = Align::Right;
	} else if (value.equals("space-between")) {
		out = Align::SpaceBetween;
	} else if (value.equals("space-around")) {
		out = Align::SpaceAround;
	} else if (value.equals("space-evenly")) {
		out = Align::SpaceEvenly;
	} else {
		return false;
	}
	return true;
}

template <ParameterName Name>
static bool css_readAlign(const StringView &value, const StyleCallback &cb) {
	Align a;
	if (css_readAlignValue(value, a)) {
		return cb(StyleParameter::create<Name>(a));
	}
	return false;
}

// `place-*` shorthand: first token -> A, optional second token -> B (else = A)
template <ParameterName A, ParameterName B>
static bool css_readPlace(const StringView &value, const StyleCallback &cb) {
	StringView first, second;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (first.empty()) {
			first = r;
		} else if (second.empty()) {
			second = r;
		}
	});
	if (first.empty()) {
		return false;
	}
	Align a, b;
	if (!css_readAlignValue(first, a)) {
		return false;
	}
	b = a;
	if (!second.empty() && !css_readAlignValue(second, b)) {
		return false;
	}
	return cb(StyleParameter::create<A>(a)) && cb(StyleParameter::create<B>(b));
}

// grid line / template values are stored verbatim (interned) - parsing the track-list
// grammar is deferred to the (future) grid layout consumer
template <ParameterName Name>
static bool css_readRawString(const StringView &value, const StyleCallback &cb,
		const StringCallback &strCb) {
	StringView v(value);
	v.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (v.empty()) {
		return false;
	}
	return cb(StyleParameter::create<Name>(strCb(v)));
}

static bool css_readGap(const StringView &value, Metric &out) {
	if (value.equals("normal")) {
		out.metric = Metric::Units::Auto;
		out.value = 0.0f;
		return true;
	}
	return parser::readStyleMetric(value, out);
}

static sprt::__malloc_unordered_map<StringView, StyleFunctionPtr> s_cssParameters{
	pair("font-weight",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("bold")) {
		return cb(StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Bold));
	} else if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Normal));
	} else {
		StringView tmp(value);
		if (auto val = tmp.readInteger(10).get(0)) {
			if (val > 0 && val <= 1'000) {
				return cb(StyleParameter::create<ParameterName::CssFontWeight>(FontWeight(val)));
			}
		}
	}
	return false;
}),
	pair("font-stretch",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::Normal));
	} else if (value.equals("ultra-condensed")) {
		return cb(
				StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::UltraCondensed));
	} else if (value.equals("extra-condensed")) {
		return cb(
				StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::ExtraCondensed));
	} else if (value.equals("condensed")) {
		return cb(StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::Condensed));
	} else if (value.equals("semi-condensed")) {
		return cb(
				StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::SemiCondensed));
	} else if (value.equals("semi-expanded")) {
		return cb(StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::SemiExpanded));
	} else if (value.equals("expanded")) {
		return cb(StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::Expanded));
	} else if (value.equals("extra-expanded")) {
		return cb(
				StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::ExtraExpanded));
	} else if (value.equals("ultra-expanded")) {
		return cb(
				StyleParameter::create<ParameterName::CssFontStretch>(FontStretch::UltraExpanded));
	} else {
		StringView tmp(value);
		if (auto val = tmp.readFloat().get(0)) {
			tmp.skipChars<StringView::WhiteSpace>();
			if (tmp.is('%') && val >= 50.0f && val <= 200.0f) {
				return cb(StyleParameter::create<ParameterName::CssFontStretch>(
						FontStretch(uint16_t(val * 2.0f))));
			}
		}
	}
	return false;
}),
	pair("font-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("italic")) {
		return cb(StyleParameter::create<ParameterName::CssFontStyle>(FontStyle::Italic));
	} else if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssFontStyle>(FontStyle::Normal));
	} else if (value.equals("oblique")) {
		return cb(StyleParameter::create<ParameterName::CssFontStyle>(FontStyle::Oblique));
	} else {
		StringView tmp(value);
		if (value.starts_with("oblique")) {
			tmp += "oblique"_len;
		}
		tmp.skipChars<StringView::WhiteSpace>();
		auto val = tmp.readFloat().get(nan());
		if (!sprt::isnan(val)) {
			tmp.skipChars<StringView::WhiteSpace>();
			if (tmp.is("deg") && val >= -90.0 && val <= 90.0) {
				return cb(StyleParameter::create<ParameterName::CssFontStyle>(
						FontStyle(uint16_t(val * (1 << 6)))));
			}
		}
	}
	return false;
}),
	pair("font-size",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("xx-small")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XXSmall));
	} else if (value.equals("x-small")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XSmall));
	} else if (value.equals("small")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::Small));
	} else if (value.equals("medium")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::Medium));
	} else if (value.equals("large")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::Large));
	} else if (value.equals("x-large")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XLarge));
	} else if (value.equals("xx-large")) {
		return cb(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XXLarge));
	} else if (value.equals("larger")) {
		return cb(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(1.15f, Metric::Em)));
	} else if (value.equals("x-larger")) {
		return cb(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(1.3f, Metric::Em)));
	} else if (value.equals("smaller")) {
		return cb(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(0.85f, Metric::Em)));
	} else if (value.equals("x-smaller")) {
		return cb(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(0.7f, Metric::Em)));
	} else {
		Metric fontSize;
		if (parser::readStyleMetric(value, fontSize)) {
			if (fontSize.metric == Metric::Units::Px) {
				return cb(StyleParameter::create<ParameterName::CssFontSize>(
						FontSize(fontSize.value)));
			} else if (fontSize.metric == Metric::Units::Em) {
				return cb(StyleParameter::create<ParameterName::CssFontSize>(
						FontSize(FontSize::Medium.get() * fontSize.value)));
			}
		}
	}
	return false;
}),
	pair("font-variant",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("small-caps")) {
		return cb(StyleParameter::create<ParameterName::CssFontVariant>(FontVariant::SmallCaps));
	} else if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssFontVariant>(FontVariant::Normal));
	}
	return false;
}),
	pair("text-decoration",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("underline")) {
		return cb(StyleParameter::create<ParameterName::CssTextDecoration>(
				TextDecoration::Underline));
	} else if (value.equals("line-through")) {
		return cb(StyleParameter::create<ParameterName::CssTextDecoration>(
				TextDecoration::LineThrough));
	} else if (value.equals("overline")) {
		return cb(
				StyleParameter::create<ParameterName::CssTextDecoration>(TextDecoration::Overline));
	} else if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssTextDecoration>(TextDecoration::None));
	}
	return false;
}),
	pair("text-transform",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("uppercase")) {
		return cb(
				StyleParameter::create<ParameterName::CssTextTransform>(TextTransform::Uppercase));
	} else if (value.equals("lowercase")) {
		return cb(
				StyleParameter::create<ParameterName::CssTextTransform>(TextTransform::Lowercase));
	} else if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssTextTransform>(TextTransform::None));
	}
	return false;
}),
	pair("text-align",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("left")) {
		return cb(StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Left));
	} else if (value.equals("right")) {
		return cb(StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Right));
	} else if (value.equals("center")) {
		return cb(StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Center));
	} else if (value.equals("justify")) {
		return cb(StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Justify));
	}
	return false;
}),
	pair("white-space",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::Normal));
	} else if (value.equals("nowrap")) {
		return cb(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::Nowrap));
	} else if (value.equals("pre")) {
		return cb(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::Pre));
	} else if (value.equals("pre-line")) {
		return cb(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::PreLine));
	} else if (value.equals("pre-wrap")) {
		return cb(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::PreWrap));
	}
	return false;
}),
	pair("hyphens",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::None));
	} else if (value.equals("manual")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::Manual));
	} else if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::Auto));
	}
	return false;
}),
	pair("-epub-hyphens",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::None));
	} else if (value.equals("manual")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::Manual));
	} else if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::Auto));
	}
	return false;
}),
	pair("display",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::None));
	} else if (value.equals("run-in")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::RunIn));
	} else if (value.equals("list-item")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::ListItem));
	} else if (value.equals("inline")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::Inline));
	} else if (value.equals("inline-block")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::InlineBlock));
	} else if (value.equals("block")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
	} else if (value.equals("flex")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::Flex));
	} else if (value.equals("inline-flex")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::InlineFlex));
	} else if (value.equals("grid")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::Grid));
	} else if (value.equals("inline-grid")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::InlineGrid));
	} else if (value.equals("table")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::Table));
	} else if (value.equals("table-row")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::TableRow));
	} else if (value.equals("table-cell")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::TableCell));
	} else if (value.equals("table-column")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::TableColumn));
	} else if (value.equals("table-caption")) {
		return cb(StyleParameter::create<ParameterName::CssDisplay>(Display::TableCaption));
	}
	return false;
}),
	pair("visibility",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("visible")) {
		return cb(StyleParameter::create<ParameterName::CssVisibility>(Visibility::Visible));
	} else if (value.equals("hidden")) {
		return cb(StyleParameter::create<ParameterName::CssVisibility>(Visibility::Hidden));
	} else if (value.equals("collapse")) {
		return cb(StyleParameter::create<ParameterName::CssVisibility>(Visibility::Collapse));
	}
	return false;
}),
	pair("list-style-type",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readListStyleType(value, cb);
}),
	pair("list-style-position",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("inside")) {
		return cb(StyleParameter::create<ParameterName::CssListStylePosition>(
				ListStylePosition::Inside));
	} else if (value.equals("outside")) {
		return cb(StyleParameter::create<ParameterName::CssListStylePosition>(
				ListStylePosition::Outside));
	}
	return false;
}),
	pair("list-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	bool ret = true;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (!css_readListStyleType(r, cb)) {
			if (r.equals("inside")) {
				cb(StyleParameter::create<ParameterName::CssListStylePosition>(
						ListStylePosition::Inside));
			} else if (r.equals("outside")) {
				cb(StyleParameter::create<ParameterName::CssListStylePosition>(
						ListStylePosition::Outside));
			}
			ret = false;
		}
	});
	return ret;
}),
	pair("x-list-style-offset",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssXListStyleOffset>(data));
	}
	return false;
}),
	pair("float",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssFloat>(Float::None));
	} else if (value.equals("left")) {
		return cb(StyleParameter::create<ParameterName::CssFloat>(Float::Left));
	} else if (value.equals("right")) {
		return cb(StyleParameter::create<ParameterName::CssFloat>(Float::Right));
	}
	return false;
}),
	pair("clear",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssClear>(Clear::None));
	} else if (value.equals("left")) {
		return cb(StyleParameter::create<ParameterName::CssClear>(Clear::Left));
	} else if (value.equals("right")) {
		return cb(StyleParameter::create<ParameterName::CssClear>(Clear::Right));
	} else if (value.equals("both")) {
		return cb(StyleParameter::create<ParameterName::CssClear>(Clear::Both));
	}
	return false;
}),
	pair("opacity",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	bool ret = false;
	StringView(value).readFloat().unwrap([&](float data) {
		ret = cb(StyleParameter::create<ParameterName::CssOpacity>(
				(uint8_t)(math::clamp(data, 0.0f, 1.0f) * 255.0f)));
	});
	return ret;
}),
	pair("color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Color4B color;
	if (readColor(value, color)) {
		cb(StyleParameter::create<ParameterName::CssColor>(Color3B(color.r, color.g, color.b)));
		if (color.a != 255) {
			cb(StyleParameter::create<ParameterName::CssOpacity>(color.a));
		}
		return true;
	}
	return false;
}),
	pair("text-indent",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssTextIndent>(data));
	}
	return false;
}),
	pair("line-height",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data, false, true)) {
		return cb(StyleParameter::create<ParameterName::CssLineHeight>(data));
	}
	return false;
}),
	pair("margin",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric top, right, bottom, left;
	if (parser::readStyleMargin(value, top, right, bottom, left)) {
		cb(StyleParameter::create<ParameterName::CssMarginTop>(top));
		cb(StyleParameter::create<ParameterName::CssMarginRight>(right));
		cb(StyleParameter::create<ParameterName::CssMarginBottom>(bottom));
		cb(StyleParameter::create<ParameterName::CssMarginLeft>(left));
		return true;
	}
	return false;
}),
	pair("margin-top",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMarginTop>(data));
	}
	return false;
}),
	pair("margin-right",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMarginRight>(data));
	}
	return false;
}),
	pair("margin-bottom",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMarginBottom>(data));
	}
	return false;
}),
	pair("margin-left",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMarginLeft>(data));
	}
	return false;
}),
	pair("width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssWidth>(data));
	}
	return false;
}),
	pair("height",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssHeight>(data));
	}
	return false;
}),
	pair("min-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMinWidth>(data));
	}
	return false;
}),
	pair("min-height",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMinHeight>(data));
	}
	return false;
}),
	pair("max-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMaxWidth>(data));
	}
	return false;
}),
	pair("max-height",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssMaxHeight>(data));
	}
	return false;
}),
	pair("padding",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric top, right, bottom, left;
	if (parser::readStyleMargin(value, top, right, bottom, left)) {
		cb(StyleParameter::create<ParameterName::CssPaddingTop>(top));
		cb(StyleParameter::create<ParameterName::CssPaddingRight>(right));
		cb(StyleParameter::create<ParameterName::CssPaddingBottom>(bottom));
		cb(StyleParameter::create<ParameterName::CssPaddingLeft>(left));
		return true;
	}
	return false;
}),
	pair("padding-top",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssPaddingTop>(data));
	}
	return false;
}),
	pair("padding-right",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssPaddingRight>(data));
	}
	return false;
}),
	pair("padding-bottom",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssPaddingBottom>(data));
	}
	return false;
}),
	pair("padding-left",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssPaddingLeft>(data));
	}
	return false;
}),
	pair("position",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("static")) {
		return cb(StyleParameter::create<ParameterName::CssPosition>(Position::Static));
	} else if (value.equals("relative")) {
		return cb(StyleParameter::create<ParameterName::CssPosition>(Position::Relative));
	} else if (value.equals("absolute")) {
		return cb(StyleParameter::create<ParameterName::CssPosition>(Position::Absolute));
	} else if (value.equals("fixed")) {
		return cb(StyleParameter::create<ParameterName::CssPosition>(Position::Fixed));
	} else if (value.equals("sticky")) {
		return cb(StyleParameter::create<ParameterName::CssPosition>(Position::Sticky));
	}
	return false;
}),
	pair("top",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssTop>(data));
	}
	return false;
}),
	pair("right",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssRight>(data));
	}
	return false;
}),
	pair("bottom",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssBottom>(data));
	}
	return false;
}),
	pair("left",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssLeft>(data));
	}
	return false;
}), 
	pair("-xl-anchor-point",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	// `-xl-anchor-point: <x> [<y>]` - normalized anchor (0,0 bottom-left .. 1,1 top-right);
	// a single value applies to both axes
	float vals[2] = {nan(), nan()};
	int count = 0;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (count < 2) {
			StringView tmp(r);
			vals[count] = tmp.readFloat().get(nan());
		}
		++count;
	});
	if (sprt::isnan(vals[0])) {
		return false;
	}
	if (sprt::isnan(vals[1])) {
		vals[1] = vals[0];
	}
	return cb(StyleParameter::create<ParameterName::CssXlAnchorPointX>(vals[0]))
			&& cb(StyleParameter::create<ParameterName::CssXlAnchorPointY>(vals[1]));
}),
	pair("-xl-position",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	// `-xl-position: <x> [<y>]` - direct node position; relative (percent) values resolve
	// against the parent size at apply time. A single value applies to both axes
	Metric vals[2];
	int count = 0;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (count < 2) {
			if (!parser::readStyleMetric(r, vals[count])) {
				err = true;
				return;
			}
			++count;
		}
	});
	if (err || count == 0) {
		return false;
	}
	if (count == 1) {
		vals[1] = vals[0];
	}
	return cb(StyleParameter::create<ParameterName::CssXlPositionX>(vals[0]))
			&& cb(StyleParameter::create<ParameterName::CssXlPositionY>(vals[1]));
}),
	pair("-xl-z-order",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	// `-xl-z-order: <int>` - the node's ZOrder. Nodes are placed in ZOrder sequence, so this sets
	// the logical placement order of flex/grid items (applied before the reorder phase)
	StringView tmp(value);
	auto v = tmp.readInteger(10);
	if (v) {
		return cb(StyleParameter::create<ParameterName::CssXlZOrder>(int32_t(v.get())));
	}
	return false;
}),

	/* flexbox & grid */

	pair("flex-direction",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("row")) {
		return cb(StyleParameter::create<ParameterName::CssFlexDirection>(FlexDirection::Row));
	} else if (value.equals("row-reverse")) {
		return cb(StyleParameter::create<ParameterName::CssFlexDirection>(
				FlexDirection::RowReverse));
	} else if (value.equals("column")) {
		return cb(StyleParameter::create<ParameterName::CssFlexDirection>(FlexDirection::Column));
	} else if (value.equals("column-reverse")) {
		return cb(StyleParameter::create<ParameterName::CssFlexDirection>(
				FlexDirection::ColumnReverse));
	}
	return false;
}),
	pair("flex-wrap",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("nowrap")) {
		return cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::NoWrap));
	} else if (value.equals("wrap")) {
		return cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::Wrap));
	} else if (value.equals("wrap-reverse")) {
		return cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::WrapReverse));
	}
	return false;
}),
	pair("flex-flow",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	bool ret = false;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (r.equals("row")) {
			cb(StyleParameter::create<ParameterName::CssFlexDirection>(FlexDirection::Row));
		} else if (r.equals("row-reverse")) {
			cb(StyleParameter::create<ParameterName::CssFlexDirection>(FlexDirection::RowReverse));
		} else if (r.equals("column")) {
			cb(StyleParameter::create<ParameterName::CssFlexDirection>(FlexDirection::Column));
		} else if (r.equals("column-reverse")) {
			cb(StyleParameter::create<ParameterName::CssFlexDirection>(
					FlexDirection::ColumnReverse));
		} else if (r.equals("nowrap")) {
			cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::NoWrap));
		} else if (r.equals("wrap")) {
			cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::Wrap));
		} else if (r.equals("wrap-reverse")) {
			cb(StyleParameter::create<ParameterName::CssFlexWrap>(FlexWrap::WrapReverse));
		} else {
			err = true;
			return;
		}
		ret = true;
	});
	return ret && !err;
}),
	pair("order",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView tmp(value);
	auto v = tmp.readInteger(10);
	if (v) {
		return cb(StyleParameter::create<ParameterName::CssOrder>(int32_t(v.get())));
	}
	return false;
}),
	pair("flex-grow",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	float f;
	if (StringView(value).readFloat().grab(f) && f >= 0.0f) {
		return cb(StyleParameter::create<ParameterName::CssFlexGrow>(f));
	}
	return false;
}),
	pair("flex-shrink",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	float f;
	if (StringView(value).readFloat().grab(f) && f >= 0.0f) {
		return cb(StyleParameter::create<ParameterName::CssFlexShrink>(f));
	}
	return false;
}),
	pair("flex-basis",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (value.equals("content")) {
		data.metric = Metric::Units::Auto;
		data.value = 0.0f;
		return cb(StyleParameter::create<ParameterName::CssFlexBasis>(data));
	}
	if (parser::readStyleMetric(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssFlexBasis>(data));
	}
	return false;
}),
	pair("flex",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("none")) {
		// none = 0 0 auto
		return cb(StyleParameter::create<ParameterName::CssFlexGrow>(0.0f))
				&& cb(StyleParameter::create<ParameterName::CssFlexShrink>(0.0f))
				&& cb(StyleParameter::create<ParameterName::CssFlexBasis>(
						Metric(0.0f, Metric::Units::Auto)));
	}
	if (value.equals("initial")) {
		// initial = 0 1 auto
		return cb(StyleParameter::create<ParameterName::CssFlexGrow>(0.0f))
				&& cb(StyleParameter::create<ParameterName::CssFlexShrink>(1.0f))
				&& cb(StyleParameter::create<ParameterName::CssFlexBasis>(
						Metric(0.0f, Metric::Units::Auto)));
	}

	// [ <grow> <shrink>? || <basis> ]; pure numbers feed grow then shrink,
	// a value with a unit (or auto/content) is the basis
	float nums[2] = {nan(), nan()};
	int nc = 0;
	Metric basis;
	bool hasBasis = false;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		StringView probe(r);
		float f;
		if (probe.readFloat().grab(f) && probe.empty()) {
			if (nc < 2) {
				nums[nc++] = f;
			} else {
				err = true;
			}
		} else if (r.equals("auto") || r.equals("content")) {
			basis.metric = Metric::Units::Auto;
			basis.value = 0.0f;
			hasBasis = true;
		} else if (parser::readStyleMetric(r, basis)) {
			hasBasis = true;
		} else {
			err = true;
		}
	});
	if (err || (nc == 0 && !hasBasis)) {
		return false;
	}
	const float grow = (nc >= 1) ? nums[0] : 1.0f;
	const float shrink = (nc >= 2) ? nums[1] : 1.0f;
	// a bare <grow> defaults basis to 0; a bare <basis> defaults grow/shrink to 1
	const Metric fb = hasBasis ? basis : Metric(0.0f, Metric::Units::Px);
	return cb(StyleParameter::create<ParameterName::CssFlexGrow>(grow))
			&& cb(StyleParameter::create<ParameterName::CssFlexShrink>(shrink))
			&& cb(StyleParameter::create<ParameterName::CssFlexBasis>(fb));
}),
	pair("justify-content",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssJustifyContent>(value, cb);
}),
	pair("align-content",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssAlignContent>(value, cb);
}),
	pair("justify-items",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssJustifyItems>(value, cb);
}),
	pair("align-items",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssAlignItems>(value, cb);
}),
	pair("justify-self",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssJustifySelf>(value, cb);
}),
	pair("align-self",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readAlign<ParameterName::CssAlignSelf>(value, cb);
}),
	pair("place-content",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readPlace<ParameterName::CssAlignContent, ParameterName::CssJustifyContent>(value,
			cb);
}),
	pair("place-items",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readPlace<ParameterName::CssAlignItems, ParameterName::CssJustifyItems>(value, cb);
}),
	pair("place-self",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	return css_readPlace<ParameterName::CssAlignSelf, ParameterName::CssJustifySelf>(value, cb);
}),
	pair("row-gap",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (css_readGap(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssRowGap>(data));
	}
	return false;
}),
	pair("column-gap",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric data;
	if (css_readGap(value, data)) {
		return cb(StyleParameter::create<ParameterName::CssColumnGap>(data));
	}
	return false;
}),
	pair("gap",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric vals[2];
	int count = 0;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (count < 2) {
			if (!css_readGap(r, vals[count])) {
				err = true;
				return;
			}
			++count;
		}
	});
	if (err || count == 0) {
		return false;
	}
	if (count == 1) {
		vals[1] = vals[0];
	}
	return cb(StyleParameter::create<ParameterName::CssRowGap>(vals[0]))
			&& cb(StyleParameter::create<ParameterName::CssColumnGap>(vals[1]));
}),
	pair("grid-auto-flow",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	bool dense = false;
	bool column = false;
	bool axisSet = false;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (r.equals("row")) {
			column = false;
			axisSet = true;
		} else if (r.equals("column")) {
			column = true;
			axisSet = true;
		} else if (r.equals("dense")) {
			dense = true;
		} else {
			err = true;
		}
	});
	if (err || (!axisSet && !dense)) {
		return false;
	}
	GridAutoFlow flow = column ? (dense ? GridAutoFlow::ColumnDense : GridAutoFlow::Column)
							   : (dense ? GridAutoFlow::RowDense : GridAutoFlow::Row);
	return cb(StyleParameter::create<ParameterName::CssGridAutoFlow>(flow));
}),
	pair("grid-template-columns",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridTemplateColumns>(value, cb, strCb);
}),
	pair("grid-template-rows",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridTemplateRows>(value, cb, strCb);
}),
	pair("grid-template-areas",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridTemplateAreas>(value, cb, strCb);
}),
	pair("grid-auto-columns",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridAutoColumns>(value, cb, strCb);
}),
	pair("grid-auto-rows",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridAutoRows>(value, cb, strCb);
}),
	pair("grid-column-start",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridColumnStart>(value, cb, strCb);
}),
	pair("grid-column-end",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridColumnEnd>(value, cb, strCb);
}),
	pair("grid-row-start",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridRowStart>(value, cb, strCb);
}),
	pair("grid-row-end",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readRawString<ParameterName::CssGridRowEnd>(value, cb, strCb);
}),
	pair("grid-column",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	// grid-column: <start> [ / <end> ]
	StringView start, end;
	bool second = false;
	value.split<StringView::Chars<'/'>>([&](const StringView &r) {
		if (!second) {
			start = r;
			second = true;
		} else {
			end = r;
		}
	});
	start.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	end.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	bool ret = false;
	if (!start.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridColumnStart>(strCb(start)));
	}
	if (!end.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridColumnEnd>(strCb(end))) || ret;
	}
	return ret;
}),
	pair("grid-row",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	// grid-row: <start> [ / <end> ]
	StringView start, end;
	bool second = false;
	value.split<StringView::Chars<'/'>>([&](const StringView &r) {
		if (!second) {
			start = r;
			second = true;
		} else {
			end = r;
		}
	});
	start.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	end.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	bool ret = false;
	if (!start.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridRowStart>(strCb(start)));
	}
	if (!end.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridRowEnd>(strCb(end))) || ret;
	}
	return ret;
}),
	pair("grid-area",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	// grid-area: <row-start> [ / <col-start> [ / <row-end> [ / <col-end> ] ] ]
	StringView parts[4];
	int count = 0;
	value.split<StringView::Chars<'/'>>([&](const StringView &r) {
		if (count < 4) {
			StringView t(r);
			t.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			parts[count++] = t;
		}
	});
	if (count == 0 || parts[0].empty()) {
		return false;
	}
	auto emit = [&](int i, StringView s) -> bool {
		auto id = strCb(s);
		switch (i) {
		case 0: return cb(StyleParameter::create<ParameterName::CssGridRowStart>(id));
		case 1: return cb(StyleParameter::create<ParameterName::CssGridColumnStart>(id));
		case 2: return cb(StyleParameter::create<ParameterName::CssGridRowEnd>(id));
		case 3: return cb(StyleParameter::create<ParameterName::CssGridColumnEnd>(id));
		default: return false;
		}
	};
	bool ret = false;
	for (int i = 0; i < count; ++i) {
		if (!parts[i].empty()) {
			ret = emit(i, parts[i]) || ret;
		}
	}
	return ret;
}),
	pair("grid-template",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	// only the simple `<rows> / <columns>` form (no template-areas strings) is expanded
	for (size_t i = 0; i < value.size(); ++i) {
		if (value.data()[i] == '"') {
			return false;
		}
	}
	StringView rows, columns;
	bool second = false;
	value.split<StringView::Chars<'/'>>([&](const StringView &r) {
		if (!second) {
			rows = r;
			second = true;
		} else {
			columns = r;
		}
	});
	if (!second) {
		return false;
	}
	rows.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	columns.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	bool ret = false;
	if (!rows.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridTemplateRows>(strCb(rows)));
	}
	if (!columns.empty()) {
		ret = cb(StyleParameter::create<ParameterName::CssGridTemplateColumns>(strCb(columns)))
				|| ret;
	}
	return ret;
}),
	pair("font-family",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("default") || value.equals("serif")) {
		return cb(StyleParameter::create<ParameterName::CssFontFamily>(StringIdNone));
	} else {
		auto str = StyleContainer::resolveCssString(value);
		if (!str.empty()) {
			return cb(StyleParameter::create<ParameterName::CssFontFamily>(strCb(str)));
		}
	}
	return false;
}),
	pair("background-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("transparent")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundColor>(Color4B(0, 0, 0, 0)));
	} else {
		Color4B color;
		if (readColor(value, color)) {
			return cb(StyleParameter::create<ParameterName::CssBackgroundColor>(color));
		}
	}
	return false;
}),
	pair("background-image",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundImage>(StringIdNone));
	} else {
		StringView tmp(value);
		tmp.trimChars<StringView::WhiteSpace>();
		if (tmp.starts_with("url")) {
			tmp += "url"_len;
		}
		tmp = StyleContainer::resolveCssString(tmp);

		if (!tmp.empty()) {
			return cb(StyleParameter::create<ParameterName::CssBackgroundImage>(strCb(tmp)));
		}
	}
	return false;
}),
	pair("background-position",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	StringView first, second;
	Metric x, y;
	bool validX = false, validY = false, swapValues = false;

	value.split<StringView::WhiteSpace>([&](const StringView r) {
		if (first.empty()) {
			first = r;
		} else {
			second = r;
		}
	});

	if (!first.empty() && !second.empty()) {
		bool parseError = false;
		bool firstWasCenter = false;
		if (first.equals("center")) {
			x.value = 0.5f;
			x.metric = Metric::Units::Percent;
			validX = true;
			firstWasCenter = true;
		} else if (first.equals("left")) {
			x.value = 0.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
		} else if (first.equals("right")) {
			x.value = 1.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
		} else if (first.equals("top")) {
			x.value = 0.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
			swapValues = true;
		} else if (first.equals("bottom")) {
			x.value = 1.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
			swapValues = true;
		}

		if (second.equals("center")) {
			y.value = 0.5f;
			y.metric = Metric::Units::Percent;
			validY = true;
		} else if (second.equals("left")) {
			if (swapValues || firstWasCenter) {
				y.value = 0.0f;
				y.metric = Metric::Units::Percent;
				validY = true;
				swapValues = true;
			} else {
				parseError = true;
			}
		} else if (second.equals("right")) {
			if (swapValues || firstWasCenter) {
				y.value = 1.0f;
				y.metric = Metric::Units::Percent;
				validY = true;
				swapValues = true;
			} else {
				parseError = true;
			}
		} else if (second.equals("top")) {
			if (!swapValues) {
				y.value = 0.0f;
				y.metric = Metric::Units::Percent;
				validY = true;
				swapValues = true;
			} else {
				parseError = true;
			}
		} else if (second.equals("bottom")) {
			if (!swapValues) {
				y.value = 1.0f;
				y.metric = Metric::Units::Percent;
				validY = true;
				swapValues = true;
			} else {
				parseError = true;
			}
		}

		if (!parseError && !validX) {
			if (parser::readStyleMetric(first, x)) {
				validX = true;
			}
		}

		if (!parseError && !validY) {
			if (parser::readStyleMetric(second, y)) {
				validY = true;
			}
		}
	} else {
		if (value.equals("center")) {
			x.value = 0.5f;
			x.metric = Metric::Units::Percent;
			validX = true;
			y.value = 0.5f;
			y.metric = Metric::Units::Percent;
			validY = true;
		} else if (value.equals("top")) {
			x.value = 0.5f;
			x.metric = Metric::Units::Percent;
			validX = true;
			y.value = 0.0f;
			y.metric = Metric::Units::Percent;
			validY = true;
		} else if (value.equals("right")) {
			x.value = 1.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
			y.value = 0.5f;
			y.metric = Metric::Units::Percent;
			validY = true;
		} else if (value.equals("bottom")) {
			x.value = 0.5f;
			x.metric = Metric::Units::Percent;
			validX = true;
			y.value = 1.0f;
			y.metric = Metric::Units::Percent;
			validY = true;
		} else if (value.equals("left")) {
			x.value = 0.0f;
			x.metric = Metric::Units::Percent;
			validX = true;
			y.value = 0.5f;
			y.metric = Metric::Units::Percent;
			validY = true;
		}
	}

	if (validX && validY) {
		if (!swapValues) {
			return cb(StyleParameter::create<ParameterName::CssBackgroundPositionX>(x))
					&& cb(StyleParameter::create<ParameterName::CssBackgroundPositionY>(y));
		} else {
			return cb(StyleParameter::create<ParameterName::CssBackgroundPositionX>(y))
					&& cb(StyleParameter::create<ParameterName::CssBackgroundPositionY>(x));
		}
	}
	return false;
}),
	pair("background-repeat",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("no-repeat")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundRepeat>(
				BackgroundRepeat::NoRepeat));
	} else if (value.equals("repeat")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundRepeat>(
				BackgroundRepeat::Repeat));
	} else if (value.equals("repeat-x")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundRepeat>(
				BackgroundRepeat::RepeatX));
	} else if (value.equals("repeat-y")) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundRepeat>(
				BackgroundRepeat::RepeatY));
	}
	return false;
}),
	pair("background-size",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	StringView first, second;
	Metric width, height;
	bool validWidth = false, validHeight = false;

	value.split<StringView::WhiteSpace>([&](const StringView r) {
		if (first.empty()) {
			first = r;
		} else {
			second = r;
		}
	});

	if (value.equals("contain")) {
		width.metric = Metric::Units::Contain;
		validWidth = true;
		height.metric = Metric::Units::Contain;
		validHeight = true;
	} else if (value.equals("cover")) {
		width.metric = Metric::Units::Cover;
		validWidth = true;
		height.metric = Metric::Units::Cover;
		validHeight = true;
	} else if (!first.empty() && !second.empty()) {
		if (first.equals("contain")) {
			width.metric = Metric::Units::Contain;
			validWidth = true;
		} else if (first.equals("cover")) {
			width.metric = Metric::Units::Cover;
			validWidth = true;
		} else if (parser::readStyleMetric(first, width)) {
			validWidth = true;
		}

		if (second.equals("contain")) {
			height.metric = Metric::Units::Contain;
			validWidth = true;
		} else if (second.equals("cover")) {
			height.metric = Metric::Units::Cover;
			validWidth = true;
		} else if (parser::readStyleMetric(second, height)) {
			validHeight = true;
		}
	} else if (parser::readStyleMetric(value, width)) {
		height.metric = Metric::Units::Auto;
		validWidth = true;
		validHeight = true;
	}

	if (validWidth && validHeight) {
		return cb(StyleParameter::create<ParameterName::CssBackgroundSizeWidth>(width))
				&& cb(StyleParameter::create<ParameterName::CssBackgroundSizeHeight>(height));
	}
	return false;
}),
	pair("vertical-align",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("baseline")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Baseline));
	} else if (value.equals("sub")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Sub));
	} else if (value.equals("super")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Super));
	} else if (value.equals("middle")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Middle));
	} else if (value.equals("top")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Top));
	} else if (value.equals("bottom")) {
		return cb(StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Bottom));
	}
	return false;
}),
	pair("outline",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorder<ParameterName::CssOutlineStyle, ParameterName::CssOutlineColor,
			ParameterName::CssOutlineWidth>(value, cb);
}),
	pair("outline-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderStyle<ParameterName::CssOutlineStyle>(value, cb);
}),
	pair("outline-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderColor<ParameterName::CssOutlineColor>(value, cb);
}),
	pair("outline-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderWidth<ParameterName::CssOutlineWidth>(value, cb);
}),
	pair("border-radius",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	// CSS `border-radius` shorthand: 1-4 length values mapped to the four corners. The elliptical
	// "horizontal / vertical" form is not supported - only the horizontal radii (before any '/')
	// are read. Corner distribution follows the CSS spec:
	//   1 value : all four corners           2 values: TL=BR, TR=BL
	//   3 values: TL, TR=BL, BR              4 values: TL, TR, BR, BL   (CSS corner order)
	StringView horiz = value;
	horiz = horiz.readUntil<StringView::Chars<'/'>>(); // drop the elliptical (vertical) part
	Metric vals[4];
	int count = 0;
	bool err = false;
	horiz.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (count < 4) {
			if (!parser::readStyleMetric(r, vals[count])) {
				err = true;
				return;
			}
			++count;
		}
	});
	if (err || count == 0) {
		return false;
	}
	Metric tl, tr, br, bl;
	switch (count) {
	case 1: tl = tr = br = bl = vals[0]; break;
	case 2:
		tl = br = vals[0];
		tr = bl = vals[1];
		break;
	case 3:
		tl = vals[0];
		tr = bl = vals[1];
		br = vals[2];
		break;
	default:
		tl = vals[0];
		tr = vals[1];
		br = vals[2];
		bl = vals[3];
		break;
	}
	// CssBorderRadius (= the first value) is kept for consumers that read the uniform shorthand
	return cb(StyleParameter::create<ParameterName::CssBorderRadius>(vals[0]))
			&& cb(StyleParameter::create<ParameterName::CssBorderTopLeftRadius>(tl))
			&& cb(StyleParameter::create<ParameterName::CssBorderTopRightRadius>(tr))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomRightRadius>(br))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomLeftRadius>(bl));
}),
	pair("border-top-left-radius",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView horiz = value; // drop the elliptical (vertical) part after '/'
	Metric data;
	if (parser::readStyleMetric(horiz.readUntil<StringView::Chars<'/'>>(), data)) {
		return cb(StyleParameter::create<ParameterName::CssBorderTopLeftRadius>(data));
	}
	return false;
}),
	pair("border-top-right-radius",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView horiz = value;
	Metric data;
	if (parser::readStyleMetric(horiz.readUntil<StringView::Chars<'/'>>(), data)) {
		return cb(StyleParameter::create<ParameterName::CssBorderTopRightRadius>(data));
	}
	return false;
}),
	pair("border-bottom-right-radius",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView horiz = value;
	Metric data;
	if (parser::readStyleMetric(horiz.readUntil<StringView::Chars<'/'>>(), data)) {
		return cb(StyleParameter::create<ParameterName::CssBorderBottomRightRadius>(data));
	}
	return false;
}),
	pair("border-bottom-left-radius",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView horiz = value;
	Metric data;
	if (parser::readStyleMetric(horiz.readUntil<StringView::Chars<'/'>>(), data)) {
		return cb(StyleParameter::create<ParameterName::CssBorderBottomLeftRadius>(data));
	}
	return false;
}),
	pair("border-top",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorder<ParameterName::CssBorderTopStyle, ParameterName::CssBorderTopColor,
			ParameterName::CssBorderTopWidth>(value, cb);
}),
	pair("border-top-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderStyle<ParameterName::CssBorderTopStyle>(value, cb);
}),
	pair("border-top-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderColor<ParameterName::CssBorderTopColor>(value, cb);
}),
	pair("border-top-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderWidth<ParameterName::CssBorderTopWidth>(value, cb);
}),
	pair("border-right",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorder<ParameterName::CssBorderRightStyle, ParameterName::CssBorderRightColor,
			ParameterName::CssBorderRightWidth>(value, cb);
}),
	pair("border-right-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderStyle<ParameterName::CssBorderRightStyle>(value, cb);
}),
	pair("border-right-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderColor<ParameterName::CssBorderRightColor>(value, cb);
}),
	pair("border-right-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderWidth<ParameterName::CssBorderRightWidth>(value, cb);
}),
	pair("border-bottom",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorder<ParameterName::CssBorderBottomStyle, ParameterName::CssBorderBottomColor,
			ParameterName::CssBorderBottomWidth>(value, cb);
}),
	pair("border-bottom-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderStyle<ParameterName::CssBorderBottomStyle>(value, cb);
}),
	pair("border-bottom-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderColor<ParameterName::CssBorderBottomColor>(value, cb);
}),
	pair("border-bottom-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderWidth<ParameterName::CssBorderBottomWidth>(value, cb);
}),
	pair("border-left",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorder<ParameterName::CssBorderLeftStyle, ParameterName::CssBorderLeftColor,
			ParameterName::CssBorderLeftWidth>(value, cb);
}),
	pair("border-left-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderStyle<ParameterName::CssBorderLeftStyle>(value, cb);
}),
	pair("border-left-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderColor<ParameterName::CssBorderLeftColor>(value, cb);
}),
	pair("border-left-width",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	return css_readBorderWidth<ParameterName::CssBorderLeftWidth>(value, cb);
}),
	pair("border-style",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.empty()) {
		return false;
	}
	BorderStyle top, right, bottom, left;
	css_readQuadValue(value, top, right, bottom, left, [&](const StringView &v) -> BorderStyle {
		if (v.equals("solid")) {
			return BorderStyle::Solid;
		} else if (v.equals("dotted")) {
			return BorderStyle::Dotted;
		} else if (v.equals("dashed")) {
			return BorderStyle::Dashed;
		}
		return BorderStyle::None;
	});
	return cb(StyleParameter::create<ParameterName::CssBorderTopStyle>(top))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightStyle>(right))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomStyle>(bottom))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftStyle>(left));
}),
	pair("border-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.empty()) {
		return false;
	}
	Color4B top, right, bottom, left;
	css_readQuadValue(value, top, right, bottom, left, [&](const StringView &v) -> Color4B {
		if (v.equals("transparent")) {
			return Color4B(0, 0, 0, 0);
		} else {
			Color4B color(0, 0, 0, 0);
			readColor(v, color);
			return color;
		}
	});
	return cb(StyleParameter::create<ParameterName::CssBorderTopColor>(top))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightColor>(right))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomColor>(bottom))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftColor>(left));
}),
	pair("border-color",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.empty()) {
		return false;
	}
	Metric top, right, bottom, left;
	css_readQuadValue(value, top, right, bottom, left, [&](const StringView &v) -> Metric {
		if (v.equals("thin")) {
			return Metric(2.0f, Metric::Units::Px);
		} else if (v.equals("medium")) {
			return Metric(4.0f, Metric::Units::Px);
		} else if (v.equals("thick")) {
			return Metric(6.0f, Metric::Units::Px);
		}

		Metric m(0.0f, Metric::Units::Px);
		parser::readStyleMetric(v, m);
		return m;
	});
	return cb(StyleParameter::create<ParameterName::CssBorderTopWidth>(top))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightWidth>(right))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomWidth>(bottom))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftWidth>(left));
}),
	pair("border",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.empty()) {
		return false;
	}
	BorderStyle style = BorderStyle::None;
	Metric width(0.0f, Metric::Units::Px);
	Color4B color(0, 0, 0, 0);
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (r.equals("solid")) {
			style = BorderStyle::Solid;
		} else if (r.equals("dotted")) {
			style = BorderStyle::Dotted;
		} else if (r.equals("dashed")) {
			style = BorderStyle::Dashed;
		} else if (r.equals("none")) {
			style = BorderStyle::None;
		} else if (r.equals("transparent")) {
			color = Color4B(0, 0, 0, 0);
		} else if (r.equals("thin")) {
			width = Metric(2.0f, Metric::Units::Px);
		} else if (r.equals("medium")) {
			width = Metric(4.0f, Metric::Units::Px);
		} else if (r.equals("thick")) {
			width = Metric(6.0f, Metric::Units::Px);
		} else if (!readColor(r, color)) {
			parser::readStyleMetric(r, width);
		}
	});
	return cb(StyleParameter::create<ParameterName::CssBorderTopStyle>(style))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightStyle>(style))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomStyle>(style))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftStyle>(style))
			&& cb(StyleParameter::create<ParameterName::CssBorderTopColor>(color))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightColor>(color))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomColor>(color))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftColor>(color))
			&& cb(StyleParameter::create<ParameterName::CssBorderTopWidth>(width))
			&& cb(StyleParameter::create<ParameterName::CssBorderRightWidth>(width))
			&& cb(StyleParameter::create<ParameterName::CssBorderBottomWidth>(width))
			&& cb(StyleParameter::create<ParameterName::CssBorderLeftWidth>(width));
}),
	pair("border-collapse",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("collapse")) {
		return cb(
				StyleParameter::create<ParameterName::CssBorderCollapse>(BorderCollapse::Collapse));
	} else if (value.equals("separate")) {
		return cb(
				StyleParameter::create<ParameterName::CssBorderCollapse>(BorderCollapse::Separate));
	}
	return false;
}),
	pair("caption-side",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("top")) {
		return cb(StyleParameter::create<ParameterName::CssCaptionSide>(CaptionSide::Top));
	} else if (value.equals("bottom")) {
		return cb(StyleParameter::create<ParameterName::CssCaptionSide>(CaptionSide::Bottom));
	}
	return false;
}),
	pair("table-layout",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssTableLayout>(TableLayout::Auto));
	} else if (value.equals("fixed")) {
		return cb(StyleParameter::create<ParameterName::CssTableLayout>(TableLayout::Fixed));
	}
	return false;
}),
	// `border-spacing: <h> [<v>]` - one value sets both axes, as in CSS
	pair("border-spacing",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	Metric vals[2];
	int count = 0;
	bool err = false;
	value.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&](const StringView &r) {
		if (count < 2) {
			if (!parser::readStyleMetric(r, vals[count])) {
				err = true;
				return;
			}
			++count;
		}
	});
	if (err || count == 0) {
		return false;
	}
	if (count == 1) {
		vals[1] = vals[0];
	}
	return cb(StyleParameter::create<ParameterName::CssBorderSpacingHorizontal>(vals[0]))
			&& cb(StyleParameter::create<ParameterName::CssBorderSpacingVertical>(vals[1]));
}),
	pair("-xl-column-span",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView tmp(value);
	auto val = tmp.readInteger(10);
	if (!val.valid() || val.get() < 1) {
		return false;
	}
	return cb(StyleParameter::create<ParameterName::CssXlColumnSpan>(uint32_t(val.get())));
}),
	pair("-xl-row-span",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &) {
	StringView tmp(value);
	auto val = tmp.readInteger(10);
	if (!val.valid() || val.get() < 1) {
		return false;
	}
	return cb(StyleParameter::create<ParameterName::CssXlRowSpan>(uint32_t(val.get())));
}),
	pair("page-break-after",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("always")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakAfter>(PageBreak::Always));
	} else if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakAfter>(PageBreak::Auto));
	} else if (value.equals("avoid")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakAfter>(PageBreak::Avoid));
	} else if (value.equals("left")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakAfter>(PageBreak::Left));
	} else if (value.equals("right")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakAfter>(PageBreak::Right));
	}
	return false;
}),
	pair("page-break-before",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("always")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakBefore>(PageBreak::Always));
	} else if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakBefore>(PageBreak::Auto));
	} else if (value.equals("avoid")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakBefore>(PageBreak::Avoid));
	} else if (value.equals("left")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakBefore>(PageBreak::Left));
	} else if (value.equals("right")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakBefore>(PageBreak::Right));
	}
	return false;
}),
	pair("page-break-inside",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("auto")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakInside>(PageBreak::Auto));
	} else if (value.equals("avoid")) {
		return cb(StyleParameter::create<ParameterName::CssPageBreakInside>(PageBreak::Avoid));
	}
	return false;
}),
	pair("orphans",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	auto v = StringView(value).readInteger(10);
	if (v) {
		return cb(StyleParameter::create<ParameterName::CssOrphans>(uint32_t(v.get())));
	}
	return false;
}),
	pair("widows",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	auto v = StringView(value).readInteger(10);
	if (v) {
		return cb(StyleParameter::create<ParameterName::CssWidows>(uint32_t(v.get())));
	}
	return false;
}),
	pair("orientation",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("landscape")) {
		return cb(
				StyleParameter::create<ParameterName::CssMediaOrientation>(Orientation::Landscape));
	} else if (value.equals("portrait")) {
		return cb(
				StyleParameter::create<ParameterName::CssMediaOrientation>(Orientation::Portrait));
	}
	return false;
}),
	pair("pointer",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssMediaPointer>(Pointer::None));
	} else if (value.equals("fine")) {
		return cb(StyleParameter::create<ParameterName::CssMediaPointer>(Pointer::Fine));
	} else if (value.equals("coarse")) {
		return cb(StyleParameter::create<ParameterName::CssMediaPointer>(Pointer::Coarse));
	}
	return false;
}),
	pair("platform",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	// custom media feature: `@media (platform: linux|windows|macos|ios|android|web)`
	Platform p = Platform::Unknown;
	if (value.equals("macos")) {
		p = Platform::MacOS;
	} else if (value.equals("ios")) {
		p = Platform::Ios;
	} else if (value.equals("windows")) {
		p = Platform::Windows;
	} else if (value.equals("android")) {
		p = Platform::Android;
	} else if (value.equals("linux")) {
		p = Platform::Linux;
	} else if (value.equals("web")) {
		p = Platform::Web;
	} else {
		return false;
	}
	return cb(StyleParameter::create<ParameterName::CssMediaPlatform>(p));
}),
	pair("hover",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssMediaHover>(Hover::None));
	} else if (value.equals("hover")) {
		return cb(StyleParameter::create<ParameterName::CssMediaHover>(Hover::Hover));
	} else if (value.equals("on-demand")) {
		return cb(StyleParameter::create<ParameterName::CssMediaHover>(Hover::OnDemand));
	}
	return false;
}),
	pair("light-level",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("dim")) {
		return cb(StyleParameter::create<ParameterName::CssMediaLightLevel>(LightLevel::Dim));
	} else if (value.equals("normal")) {
		return cb(StyleParameter::create<ParameterName::CssMediaLightLevel>(LightLevel::Normal));
	} else if (value.equals("washed")) {
		return cb(StyleParameter::create<ParameterName::CssMediaLightLevel>(LightLevel::Washed));
	}
	return false;
}),
	pair("scripting",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	if (value.equals("none")) {
		return cb(StyleParameter::create<ParameterName::CssMediaScripting>(Scripting::None));
	} else if (value.equals("initial-only")) {
		return cb(StyleParameter::create<ParameterName::CssMediaScripting>(Scripting::InitialOnly));
	} else if (value.equals("enabled")) {
		return cb(StyleParameter::create<ParameterName::CssMediaScripting>(Scripting::Enabled));
	}
	return false;
}),
	pair("aspect-ratio",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	float ratio = 0.0f;
	if (css_readAspectRatioValue(value, ratio)) {
		return cb(StyleParameter::create<ParameterName::CssMediaAspectRatio>(ratio));
	}
	return false;
}),
	pair("min-aspect-ratio",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	float ratio = 0.0f;
	if (css_readAspectRatioValue(value, ratio)) {
		return cb(StyleParameter::create<ParameterName::CssMediaMinAspectRatio>(ratio));
	}
	return false;
}),
	pair("max-aspect-ratio",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	float ratio = 0.0f;
	if (css_readAspectRatioValue(value, ratio)) {
		return cb(StyleParameter::create<ParameterName::CssMediaMaxAspectRatio>(ratio));
	}
	return false;
}),
	pair("resolution",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	Metric size;
	if (parser::readStyleMetric(value, size, true)) {
		return cb(StyleParameter::create<ParameterName::CssMediaResolution>(size));
	}
	return false;
}),
	pair("min-resolution",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	Metric size;
	if (parser::readStyleMetric(value, size, true)) {
		return cb(StyleParameter::create<ParameterName::CssMediaMinResolution>(size));
	}
	return false;
}),
	pair("max-resolution",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	Metric size;
	if (parser::readStyleMetric(value, size, true)) {
		return cb(StyleParameter::create<ParameterName::CssMediaMaxResolution>(size));
	}
	return false;
}),
	pair("x-option",
			[](const StringView &value, const StyleCallback &cb, const StringCallback &strCb) {
	auto str = StyleContainer::resolveCssString(value);
	if (!str.empty()) {
		return cb(StyleParameter::create<ParameterName::CssMediaOption>(strCb(str)));
	}
	return false;
})};

void StyleContainer::readCssParameter(const StringView &name, const StringView &value,
		const StyleCallback &cb, const StringCallback &strCb) {
	auto it = s_cssParameters.find(name);
	if (it != s_cssParameters.end()) {
		it->second(value, cb, strCb);
	} else {
		if (!name.is('-')) {
			log::source().info("document::StyleContainer", "Unknown CSS parameter: ", name);
		}
	}
}

bool expandCssVariables(StringView value, const Callback<StringView(StringView)> &lookup,
		const Callback<void(StringView)> &out, uint32_t depth) {
	// a substituted value is expanded in turn, so a cycle would recurse forever; CSS calls a
	// cyclic reference invalid, and this is where that is detected
	constexpr uint32_t MaxDepth = 16;
	if (depth > MaxDepth) {
		return false;
	}

	const size_t n = value.size();
	size_t i = 0, plain = 0;
	while (i < n) {
		if (value[i] != 'v' || n - i < 4 || value.sub(i, 4) != "var(") {
			++i;
			continue;
		}

		// the argument list runs to the matching paren
		size_t argStart = i + 4;
		size_t j = argStart;
		uint32_t nest = 1;
		while (j < n && nest > 0) {
			if (value[j] == '(') {
				++nest;
			} else if (value[j] == ')') {
				--nest;
			}
			++j;
		}
		if (nest != 0) {
			return false; // unbalanced - not a usable declaration
		}
		auto args = value.sub(argStart, (j - 1) - argStart);

		// split off the fallback at the first TOP-LEVEL comma (a fallback may itself be a
		// var() with its own comma)
		StringView name = args;
		StringView fallback;
		{
			uint32_t argNest = 0;
			for (size_t k = 0; k < args.size(); ++k) {
				if (args[k] == '(') {
					++argNest;
				} else if (args[k] == ')') {
					if (argNest > 0) {
						--argNest;
					}
				} else if (args[k] == ',' && argNest == 0) {
					name = args.sub(0, k);
					fallback = args.sub(k + 1);
					break;
				}
			}
		}
		name.trimChars<StringView::WhiteSpace>();
		fallback.trimChars<StringView::WhiteSpace>();

		out << value.sub(plain, i - plain);

		auto resolved = lookup(name);
		if (!resolved.empty()) {
			if (!expandCssVariables(resolved, lookup, out, depth + 1)) {
				return false;
			}
		} else if (!fallback.empty()) {
			if (!expandCssVariables(fallback, lookup, out, depth + 1)) {
				return false;
			}
		} else {
			return false; // undefined variable, no fallback
		}

		i = plain = j;
	}

	out << value.sub(plain, n - plain);
	return true;
}

} // namespace stappler::document
