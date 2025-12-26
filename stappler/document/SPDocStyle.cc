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

#include "SPDocStyle.h"

namespace STAPPLER_VERSIONIZED stappler::document {

template <>
void StyleParameter::set<ParameterName::CssFontStyle, FontStyle>(const FontStyle &v) {
	value.fontStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontWeight, FontWeight>(const FontWeight &v) {
	value.fontWeight = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontSize, FontSize>(const FontSize &v) {
	value.fontSize = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontStretch, FontStretch>(const FontStretch &v) {
	value.fontStretch = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontVariant, FontVariant>(const FontVariant &v) {
	value.fontVariant = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontSizeNumeric, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontSizeIncrement, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssTextTransform, TextTransform>(const TextTransform &v) {
	value.textTransform = v;
}
template <>
void StyleParameter::set<ParameterName::CssTextDecoration, TextDecoration>(
		const TextDecoration &v) {
	value.textDecoration = v;
}
template <>
void StyleParameter::set<ParameterName::CssTextAlign, TextAlign>(const TextAlign &v) {
	value.textAlign = v;
}
template <>
void StyleParameter::set<ParameterName::CssWhiteSpace, WhiteSpace>(const WhiteSpace &v) {
	value.whiteSpace = v;
}
template <>
void StyleParameter::set<ParameterName::CssHyphens, Hyphens>(const Hyphens &v) {
	value.hyphens = v;
}
template <>
void StyleParameter::set<ParameterName::CssDisplay, Display>(const Display &v) {
	value.display = v;
}
template <>
void StyleParameter::set<ParameterName::CssListStyleType, ListStyleType>(const ListStyleType &v) {
	value.listStyleType = v;
}
template <>
void StyleParameter::set<ParameterName::CssListStylePosition, ListStylePosition>(
		const ListStylePosition &v) {
	value.listStylePosition = v;
}
template <>
void StyleParameter::set<ParameterName::CssXListStyleOffset, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssFloat, Float>(const Float &v) {
	value.floating = v;
}
template <>
void StyleParameter::set<ParameterName::CssClear, Clear>(const Clear &v) {
	value.clear = v;
}
template <>
void StyleParameter::set<ParameterName::CssColor, Color3B>(const Color3B &v) {
	value.color = v;
}
template <>
void StyleParameter::set<ParameterName::CssColor, Color>(const Color &v) {
	value.color = v;
}
template <>
void StyleParameter::set<ParameterName::CssOpacity, uint8_t>(const uint8_t &v) {
	value.opacity = v;
}
template <>
void StyleParameter::set<ParameterName::CssTextIndent, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssLineHeight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMarginTop, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMarginRight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMarginBottom, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMarginLeft, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssHeight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMinWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMinHeight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMaxWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMaxHeight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssPaddingTop, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssPaddingRight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssPaddingBottom, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssPaddingLeft, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssFontFamily, StringId>(const StringId &v) {
	value.stringId = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundImage, StringId>(const StringId &v) {
	value.stringId = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundPositionX, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundPositionY, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundRepeat, BackgroundRepeat>(
		const BackgroundRepeat &v) {
	value.backgroundRepeat = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundSizeWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBackgroundSizeHeight, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssVerticalAlign, VerticalAlign>(const VerticalAlign &v) {
	value.verticalAlign = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderTopStyle, BorderStyle>(const BorderStyle &v) {
	value.borderStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderTopWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderTopColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderRightStyle, BorderStyle>(const BorderStyle &v) {
	value.borderStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderRightWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderRightColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderBottomStyle, BorderStyle>(const BorderStyle &v) {
	value.borderStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderBottomWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderBottomColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderLeftStyle, BorderStyle>(const BorderStyle &v) {
	value.borderStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderLeftWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderLeftColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssOutlineStyle, BorderStyle>(const BorderStyle &v) {
	value.borderStyle = v;
}
template <>
void StyleParameter::set<ParameterName::CssOutlineWidth, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssOutlineColor, Color4B>(const Color4B &v) {
	value.color4 = v;
}
template <>
void StyleParameter::set<ParameterName::CssPageBreakAfter, PageBreak>(const PageBreak &v) {
	value.pageBreak = v;
}
template <>
void StyleParameter::set<ParameterName::CssPageBreakBefore, PageBreak>(const PageBreak &v) {
	value.pageBreak = v;
}
template <>
void StyleParameter::set<ParameterName::CssPageBreakInside, PageBreak>(const PageBreak &v) {
	value.pageBreak = v;
}
template <>
void StyleParameter::set<ParameterName::CssAutofit, Autofit>(const Autofit &v) {
	value.autofit = v;
}
template <>
void StyleParameter::set<ParameterName::CssBorderCollapse, BorderCollapse>(
		const BorderCollapse &v) {
	value.borderCollapse = v;
}
template <>
void StyleParameter::set<ParameterName::CssCaptionSide, CaptionSide>(const CaptionSide &v) {
	value.captionSide = v;
}
template <>
void StyleParameter::set<ParameterName::CssOrphans, uint32_t>(const uint32_t &v) {
	value.uintValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssWidows, uint32_t>(const uint32_t &v) {
	value.uintValue = v;
}

template <>
void StyleParameter::set<ParameterName::CssMediaType, MediaType>(const MediaType &v) {
	value.mediaType = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaOrientation, Orientation>(const Orientation &v) {
	value.orientation = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaHover, Hover>(const Hover &v) {
	value.hover = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaPointer, Pointer>(const Pointer &v) {
	value.pointer = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaLightLevel, LightLevel>(const LightLevel &v) {
	value.lightLevel = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaScripting, Scripting>(const Scripting &v) {
	value.scripting = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaAspectRatio, float>(const float &v) {
	value.floatValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaMinAspectRatio, float>(const float &v) {
	value.floatValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaMaxAspectRatio, float>(const float &v) {
	value.floatValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaResolution, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaMinResolution, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaMaxResolution, Metric>(const Metric &v) {
	value.sizeValue = v;
}
template <>
void StyleParameter::set<ParameterName::CssMediaOption, StringId>(const StringId &v) {
	value.stringId = v;
}

void StyleList::set(const StyleParameter &p, bool force) {
	if (force && p.mediaQuery == MediaQueryIdNone) {
		for (auto &it : data) {
			if (it.name == p.name && it.mediaQuery == p.mediaQuery) {
				it.value = p.value;
				return;
			}
		}
	}

	data.push_back(p);
}

void getStyleForTag(StyleList &style, StringView tag, StringView parent,
		const MediaParameters &media) {
	if (tag == "div") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
	}

	if (tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5"
			|| tag == "h6") {
		if (parent != "li" && parent != "blockquote") {
			style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
					Metric(0.5f, Metric::Units::Em)));
			style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
					Metric(0.5f, Metric::Units::Em)));
			if (parent != "dd" && parent != "figcaption") {
				style.data.push_back(StyleParameter::create<ParameterName::CssTextIndent>(
						Metric(1.5f, Metric::Units::Rem)));
			}
		}
		style.data.push_back(StyleParameter::create<ParameterName::CssLineHeight>(
				Metric(1.15f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
	}

	if (tag == "span" || tag == "strong" || tag == "em" || tag == "nobr" || tag == "sub"
			|| tag == "sup" || tag == "inf" || tag == "b" || tag == "i" || tag == "u"
			|| tag == "code") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Inline));
	}

	if (tag == "h1") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize(32)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::ExtraLight));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(222)));

	} else if (tag == "h2") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize(28)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Regular));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(222)));

	} else if (tag == "h3") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XXLarge));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Regular));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(200)));

	} else if (tag == "h4") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize::XLarge));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Medium));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(188)));

	} else if (tag == "h5") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize(18)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Regular));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(222)));

	} else if (tag == "h6") {
		style.set(StyleParameter::create<ParameterName::CssMarginTop>(
						  Metric(0.8f, Metric::Units::Em)),
				true);
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize::Large));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Medium));
		style.data.push_back(StyleParameter::create<ParameterName::CssOpacity>(uint8_t(216)));

	} else if (tag == "p") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Justify));
		style.data.push_back(StyleParameter::create<ParameterName::CssHyphens>(Hyphens::Auto));

	} else if (tag == "hr") {
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginRight>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginLeft>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssHeight>(Metric(2, Metric::Units::Px)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssBackgroundColor>(Color4B(0, 0, 0, 127)));

	} else if (tag == "a") {
		style.data.push_back(StyleParameter::create<ParameterName::CssTextDecoration>(
				TextDecoration::Underline));

		auto c = Color(media.defaultBackground).text();
		if (c == Color::White.text()) {
			// light theme
			style.data.push_back(
					StyleParameter::create<ParameterName::CssColor>(Color3B(0x0d, 0x47, 0xa1)));
		} else {
			// dark theme 70b1f0
			style.data.push_back(
					StyleParameter::create<ParameterName::CssColor>(Color3B(0x70, 0xb1, 0xf0)));
		}

	} else if (tag == "b" || tag == "strong") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Bold));

	} else if (tag == "s" || tag == "strike") {
		style.data.push_back(StyleParameter::create<ParameterName::CssTextDecoration>(
				TextDecoration::LineThrough));

	} else if (tag == "i" || tag == "em") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontStyle>(FontStyle::Italic));

	} else if (tag == "u") {
		style.data.push_back(StyleParameter::create<ParameterName::CssTextDecoration>(
				TextDecoration::Underline));

	} else if (tag == "nobr") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::Nowrap));

	} else if (tag == "pre") {
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::Pre));
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssBackgroundColor>(
				Color4B(228, 228, 228, 255)));

		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingBottom>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingRight>(
				Metric(0.5f, Metric::Units::Em)));

	} else if (tag == "code") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontFamily>(StringId("monospace"_hash)));
		style.data.push_back(StyleParameter::create<ParameterName::CssBackgroundColor>(
				Color4B(228, 228, 228, 255)));

	} else if (tag == "sub" || tag == "inf") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Sub));
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(0.7f, Metric::Em)));

	} else if (tag == "sup") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssVerticalAlign>(VerticalAlign::Super));
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSizeIncrement>(
				Metric(0.7f, Metric::Em)));

	} else if (tag == "body") {
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginLeft>(
				Metric(0.8f, Metric::Units::Rem), MediaQuery::IsScreenLayout));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginRight>(
				Metric(0.8f, Metric::Units::Rem), MediaQuery::IsScreenLayout));

	} else if (tag == "br") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssWhiteSpace>(WhiteSpace::PreLine));
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Inline));

	} else if (tag == "li") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::ListItem));
		style.data.push_back(StyleParameter::create<ParameterName::CssLineHeight>(
				Metric(1.2f, Metric::Units::Em)));

	} else if (tag == "ol") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Decimal));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
				Metric(2.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssXListStyleOffset>(
				Metric(0.6f, Metric::Units::Rem)));

	} else if (tag == "ul") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		if (parent == "li") {
			style.data.push_back(
					StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Circle));
		} else {
			style.data.push_back(
					StyleParameter::create<ParameterName::CssListStyleType>(ListStyleType::Disc));
		}
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
				Metric(2.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssXListStyleOffset>(
				Metric(0.6f, Metric::Units::Rem)));

	} else if (tag == "img") {
		style.data.push_back(StyleParameter::create<ParameterName::CssBackgroundSizeWidth>(
				Metric(1.0, Metric::Units::Contain)));
		style.data.push_back(StyleParameter::create<ParameterName::CssBackgroundSizeHeight>(
				Metric(1.0, Metric::Units::Contain)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssPageBreakInside>(PageBreak::Avoid));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssDisplay>(Display::InlineBlock));

	} else if (tag == "table") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Table));

	} else if (tag == "blockquote") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));

		if (parent == "blockquote") {
			style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
					Metric(0.8f, Metric::Units::Rem)));
		} else {
			style.data.push_back(StyleParameter::create<ParameterName::CssMarginLeft>(
					Metric(1.5f, Metric::Units::Rem)));
			style.data.push_back(StyleParameter::create<ParameterName::CssMarginRight>(
					Metric(1.5f, Metric::Units::Rem)));
			style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
					Metric(1.0f, Metric::Units::Rem)));
		}

		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingTop>(
				Metric(0.1f, Metric::Units::Rem)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingBottom>(
				Metric(0.3f, Metric::Units::Rem)));

		style.data.push_back(
				StyleParameter::create<ParameterName::CssBorderLeftColor>(Color4B(0, 0, 0, 64)));
		style.data.push_back(StyleParameter::create<ParameterName::CssBorderLeftWidth>(
				Metric(3, Metric::Units::Px)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssBorderLeftStyle>(BorderStyle::Solid));

	} else if (tag == "dl") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginLeft>(
				Metric(1.5f, Metric::Units::Rem)));

	} else if (tag == "dt") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Bold));

	} else if (tag == "dd") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
				Metric(1.0f, Metric::Units::Rem)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));

		style.data.push_back(
				StyleParameter::create<ParameterName::CssBorderLeftColor>(Color4B(0, 0, 0, 64)));
		style.data.push_back(StyleParameter::create<ParameterName::CssBorderLeftWidth>(
				Metric(2, Metric::Units::Px)));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssBorderLeftStyle>(BorderStyle::Solid));

	} else if (tag == "figure") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));

	} else if (tag == "figcaption") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Block));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginTop>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssMarginBottom>(
				Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(StyleParameter::create<ParameterName::CssFontSize>(FontSize::Small));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssFontWeight>(FontWeight::Medium));
		style.data.push_back(
				StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Center));

	} else if (tag == "caption") {
		style.data.push_back(
				StyleParameter::create<ParameterName::CssTextAlign>(TextAlign::Center));

		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingTop>(
				Metric(0.3f, Metric::Units::Rem)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingBottom>(
				Metric(0.3f, Metric::Units::Rem)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingLeft>(
				Metric(0.3f, Metric::Units::Rem)));
		style.data.push_back(StyleParameter::create<ParameterName::CssPaddingRight>(
				Metric(0.3f, Metric::Units::Rem)));
	} else if (tag == "__value__") {
		style.data.push_back(StyleParameter::create<ParameterName::CssDisplay>(Display::Inline));
	}
}

