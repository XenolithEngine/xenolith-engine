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

#include "XLInheritedStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId InheritedColorStyle::Id;
ComponentId InheritedFontStyle::Id;
ComponentId InheritedTextStyle::Id;

void InheritedColorStyle::merge(const InheritedColorStyle &parent) {
	if ((parent.defined & DefinedColor) && !(defined & DefinedColor)) {
		color = parent.color;
		defined |= DefinedColor;
	}
	if ((parent.defined & DefinedOpacity) && !(defined & DefinedOpacity)) {
		opacity = parent.opacity;
		defined |= DefinedOpacity;
	}
}

void InheritedFontStyle::merge(const InheritedFontStyle &parent) {
	if ((parent.defined & DefinedFontSize) && !(defined & DefinedFontSize)) {
		fontSize = parent.fontSize;
		defined |= DefinedFontSize;
	}
	if ((parent.defined & DefinedFontStyle) && !(defined & DefinedFontStyle)) {
		fontStyle = parent.fontStyle;
		defined |= DefinedFontStyle;
	}
	if ((parent.defined & DefinedFontWeight) && !(defined & DefinedFontWeight)) {
		fontWeight = parent.fontWeight;
		defined |= DefinedFontWeight;
	}
	if ((parent.defined & DefinedFontStretch) && !(defined & DefinedFontStretch)) {
		fontStretch = parent.fontStretch;
		defined |= DefinedFontStretch;
	}
	if ((parent.defined & DefinedFontGrade) && !(defined & DefinedFontGrade)) {
		fontGrade = parent.fontGrade;
		defined |= DefinedFontGrade;
	}
	if ((parent.defined & DefinedFontVariant) && !(defined & DefinedFontVariant)) {
		fontVariant = parent.fontVariant;
		defined |= DefinedFontVariant;
	}
	if ((parent.defined & DefinedFontFamily) && !(defined & DefinedFontFamily)) {
		fontFamily = parent.fontFamily;
		defined |= DefinedFontFamily;
	}
}

void InheritedTextStyle::merge(const InheritedTextStyle &parent) {
	if ((parent.defined & DefinedTextTransform) && !(defined & DefinedTextTransform)) {
		textTransform = parent.textTransform;
		defined |= DefinedTextTransform;
	}
	if ((parent.defined & DefinedTextDecoration) && !(defined & DefinedTextDecoration)) {
		textDecoration = parent.textDecoration;
		defined |= DefinedTextDecoration;
	}
	if ((parent.defined & DefinedWhiteSpace) && !(defined & DefinedWhiteSpace)) {
		whiteSpace = parent.whiteSpace;
		defined |= DefinedWhiteSpace;
	}
	if ((parent.defined & DefinedHyphens) && !(defined & DefinedHyphens)) {
		hyphens = parent.hyphens;
		defined |= DefinedHyphens;
	}
	if ((parent.defined & DefinedVerticalAlign) && !(defined & DefinedVerticalAlign)) {
		verticalAlign = parent.verticalAlign;
		defined |= DefinedVerticalAlign;
	}
	if ((parent.defined & DefinedTextAlign) && !(defined & DefinedTextAlign)) {
		textAlign = parent.textAlign;
		defined |= DefinedTextAlign;
	}
	if ((parent.defined & DefinedLineHeight) && !(defined & DefinedLineHeight)) {
		lineHeight = parent.lineHeight;
		lineHeightAbsolute = parent.lineHeightAbsolute;
		defined |= DefinedLineHeight;
	}
}

} // namespace stappler::xenolith