void StyleList::merge(const StyleList &style, bool inherit) {
	for (auto &it : style.data) {
		if (!inherit || StyleList::isInheritable(it.name)) {
			set(it, true);
		}
	}
}

void StyleList::merge(const StyleList &style, const SpanView<bool> &media, bool inherit) {
	for (auto &it : style.data) {
		if ((!inherit || StyleList::isInheritable(it.name))
				&& (it.mediaQuery == MediaQueryIdNone || media.at(it.mediaQuery.get()))) {
			set(StyleParameter(it, MediaQueryIdNone, it.rule), true);
		}
	}
}

auto StyleList::get(ParameterName name) const -> Vector<StyleParameter> {
	Vector<StyleParameter> ret;
	for (auto &it : data) {
		if (it.name == name) {
			ret.push_back(it);
		}
	}
	return ret;
}

auto StyleList::get(ParameterName name, const StyleInterface *iface) const
		-> Vector<StyleParameter> {
	Vector<StyleParameter> ret;
	ret.reserve(1);
	for (auto &it : data) {
		if (it.name == name
				&& (it.mediaQuery == MediaQueryIdNone || iface->resolveMediaQuery(it.mediaQuery))) {
			if (ret.empty()) {
				ret.push_back(it);
			} else {
				ret[0] = it;
			}
		}
	}
	return ret;
}

bool MediaQuery::Query::setMediaType(StringView id) {
	if (id == "all") {
		params.push_back(StyleParameter::create<ParameterName::CssMediaType>(MediaType::All));
	} else if (id == "screen") {
		params.push_back(StyleParameter::create<ParameterName::CssMediaType>(MediaType::Screen));
	} else if (id == "print") {
		params.push_back(StyleParameter::create<ParameterName::CssMediaType>(MediaType::Print));
	} else if (id == "speech") {
		params.push_back(StyleParameter::create<ParameterName::CssMediaType>(MediaType::Speech));
	} else {
		return false; // invalid mediatype
	}
	return true;
}

void MediaQuery::clear() { list.clear(); }

static FontSize modifySize(FontSize current, Metric inc) { return current.scale(inc.value); }

FontStyleParameters StyleList::compileFontStyle(const StyleInterface *renderer) const {
	FontStyleParameters compiled;
	Metric incr = Metric();
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssFontSizeIncrement: incr = it.value.sizeValue; break;
			case ParameterName::CssFontStyle: compiled.fontStyle = it.value.fontStyle; break;
			case ParameterName::CssFontWeight: compiled.fontWeight = it.value.fontWeight; break;
			case ParameterName::CssFontSize: compiled.fontSize = it.value.fontSize; break;
			case ParameterName::CssFontStretch: compiled.fontStretch = it.value.fontStretch; break;
			case ParameterName::CssFontVariant: compiled.fontVariant = it.value.fontVariant; break;
			case ParameterName::CssListStyleType:
				compiled.listStyleType = it.value.listStyleType;
				break;
			case ParameterName::CssFontFamily:
				compiled.fontFamily = renderer->resolveString(it.value.stringId);
				break;
			default: break;
			}
		}
	}
	if (compiled.fontFamily.empty()) {
		compiled.fontFamily = StringView("default");
	}
	if (incr.metric != Metric::Auto) {
		compiled.fontSize = modifySize(compiled.fontSize, incr);
	}

	compiled.density = renderer->getDensity() * renderer->getFontScale();

	return compiled;
}
TextLayoutParameters StyleList::compileTextLayout(const StyleInterface *renderer) const {
	TextLayoutParameters compiled;
	auto textColor = renderer->getTextColor();
	compiled.color = Color3B(textColor.r, textColor.g, textColor.b);
	compiled.opacity = textColor.a;

	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssTextTransform:
				compiled.textTransform = it.value.textTransform;
				break;
			case ParameterName::CssTextDecoration:
				compiled.textDecoration = it.value.textDecoration;
				break;
			case ParameterName::CssWhiteSpace: compiled.whiteSpace = it.value.whiteSpace; break;
			case ParameterName::CssHyphens: compiled.hyphens = it.value.hyphens; break;
			case ParameterName::CssVerticalAlign:
				compiled.verticalAlign = it.value.verticalAlign;
				break;
			case ParameterName::CssColor: compiled.color = it.value.color; break;
			case ParameterName::CssOpacity: compiled.opacity = it.value.opacity; break;
			default: break;
			}
		}
	}
	return compiled;
}
ParagraphLayoutParameters StyleList::compileParagraphLayout(const StyleInterface *renderer) const {
	ParagraphLayoutParameters compiled;
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssTextAlign: compiled.textAlign = it.value.textAlign; break;
			case ParameterName::CssTextIndent: compiled.textIndent = it.value.sizeValue; break;
			case ParameterName::CssLineHeight: compiled.lineHeight = it.value.sizeValue; break;
			case ParameterName::CssXListStyleOffset:
				compiled.listOffset = it.value.sizeValue;
				break;
			default: break;
			}
		}
	}
	return compiled;
}
BlockModelParameters StyleList::compileBlockModel(const StyleInterface *renderer) const {
	BlockModelParameters compiled;
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssDisplay: compiled.display = it.value.display; break;
			case ParameterName::CssFloat: compiled.floating = it.value.floating; break;
			case ParameterName::CssClear: compiled.clear = it.value.clear; break;
			case ParameterName::CssMarginTop: compiled.marginTop = it.value.sizeValue; break;
			case ParameterName::CssMarginRight: compiled.marginRight = it.value.sizeValue; break;
			case ParameterName::CssMarginBottom: compiled.marginBottom = it.value.sizeValue; break;
			case ParameterName::CssMarginLeft: compiled.marginLeft = it.value.sizeValue; break;
			case ParameterName::CssPaddingTop: compiled.paddingTop = it.value.sizeValue; break;
			case ParameterName::CssPaddingRight: compiled.paddingRight = it.value.sizeValue; break;
			case ParameterName::CssPaddingBottom:
				compiled.paddingBottom = it.value.sizeValue;
				break;
			case ParameterName::CssPaddingLeft: compiled.paddingLeft = it.value.sizeValue; break;
			case ParameterName::CssWidth: compiled.width = it.value.sizeValue; break;
			case ParameterName::CssHeight: compiled.height = it.value.sizeValue; break;
			case ParameterName::CssMinWidth: compiled.minWidth = it.value.sizeValue; break;
			case ParameterName::CssMinHeight: compiled.minHeight = it.value.sizeValue; break;
			case ParameterName::CssMaxWidth: compiled.maxWidth = it.value.sizeValue; break;
			case ParameterName::CssMaxHeight: compiled.maxHeight = it.value.sizeValue; break;
			case ParameterName::CssListStylePosition:
				compiled.listStylePosition = it.value.listStylePosition;
				break;
			case ParameterName::CssListStyleType:
				compiled.listStyleType = it.value.listStyleType;
				break;
			case ParameterName::CssPageBreakAfter:
				compiled.pageBreakAfter = it.value.pageBreak;
				break;
			case ParameterName::CssPageBreakBefore:
				compiled.pageBreakBefore = it.value.pageBreak;
				break;
			case ParameterName::CssPageBreakInside:
				compiled.pageBreakInside = it.value.pageBreak;
				break;
			case ParameterName::CssBorderCollapse:
				compiled.borderCollapse = it.value.borderCollapse;
				break;
			case ParameterName::CssCaptionSide: compiled.captionSide = it.value.captionSide; break;
			case ParameterName::CssOrphans: compiled.orphans = it.value.uintValue; break;
			case ParameterName::CssWidows: compiled.widows = it.value.uintValue; break;
			default: break;
			}
		}
	}
	return compiled;
}
InlineModelParameters StyleList::compileInlineModel(const StyleInterface *renderer) const {
	InlineModelParameters compiled;
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssMarginRight: compiled.marginRight = it.value.sizeValue; break;
			case ParameterName::CssMarginLeft: compiled.marginLeft = it.value.sizeValue; break;
			case ParameterName::CssPaddingRight: compiled.paddingRight = it.value.sizeValue; break;
			case ParameterName::CssPaddingLeft: compiled.paddingLeft = it.value.sizeValue; break;
			default: break;
			}
		}
	}
	return compiled;
}
BackgroundParameters StyleList::compileBackground(const StyleInterface *renderer) const {
	BackgroundParameters compiled;
	uint8_t opacity = 255;
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssDisplay: compiled.display = it.value.display; break;
			case ParameterName::CssBackgroundColor:
				compiled.backgroundColor = it.value.color4;
				break;
			case ParameterName::CssOpacity: opacity = it.value.opacity; break;
			case ParameterName::CssBackgroundRepeat:
				compiled.backgroundRepeat = it.value.backgroundRepeat;
				break;
			case ParameterName::CssBackgroundPositionX:
				compiled.backgroundPositionX = it.value.sizeValue;
				break;
			case ParameterName::CssBackgroundPositionY:
				compiled.backgroundPositionY = it.value.sizeValue;
				break;
			case ParameterName::CssBackgroundSizeWidth:
				compiled.backgroundSizeWidth = it.value.sizeValue;
				break;
			case ParameterName::CssBackgroundSizeHeight:
				compiled.backgroundSizeHeight = it.value.sizeValue;
				break;
			case ParameterName::CssBackgroundImage:
				compiled.backgroundImage = renderer->resolveString(it.value.stringId);
				break;
			default: break;
			}
		}
	}

	compiled.backgroundColor.a = uint8_t(compiled.backgroundColor.a * opacity / 255.0f);
	return compiled;
}

OutlineParameters StyleList::compileOutline(const StyleInterface *renderer) const {
	OutlineParameters compiled;
	for (auto &it : data) {
		if (it.mediaQuery == MediaQueryIdNone || renderer->resolveMediaQuery(it.mediaQuery)) {
			switch (it.name) {
			case ParameterName::CssBorderLeftStyle:
				compiled.left.style = it.value.borderStyle;
				break;
			case ParameterName::CssBorderLeftWidth: compiled.left.width = it.value.sizeValue; break;
			case ParameterName::CssBorderLeftColor: compiled.left.color = it.value.color4; break;
			case ParameterName::CssBorderTopStyle: compiled.top.style = it.value.borderStyle; break;
			case ParameterName::CssBorderTopWidth: compiled.top.width = it.value.sizeValue; break;
			case ParameterName::CssBorderTopColor: compiled.top.color = it.value.color4; break;
			case ParameterName::CssBorderRightStyle:
				compiled.right.style = it.value.borderStyle;
				break;
			case ParameterName::CssBorderRightWidth:
				compiled.right.width = it.value.sizeValue;
				break;
			case ParameterName::CssBorderRightColor: compiled.right.color = it.value.color4; break;
			case ParameterName::CssBorderBottomStyle:
				compiled.bottom.style = it.value.borderStyle;
				break;
			case ParameterName::CssBorderBottomWidth:
				compiled.bottom.width = it.value.sizeValue;
				break;
			case ParameterName::CssBorderBottomColor:
				compiled.bottom.color = it.value.color4;
				break;
			case ParameterName::CssOutlineStyle:
				compiled.outline.style = it.value.borderStyle;
				break;
			case ParameterName::CssOutlineWidth: compiled.outline.width = it.value.sizeValue; break;
			case ParameterName::CssOutlineColor: compiled.outline.color = it.value.color4; break;
			default: break;
			}
		}
	}
	return compiled;
}

void writeStyle(memory::PoolInterface::StringStreamType &stream, const Metric &size) {
	switch (size.metric) {
	case Metric::Units::Percent: stream << size.value * 100 << "%"; break;
	case Metric::Units::Px: stream << size.value << "px"; break;
	case Metric::Units::Em: stream << size.value << "em"; break;
	case Metric::Units::Rem: stream << size.value << "rem"; break;
	case Metric::Units::Auto: stream << "auto"; break;
	case Metric::Units::Dpi: stream << size.value << "dpi"; break;
	case Metric::Units::Dppx: stream << size.value << "dppx"; break;
	case Metric::Units::Contain: stream << "contain"; break;
	case Metric::Units::Cover: stream << "cover"; break;
	case Metric::Units::Vw: stream << "vw"; break;
	case Metric::Units::Vh: stream << "vh"; break;
	case Metric::Units::VMin: stream << "vmin"; break;
	case Metric::Units::VMax: stream << "vmax"; break;
	}
}

void writeStyle(memory::PoolInterface::StringStreamType &stream, const Color3B &c) {
	stream << "rgb(" << int(c.r) << ", " << int(c.g) << ", " << int(c.b) << ")";
}

void writeStyle(memory::PoolInterface::StringStreamType &stream, const Color4B &c) {
	stream << "rgba(" << int(c.r) << ", " << int(c.g) << ", " << int(c.b) << ", "
		   << float(c.a) / 255.0f << ")";
}

void writeStyle(memory::PoolInterface::StringStreamType &stream, const uint8_t &o) {
	stream << float(o) / 255.0f;
}

void writeStyle(memory::PoolInterface::StringStreamType &stream, const StringId &s,
		const StyleInterface *iface) {
	if (iface) {
		stream << "\"" << iface->resolveString(s) << "\"";
	} else {
		stream << "StringId(" << s << ")";
	}
}

auto StyleList::css(const StyleInterface *iface) const -> String {
	memory::PoolInterface::StringStreamType stream;
	stream << "{\n";
	for (auto &it : data) {
		stream << "\t";
		switch (it.name) {
		case ParameterName::__BeginCssParameters:
		case ParameterName::__EndCssParameters:
		case ParameterName::__BeginCssMediaParameters:
		case ParameterName::__EndCssMediaParameters: break;
		case ParameterName::Unknown: stream << "unknown"; break;
		case ParameterName::CssFontStyle:
			stream << "font-style: ";
			switch (it.value.fontStyle.get()) {
			case FontStyle::Italic.get(): stream << "italic"; break;
			case FontStyle::Normal.get(): stream << "normal"; break;
			case FontStyle::Oblique.get(): stream << "oblique"; break;
			default: stream << it.value.fontStyle.get(); break;
			};
			break; // enum
		case ParameterName::CssFontWeight:
			stream << "font-weight: ";
			switch (it.value.fontWeight.get()) {
			case FontWeight::Bold.get(): stream << "bold"; break;
			case FontWeight::Normal.get(): stream << "normal"; break;
			default: stream << it.value.fontWeight.get(); break;
			};
			break; // enum
		case ParameterName::CssFontStretch:
			stream << "font-stretch: ";
			switch (it.value.fontStretch.get()) {
			case FontStretch::Normal.get(): stream << "normal"; break;
			case FontStretch::UltraCondensed.get(): stream << "ultra-condensed"; break;
			case FontStretch::ExtraCondensed.get(): stream << "extra-condensed"; break;
			case FontStretch::Condensed.get(): stream << "condensed"; break;
			case FontStretch::SemiCondensed.get(): stream << "semi-condensed"; break;
			case FontStretch::SemiExpanded.get(): stream << "semi-expanded"; break;
			case FontStretch::Expanded.get(): stream << "expanded"; break;
			case FontStretch::ExtraExpanded.get(): stream << "extra-expanded"; break;
			case FontStretch::UltraExpanded.get(): stream << "ultra-expanded"; break;
			default: stream << it.value.fontStretch.get(); break;
			};
			break; // enum
		case ParameterName::CssFontVariant:
			stream << "font-variant: ";
			switch (it.value.fontVariant) {
			case FontVariant::Normal: stream << "normal"; break;
			case FontVariant::SmallCaps: stream << "smapp-caps"; break;
			};
			break; // enum
		case ParameterName::CssFontSize:
			stream << "font-size: ";
			switch (it.value.fontSize.get()) {
			case FontSize::XXSmall.get(): stream << "xx-small"; break;
			case FontSize::XSmall.get(): stream << "x-small"; break;
			case FontSize::Small.get(): stream << "small"; break;
			case FontSize::Medium.get(): stream << "medium"; break;
			case FontSize::Large.get(): stream << "large"; break;
			case FontSize::XLarge.get(): stream << "x-large"; break;
			case FontSize::XXLarge.get(): stream << "xx-large"; break;
			default: stream << "-" << it.value.fontSize.get() << "-"; break;
			};
			break; // enum
		case ParameterName::CssFontSizeNumeric:
			stream << "font-size: ";
			writeStyle(stream, it.value.sizeValue);
			break;
		case ParameterName::CssFontSizeIncrement:
			stream << "font-size-increment: ";
			writeStyle(stream, it.value.sizeValue);
			break; // enum
		case ParameterName::CssTextTransform:
			stream << "text-transform: ";
			switch (it.value.textTransform) {
			case TextTransform::Uppercase: stream << "uppercase"; break;
			case TextTransform::Lowercase: stream << "lovercase"; break;
			case TextTransform::None: stream << "none"; break;
			};
			break; // enum
		case ParameterName::CssTextDecoration:
			stream << "text-decoration: ";
			switch (it.value.textDecoration) {
			case TextDecoration::Overline: stream << "overline"; break;
			case TextDecoration::LineThrough: stream << "line-through"; break;
			case TextDecoration::Underline: stream << "underline"; break;
			case TextDecoration::None: stream << "none"; break;
			};
			break; // enum
		case ParameterName::CssTextAlign:
			stream << "text-align: ";
			switch (it.value.textAlign) {
			case TextAlign::Center: stream << "center"; break;
			case TextAlign::Left: stream << "left"; break;
			case TextAlign::Right: stream << "right"; break;
			case TextAlign::Justify: stream << "justify"; break;
			};
			break; // enum
		case ParameterName::CssWhiteSpace:
			stream << "white-space: ";
			switch (it.value.whiteSpace) {
			case WhiteSpace::Normal: stream << "normal"; break;
			case WhiteSpace::Nowrap: stream << "nowrap"; break;
			case WhiteSpace::Pre: stream << "pre"; break;
			case WhiteSpace::PreLine: stream << "pre-line"; break;
			case WhiteSpace::PreWrap: stream << "pre-wrap"; break;
			};
			break; // enum
		case ParameterName::CssHyphens:
			stream << "hyphens: ";
			switch (it.value.hyphens) {
			case Hyphens::None: stream << "none"; break;
			case Hyphens::Manual: stream << "manual"; break;
			case Hyphens::Auto: stream << "auto"; break;
			};
			break; // enum
		case ParameterName::CssDisplay:
			stream << "display: ";
			switch (it.value.display) {
			case Display::None: stream << "none"; break;
			case Display::Default: stream << "default"; break;
			case Display::RunIn: stream << "run-in"; break;
			case Display::Inline: stream << "inline"; break;
			case Display::InlineBlock: stream << "inline-block"; break;
			case Display::Block: stream << "block"; break;
			case Display::ListItem: stream << "list-item"; break;
			case Display::Table: stream << "table"; break;
			case Display::TableCell: stream << "table-cell"; break;
			case Display::TableColumn: stream << "table-column"; break;
			case Display::TableCaption: stream << "table-caption"; break;
			};
			break; // enum
		case ParameterName::CssFloat:
			stream << "float: ";
			switch (it.value.floating) {
			case Float::None: stream << "none"; break;
			case Float::Left: stream << "left"; break;
			case Float::Right: stream << "right"; break;
			};
			break; // enum
		case ParameterName::CssClear:
			stream << "clear: ";
			switch (it.value.clear) {
			case Clear::None: stream << "none"; break;
			case Clear::Left: stream << "left"; break;
			case Clear::Right: stream << "right"; break;
			case Clear::Both: stream << "both"; break;
			};
			break; // enum
		case ParameterName::CssListStylePosition:
			stream << "list-style-position: ";
			switch (it.value.listStylePosition) {
			case ListStylePosition::Outside: stream << "outside"; break;
			case ListStylePosition::Inside: stream << "inside"; break;
			};
			break; // enum
		case ParameterName::CssListStyleType:
			stream << "list-style-type: ";
			switch (it.value.listStyleType) {
			case ListStyleType::None: stream << "none"; break;
			case ListStyleType::Circle: stream << "circle"; break;
			case ListStyleType::Disc: stream << "disc"; break;
			case ListStyleType::Square: stream << "square"; break;
			case ListStyleType::XMdash: stream << "x-mdash"; break;
			case ListStyleType::Decimal: stream << "decimial"; break;
			case ListStyleType::DecimalLeadingZero: stream << "decimial-leading-zero"; break;
			case ListStyleType::LowerAlpha: stream << "lower-alpha"; break;
			case ListStyleType::LowerGreek: stream << "lower-greek"; break;
			case ListStyleType::LowerRoman: stream << "lower-roman"; break;
			case ListStyleType::UpperAlpha: stream << "upper-alpha"; break;
			case ListStyleType::UpperRoman: stream << "upper-roman"; break;
			};
			break; // enum
		case ParameterName::CssXListStyleOffset:
			stream << "x-list-style-offset: ";
			writeStyle(stream, it.value.sizeValue);
			break; // enum
		case ParameterName::CssColor:
			stream << "color: ";
			writeStyle(stream, it.value.color);
			break; // color
		case ParameterName::CssOpacity:
			stream << "opacity: ";
			writeStyle(stream, it.value.opacity);
			break; // opacity
		case ParameterName::CssTextIndent:
			stream << "text-indent: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssLineHeight:
			stream << "line-height: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMarginTop:
			stream << "margin-top: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMarginRight:
			stream << "margin-right: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMarginBottom:
			stream << "margin-bottom: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMarginLeft:
			stream << "margin-left: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssWidth:
			stream << "width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssHeight:
			stream << "height: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMinWidth:
			stream << "min-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMinHeight:
			stream << "min-height: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMaxWidth:
			stream << "max-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssMaxHeight:
			stream << "max-height: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssPaddingTop:
			stream << "padding-top: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssPaddingRight:
			stream << "padding-right: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssPaddingBottom:
			stream << "padding-bottom: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssPaddingLeft:
			stream << "padding-left: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssFontFamily:
			stream << "font-family: ";
			writeStyle(stream, it.value.stringId, iface);
			break; // string id
		case ParameterName::CssBackgroundColor:
			stream << "background-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssBackgroundImage:
			stream << "background-image: ";
			writeStyle(stream, it.value.stringId, iface);
			break; // string id
		case ParameterName::CssBackgroundPositionX:
			stream << "background-position-x: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBackgroundPositionY:
			stream << "background-position-y: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBackgroundRepeat:
			stream << "background-repeat: ";
			switch (it.value.backgroundRepeat) {
			case BackgroundRepeat::NoRepeat: stream << "no-repeat"; break;
			case BackgroundRepeat::Repeat: stream << "repeat"; break;
			case BackgroundRepeat::RepeatX: stream << "repeat-x"; break;
			case BackgroundRepeat::RepeatY: stream << "repeat-y"; break;
			};
			break; // enum
		case ParameterName::CssBackgroundSizeWidth:
			stream << "background-size-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBackgroundSizeHeight:
			stream << "background-size-height: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssVerticalAlign:
			stream << "vertical-align: ";
			switch (it.value.verticalAlign) {
			case VerticalAlign::Baseline: stream << "baseline"; break;
			case VerticalAlign::Sub: stream << "sub"; break;
			case VerticalAlign::Super: stream << "super"; break;
			case VerticalAlign::Middle: stream << "middle"; break;
			case VerticalAlign::Top: stream << "top"; break;
			case VerticalAlign::Bottom: stream << "bottom"; break;
			};
			break; // enum
		case ParameterName::CssBorderTopStyle:
			stream << "border-top-style: ";
			switch (it.value.borderStyle) {
			case BorderStyle::None: stream << "none"; break;
			case BorderStyle::Solid: stream << "solid"; break;
			case BorderStyle::Dotted: stream << "dotted"; break;
			case BorderStyle::Dashed: stream << "dashed"; break;
			};
			break; // enum
		case ParameterName::CssBorderTopWidth:
			stream << "border-top-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBorderTopColor:
			stream << "border-top-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssBorderRightStyle:
			stream << "border-right-style: ";
			switch (it.value.borderStyle) {
			case BorderStyle::None: stream << "none"; break;
			case BorderStyle::Solid: stream << "solid"; break;
			case BorderStyle::Dotted: stream << "dotted"; break;
			case BorderStyle::Dashed: stream << "dashed"; break;
			};
			break; // enum
		case ParameterName::CssBorderRightWidth:
			stream << "border-right-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBorderRightColor:
			stream << "border-right-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssBorderBottomStyle:
			stream << "border-bottom-style: ";
			switch (it.value.borderStyle) {
			case BorderStyle::None: stream << "none"; break;
			case BorderStyle::Solid: stream << "solid"; break;
			case BorderStyle::Dotted: stream << "dotted"; break;
			case BorderStyle::Dashed: stream << "dashed"; break;
			};
			break; // enum
		case ParameterName::CssBorderBottomWidth:
			stream << "border-bottom-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBorderBottomColor:
			stream << "border-bottom-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssBorderLeftStyle:
			stream << "border-left-style: ";
			switch (it.value.borderStyle) {
			case BorderStyle::None: stream << "none"; break;
			case BorderStyle::Solid: stream << "solid"; break;
			case BorderStyle::Dotted: stream << "dotted"; break;
			case BorderStyle::Dashed: stream << "dashed"; break;
			};
			break; // enum
		case ParameterName::CssBorderLeftWidth:
			stream << "border-left-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssBorderLeftColor:
			stream << "border-left-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssBorderCollapse:
			stream << "border-collapse: ";
			switch (it.value.borderCollapse) {
			case BorderCollapse::Separate: stream << "separate"; break;
			case BorderCollapse::Collapse: stream << "collapse"; break;
			};
			break; // enum
		case ParameterName::CssCaptionSide:
			stream << "caption-side: ";
			switch (it.value.captionSide) {
			case CaptionSide::Top: stream << "top"; break;
			case CaptionSide::Bottom: stream << "bottom"; break;
			};
			break; // enum
		case ParameterName::CssOutlineStyle:
			stream << "outline-style: ";
			switch (it.value.borderStyle) {
			case BorderStyle::None: stream << "none"; break;
			case BorderStyle::Solid: stream << "solid"; break;
			case BorderStyle::Dotted: stream << "dotted"; break;
			case BorderStyle::Dashed: stream << "dashed"; break;
			};
			break; // enum
		case ParameterName::CssOutlineWidth:
			stream << "outline-width: ";
			writeStyle(stream, it.value.sizeValue);
			break; // size
		case ParameterName::CssOutlineColor:
			stream << "outline-color: ";
			writeStyle(stream, it.value.color4);
			break; // color4
		case ParameterName::CssPageBreakAfter:
			stream << "page-break-after: ";
			switch (it.value.pageBreak) {
			case PageBreak::Always: stream << "always"; break;
			case PageBreak::Auto: stream << "auto"; break;
			case PageBreak::Avoid: stream << "avoid"; break;
			case PageBreak::Left: stream << "left"; break;
			case PageBreak::Right: stream << "right"; break;
			};
			break; // enum
		case ParameterName::CssPageBreakBefore:
			stream << "page-break-before: ";
			switch (it.value.pageBreak) {
			case PageBreak::Always: stream << "always"; break;
			case PageBreak::Auto: stream << "auto"; break;
			case PageBreak::Avoid: stream << "avoid"; break;
			case PageBreak::Left: stream << "left"; break;
			case PageBreak::Right: stream << "right"; break;
			};
			break; // enum
		case ParameterName::CssPageBreakInside:
			stream << "page-break-inside: ";
			switch (it.value.pageBreak) {
			case PageBreak::Always: stream << "always"; break;
			case PageBreak::Auto: stream << "auto"; break;
			case PageBreak::Avoid: stream << "avoid"; break;
			case PageBreak::Left: stream << "left"; break;
			case PageBreak::Right: stream << "right"; break;
			};
			break; // enum
		case ParameterName::CssAutofit:
			stream << "x-autofit: ";
			switch (it.value.autofit) {
			case Autofit::None: stream << "none"; break;
			case Autofit::Cover: stream << "cover"; break;
			case Autofit::Contain: stream << "contain"; break;
			case Autofit::Width: stream << "width"; break;
			case Autofit::Height: stream << "height"; break;
			}
			break; // enum
		case ParameterName::CssOrphans: stream << "orphans: " << it.value.uintValue; break; // enum
		case ParameterName::CssWidows:
			stream << "widows: " << it.value.uintValue;
			break; // enum

		/* media - specific */
		case ParameterName::CssMediaType:
			stream << "media-type: ";
			switch (it.value.mediaType) {
			case MediaType::All: stream << "all"; break;
			case MediaType::Screen: stream << "screen"; break;
			case MediaType::Print: stream << "print"; break;
			case MediaType::Speech: stream << "speach"; break;
			};
			break; // enum
		case ParameterName::CssMediaOrientation:
			stream << "orientation: ";
			switch (it.value.orientation) {
			case Orientation::Landscape: stream << "landscape"; break;
			case Orientation::Portrait: stream << "portrait"; break;
			};
			break; // enum
		case ParameterName::CssMediaPointer:
			stream << "pointer: ";
			switch (it.value.pointer) {
			case Pointer::None: stream << "none"; break;
			case Pointer::Fine: stream << "fine"; break;
			case Pointer::Coarse: stream << "coarse"; break;
			};
			break; // enum
		case ParameterName::CssMediaHover:
			stream << "hover: ";
			switch (it.value.hover) {
			case Hover::None: stream << "none"; break;
			case Hover::Hover: stream << "hover"; break;
			case Hover::OnDemand: stream << "on-demand"; break;
			};
			break; // enum
		case ParameterName::CssMediaLightLevel:
			stream << "light-level: ";
			switch (it.value.lightLevel) {
			case LightLevel::Dim: stream << "dim"; break;
			case LightLevel::Washed: stream << "washed"; break;
			case LightLevel::Normal: stream << "normal"; break;
			};
			break; // enum
		case ParameterName::CssMediaScripting:
			stream << "scripting: ";
			switch (it.value.scripting) {
			case Scripting::None: stream << "none"; break;
			case Scripting::InitialOnly: stream << "initial-only"; break;
			case Scripting::Enabled: stream << "enabled"; break;
			};
			break; // enum
		case ParameterName::CssMediaAspectRatio:
			stream << "aspect-ratio: ";
			writeStyle(stream, it.value.floatValue);
			break;
		case ParameterName::CssMediaMinAspectRatio:
			stream << "min-aspect-ratio: ";
			writeStyle(stream, it.value.floatValue);
			break;
		case ParameterName::CssMediaMaxAspectRatio:
			stream << "max-aspect-ratio: ";
			writeStyle(stream, it.value.floatValue);
			break;
		case ParameterName::CssMediaResolution:
			stream << "resolution";
			writeStyle(stream, it.value.sizeValue);
			break;
		case ParameterName::CssMediaMinResolution:
			stream << "min-resolution: ";
			writeStyle(stream, it.value.sizeValue);
			break;
		case ParameterName::CssMediaMaxResolution:
			stream << "max-resolution: ";
			writeStyle(stream, it.value.sizeValue);
			break;
		case ParameterName::CssMediaOption:
			stream << "media-option: ";
			writeStyle(stream, it.value.stringId, iface);
			break;
		}
		if (it.mediaQuery != MediaQueryIdNone) {
			stream << " media(" << it.mediaQuery.get();
			if (iface && iface->resolveMediaQuery(it.mediaQuery)) {
				stream << ":passed";
			}
			stream << ")";
		}
		stream << ";\n";
	}
	stream << "}\n";
	return stream.str();
}

BackgroundParameters::BackgroundParameters(Autofit a) {
	backgroundPositionX.metric = Metric::Units::Auto;
	backgroundPositionY.metric = Metric::Units::Auto;
	switch (a) {
	case Autofit::Contain:
		backgroundSizeWidth.metric = Metric::Units::Contain;
		backgroundSizeHeight.metric = Metric::Units::Contain;
		break;
	case Autofit::Cover:
		backgroundSizeWidth.metric = Metric::Units::Cover;
		backgroundSizeHeight.metric = Metric::Units::Cover;
		break;
	case Autofit::Width:
		backgroundSizeWidth.metric = Metric::Units::Percent;
		backgroundSizeWidth.value = 1.0f;
		backgroundSizeHeight.metric = Metric::Units::Auto;
		break;
	case Autofit::Height:
		backgroundSizeWidth.metric = Metric::Units::Auto;
		backgroundSizeHeight.metric = Metric::Units::Percent;
		backgroundSizeHeight.value = 1.0f;
		break;
	case Autofit::None:
		backgroundSizeWidth.metric = Metric::Units::Percent;
		backgroundSizeWidth.value = 1.0f;
		backgroundSizeHeight.metric = Metric::Units::Percent;
		backgroundSizeHeight.value = 1.0f;
		break;
	}
}

float MediaParameters::getDefaultFontSize() const { return FontSize::Medium.get() * fontScale; }

void MediaParameters::addOption(const StringView &str) {
	auto value = string::hash32(str);
	_options.insert(pair(value, str.str<memory::StandartInterface>()));
}
void MediaParameters::removeOption(const StringView &str) {
	auto value = string::hash32(str);
	auto it = _options.find(value);
	if (it != _options.end()) {
		_options.erase(it);
	}
}

bool MediaParameters::hasOption(const StringView &str) const {
	return hasOption(string::hash32(str));
}
bool MediaParameters::hasOption(StringId value) const {
	auto it = _options.find(value);
	if (it != _options.end()) {
		return true;
	} else {
		return false;
	}
}

bool MediaParameters::resolveQuery(const MediaQuery &q) const {
	bool success = false;
	for (auto &query : q.list) {
		bool querySuccess = true;
		for (auto &param : query.params) {
			bool paramSuccess = false;
			switch (param.name) {
			case ParameterName::CssMediaType:
				if (param.value.mediaType == MediaType::All || param.value.mediaType == mediaType) {
					paramSuccess = true;
				}
				break;
			case ParameterName::CssMediaOrientation:
				paramSuccess = param.value.orientation == orientation;
				break;
			case ParameterName::CssMediaPointer:
				paramSuccess = param.value.pointer == pointer;
				break;
			case ParameterName::CssMediaHover: paramSuccess = param.value.hover == hover; break;
			case ParameterName::CssMediaLightLevel:
				paramSuccess = param.value.lightLevel == lightLevel;
				break;
			case ParameterName::CssMediaScripting:
				paramSuccess = param.value.scripting == scripting;
				break;
			case ParameterName::CssWidth:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.width == param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value == 1.0f);
				break;
			case ParameterName::CssHeight:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.height == param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value == 1.0f);
				break;
			case ParameterName::CssMinWidth:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.width >= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value >= 1.0f);
				break;
			case ParameterName::CssMinHeight:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.height >= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value >= 1.0f);
				break;
			case ParameterName::CssMaxWidth:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.width <= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value <= 1.0f);
				break;
			case ParameterName::CssMaxHeight:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Px
									   && surfaceSize.height <= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Percent
								&& param.value.sizeValue.value <= 1.0f);
				break;
			case ParameterName::CssMediaAspectRatio:
				paramSuccess = (surfaceSize.width / surfaceSize.height) == param.value.floatValue;
				break;
			case ParameterName::CssMediaMinAspectRatio:
				paramSuccess = (surfaceSize.width / surfaceSize.height) >= param.value.floatValue;
				break;
			case ParameterName::CssMediaMaxAspectRatio:
				paramSuccess = (surfaceSize.width / surfaceSize.height) <= param.value.floatValue;
				break;
			case ParameterName::CssMediaResolution:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Dpi
									   && dpi == param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Dppx
								&& density == param.value.sizeValue.value);
				break;
			case ParameterName::CssMediaMinResolution:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Dpi
									   && dpi >= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Dppx
								&& density >= param.value.sizeValue.value);
				break;
			case ParameterName::CssMediaMaxResolution:
				paramSuccess = (param.value.sizeValue.metric == Metric::Units::Dpi
									   && dpi <= param.value.sizeValue.value)
						|| (param.value.sizeValue.metric == Metric::Units::Dppx
								&& density <= param.value.sizeValue.value);
				break;
			case ParameterName::CssMediaOption:
				paramSuccess = hasOption(param.value.stringId);
				break;
			default: break;
			}
			if (!paramSuccess) {
				querySuccess = false;
				break;
			}
		}
		if (query.negative) {
			querySuccess = !querySuccess;
		}

		if (querySuccess) {
			success = true;
			break;
		}
	}

	return success;
}

template <>
auto MediaParameters::resolveMediaQueries<memory::PoolInterface>(
		const SpanView<MediaQuery> &vec) const -> memory::PoolInterface::VectorType<bool> {
	memory::PoolInterface::VectorType<bool> ret;
	for (auto &it : vec) { ret.push_back(resolveQuery(it)); }
	return ret;
}

template <>
auto MediaParameters::resolveMediaQueries<memory::StandartInterface>(
		const SpanView<MediaQuery> &vec) const -> memory::StandartInterface::VectorType<bool> {
	memory::StandartInterface::VectorType<bool> ret;
	for (auto &it : vec) { ret.push_back(resolveQuery(it)); }
	return ret;
}

bool MediaParameters::shouldRenderImages() const {
	return (flags & RenderFlags::NoImages) == RenderFlags::None;
}

float MediaParameters::computeValueStrong(Metric m, float base, float fontSize) const {
	switch (m.metric) {
	case Metric::Auto: return nan(); break;
	case Metric::Px: return m.value; break;
	case Metric::Em:
		return (!isnan(fontSize)) ? fontSize * m.value : getDefaultFontSize() * m.value;
		break;
	case Metric::Rem: return getDefaultFontSize() * m.value; break;
	case Metric::Percent: return (!std::isnan(base) ? (base * m.value) : nan()); break;
	case Metric::Cover: return base; break;
	case Metric::Contain: return base; break;
	case Metric::Vw: return m.value * surfaceSize.width * 0.01; break;
	case Metric::Vh: return m.value * surfaceSize.height * 0.01; break;
	case Metric::VMin:
		return m.value * std::min(surfaceSize.width, surfaceSize.height) * 0.01;
		break;
	case Metric::VMax:
		return m.value * std::max(surfaceSize.width, surfaceSize.height) * 0.01;
		break;
	default: return 0.0f; break;
	}
	return 0.0f;
}

float MediaParameters::computeValueAuto(Metric m, float base, float fontSize) const {
	switch (m.metric) {
	case Metric::Auto: return 0.0f; break;
	default: return computeValueStrong(m, base, fontSize); break;
	}
	return 0.0f;
}

SimpleStyleInterface::SimpleStyleInterface() { }
SimpleStyleInterface::SimpleStyleInterface(SpanView<bool> media, SpanView<StringView> strings,
		float density, float fontScale)
: _density(density), _fontScale(fontScale), _media(media), _strings(strings) { }

bool SimpleStyleInterface::resolveMediaQuery(MediaQueryId queryId) const {
	if (queryId.get() < _media.size()) {
		return _media[queryId.get()];
	}
	return false;
}

StringView SimpleStyleInterface::resolveString(StringId str) const {
	if (str < _strings.size()) {
		return _strings[str];
	}
	return StringView();
}

float SimpleStyleInterface::getDensity() const { return _density; }

float SimpleStyleInterface::getFontScale() const { return _fontScale; }

Color4B SimpleStyleInterface::getTextColor() const { return Color4B::WHITE; }

StyleValue::StyleValue() { memset(reinterpret_cast<void *>(this), 0, sizeof(StyleValue)); }

StyleParameter::StyleParameter(ParameterName name, MediaQueryId query, StyleRule r)
: name(name), mediaQuery(query), rule(r) { }


StyleParameter::StyleParameter(const StyleParameter &p, MediaQueryId query, StyleRule r)
: name(p.name), mediaQuery(query), value(p.value), rule(r) { }

bool StyleList::isInheritable(ParameterName name) {
	if (name == ParameterName::CssMarginTop || name == ParameterName::CssMarginRight
			|| name == ParameterName::CssMarginBottom || name == ParameterName::CssMarginLeft
			|| name == ParameterName::CssPaddingTop || name == ParameterName::CssPaddingRight
			|| name == ParameterName::CssPaddingBottom || name == ParameterName::CssPaddingLeft
			|| name == ParameterName::CssWidth || name == ParameterName::CssHeight
			|| name == ParameterName::CssMinWidth || name == ParameterName::CssMinHeight
			|| name == ParameterName::CssMaxWidth || name == ParameterName::CssMaxHeight
			|| name == ParameterName::CssDisplay || name == ParameterName::CssFloat
			|| name == ParameterName::CssClear || name == ParameterName::CssBackgroundColor
			|| name == ParameterName::CssOutlineStyle || name == ParameterName::CssBorderTopStyle
			|| name == ParameterName::CssBorderRightStyle
			|| name == ParameterName::CssBorderBottomStyle
			|| name == ParameterName::CssBorderLeftStyle || name == ParameterName::CssBorderTopWidth
			|| name == ParameterName::CssBorderRightWidth
			|| name == ParameterName::CssBorderBottomWidth
			|| name == ParameterName::CssBorderLeftWidth || name == ParameterName::CssBorderTopColor
			|| name == ParameterName::CssBorderRightColor
			|| name == ParameterName::CssBorderBottomColor
			|| name == ParameterName::CssBorderLeftColor
			|| name == ParameterName::CssPageBreakBefore || name == ParameterName::CssPageBreakAfter
			|| name == ParameterName::CssPageBreakInside) {
		return false;
	}
	return true;
}

} // namespace stappler::document
