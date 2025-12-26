/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLFontLabelBase.h"
#include "XLFontLocale.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

TextLayout::~TextLayout() { }

TextLayout::TextLayout(FontController *h, size_t nchars, size_t nranges) : _handle(h) {
	if (nchars) {
		_data.chars.reserve(nchars);
		_data.lines.reserve(nchars / 60);
	}
	if (nranges) {
		_data.ranges.reserve(nranges);
	}
}

void TextLayout::reserve(size_t nchars, size_t nranges) { _data.reserve(nchars, nranges); }

void TextLayout::clear() {
	_data.chars.clear();
	_data.lines.clear();
	_data.ranges.clear();
	_data.overflow = false;
}

Rc<FontFaceSet> TextLayout::getLayout(const FontParameters &f) {
	auto font = _handle->getLayout(f);
	if (font) {
		_fonts.emplace(font);
	}
	return font;
}

RangeLineIterator TextLayout::begin() const { return _data.begin(); }

RangeLineIterator TextLayout::end() const { return _data.end(); }

auto TextLayout::str(bool filter) const -> WideString {
	WideString ret;
	ret.reserve(_data.chars.size());
	_data.str([&](char32_t ch) { unicode::utf16Encode(ret, ch); }, filter);
	return ret;
}

auto TextLayout::str(uint32_t s_start, uint32_t s_end, size_t maxWords, bool ellipsis,
		bool filter) const -> WideString {
	WideString ret;
	ret.reserve(s_end - s_start + 2);
	_data.str([&](char32_t ch) { unicode::utf16Encode(ret, ch); }, s_start, s_end, maxWords,
			ellipsis, filter);
	return ret;
}

Pair<uint32_t, CharSelectMode> TextLayout::getChar(int32_t x, int32_t y, CharSelectMode m) const {
	return _data.getChar(x, y, m);
}

const LineLayoutData *TextLayout::getLine(uint32_t charIndex) const {
	return _data.getLine(charIndex);
}

uint32_t TextLayout::getLineForChar(uint32_t charIndex) const {
	return _data.getLineForChar(charIndex);
}

Pair<uint32_t, uint32_t> TextLayout::selectWord(uint32_t origin) const {
	return _data.selectWord(origin);
}

geom::Rect TextLayout::getLineRect(uint32_t lineId, float density, const geom::Vec2 &origin) const {
	return _data.getLineRect(lineId, density, origin);
}

geom::Rect TextLayout::getLineRect(const LineLayoutData &line, float density,
		const geom::Vec2 &origin) const {
	return _data.getLineRect(line, density, origin);
}

auto TextLayout::getLabelRects(uint32_t firstCharId, uint32_t lastCharId, float density,
		const geom::Vec2 &origin, const geom::Padding &p) const -> Vector<geom::Rect> {
	Vector<geom::Rect> ret;
	getLabelRects(ret, firstCharId, lastCharId, density, origin, p);
	return ret;
}

void TextLayout::getLabelRects(Vector<geom::Rect> &ret, uint32_t firstCharId, uint32_t lastCharId,
		float density, const geom::Vec2 &origin, const geom::Padding &p) const {
	_data.getLabelRects([&](const geom::Rect &rect) { ret.push_back(rect); }, firstCharId,
			lastCharId, density, origin, p);
}

LabelBase::DescriptionStyle::DescriptionStyle() {
	font.fontFamily = StringView("default");
	font.fontSize = FontSize(14);
	text.opacity = 222;
	text.color = Color3B::BLACK;
	text.whiteSpace = font::WhiteSpace::PreWrap;
}

String LabelBase::DescriptionStyle::getConfigName(bool caps) const {
	return font.getConfigName<Interface>(caps);
}

LabelBase::DescriptionStyle LabelBase::DescriptionStyle::merge(
		const Rc<font::FontController> &source, const Style &style) const {
	DescriptionStyle ret(*this);
	for (auto &it : style.params) {
		switch (it.name) {
		case Style::Name::TextTransform: ret.text.textTransform = it.value.textTransform; break;
		case Style::Name::TextDecoration: ret.text.textDecoration = it.value.textDecoration; break;
		case Style::Name::Hyphens: ret.text.hyphens = it.value.hyphens; break;
		case Style::Name::VerticalAlign: ret.text.verticalAlign = it.value.verticalAlign; break;
		case Style::Name::Color:
			ret.text.color = it.value.color;
			ret.colorDirty = true;
			break;
		case Style::Name::Opacity:
			ret.text.opacity = it.value.opacity;
			ret.opacityDirty = true;
			break;
		case Style::Name::FontSize: ret.font.fontSize = it.value.fontSize; break;
		case Style::Name::FontStyle: ret.font.fontStyle = it.value.fontStyle; break;
		case Style::Name::FontWeight: ret.font.fontWeight = it.value.fontWeight; break;
		case Style::Name::FontStretch: ret.font.fontStretch = it.value.fontStretch; break;
		case Style::Name::FontFamily:
			ret.font.fontFamily = source->getFamilyName(it.value.fontFamily);
			break;
		case Style::Name::FontGrade: ret.font.fontGrade = it.value.fontGrade; break;
		}
	}
	return ret;
}

bool LabelBase::DescriptionStyle::operator==(const DescriptionStyle &style) const {
	return font == style.font && text == style.text && colorDirty == style.colorDirty
			&& opacityDirty == style.opacityDirty;
}
bool LabelBase::DescriptionStyle::operator!=(const DescriptionStyle &style) const {
	return !((*this) == style);
}

LabelBase::Style::Value::Value() { memset(this, 0, sizeof(Value)); }

void LabelBase::Style::set(const Param &p, bool force) {
	if (force) {
		auto it = params.begin();
		while (it != params.end()) {
			if (it->name == p.name) {
				it = params.erase(it);
			} else {
				it++;
			}
		}
	}
	params.push_back(p);
}

void LabelBase::Style::merge(const Style &style) {
	for (auto &it : style.params) { set(it, true); }
}

void LabelBase::Style::clear() { params.clear(); }

LabelBase::ExternalFormatter::~ExternalFormatter() { }

bool LabelBase::ExternalFormatter::init(font::FontController *s, float w, float density) {
	if (!s) {
		return false;
	}

	_density = density;
	_spec = Rc<TextLayout>::alloc(s);
	_formatter.setFontCallback([this](const FontParameters &f) { return _spec->getLayout(f); });
	_formatter.reset(_spec->getData());
	if (w > 0.0f) {
		_formatter.setWidth(static_cast<uint16_t>(roundf(w * _density)));
	}
	return true;
}

void LabelBase::ExternalFormatter::setLineHeightAbsolute(float value) {
	_formatter.setLineHeightAbsolute(static_cast<uint16_t>(value * _density));
}

void LabelBase::ExternalFormatter::setLineHeightRelative(float value) {
	_formatter.setLineHeightRelative(value);
}

void LabelBase::ExternalFormatter::reserve(size_t chars, size_t ranges) {
	if (_spec) {
		_spec->reserve(chars, ranges);
	}
}

void LabelBase::ExternalFormatter::addString(const DescriptionStyle &style, const StringView &str,
		bool localized) {
	addString(style, string::toUtf16<Interface>(str), localized);
}
void LabelBase::ExternalFormatter::addString(const DescriptionStyle &style,
		const WideStringView &str, bool localized) {
	if (!begin) {
		_formatter.begin(0, 0);
		begin = true;
	}
	if (localized && locale::hasLocaleTags(str)) {
		auto u16str = locale::resolveLocaleTags(str);
		_formatter.read(style.font, style.text, u16str.data(), u16str.size());
	} else {
		_formatter.read(style.font, style.text, str.data(), str.size());
	}
}

Size2 LabelBase::ExternalFormatter::finalize() {
	if (_spec) {
		_formatter.finalize();
		return Size2(_spec->getWidth() / _density, _spec->getHeight() / _density);
	}
	return Size2();
}

WideString LabelBase::getLocalizedString(const StringView &s) {
	return getLocalizedString(string::toUtf16<Interface>(s));
}
WideString LabelBase::getLocalizedString(const WideStringView &s) {
	if (locale::hasLocaleTags(s)) {
		return locale::resolveLocaleTags(s);
	}
	return s.str<Interface>();
}

float LabelBase::getStringWidth(font::FontController *source, const DescriptionStyle &style,
		const StringView &str, bool localized) {
	return getStringWidth(source, style, string::toUtf16<Interface>(str), localized);
}

float LabelBase::getStringWidth(font::FontController *source, const DescriptionStyle &style,
		const WideStringView &str, bool localized) {
	if (!source) {
		return 0.0f;
	}

	TextLayoutData<Interface> spec;

	font::Formatter fmt([source = Rc<font::FontController>(source)](const FontParameters &f) {
		return source->getLayout(f);
	}, &spec);
	fmt.begin(0, 0);

	if (localized && locale::hasLocaleTags(str)) {
		auto u16str = locale::resolveLocaleTags(str);
		spec.reserve(u16str.size());
		fmt.read(style.font, style.text, u16str.data(), u16str.size());
	} else {
		spec.reserve(str.size());
		fmt.read(style.font, style.text, str.data(), str.size());
	}

	fmt.finalize();
	return spec.width / style.font.density;
}

Size2 LabelBase::getLabelSize(font::FontController *source, const DescriptionStyle &style,
		const StringView &s, float w, bool localized) {
	return getLabelSize(source, style, string::toUtf16<Interface>(s), w, localized);
}

Size2 LabelBase::getLabelSize(font::FontController *source, const DescriptionStyle &style,
		const WideStringView &str, float w, bool localized) {
	if (str.empty()) {
		return Size2(0.0f, 0.0f);
	}

	if (!source) {
		return Size2(0.0f, 0.0f);
	}

	TextLayoutData<Interface> spec;

	font::Formatter fmt([source = Rc<font::FontController>(source)](const FontParameters &f) {
		return source->getLayout(f);
	}, &spec);
	fmt.setWidth(static_cast<uint16_t>(roundf(w * style.font.density)));
	fmt.begin(0, 0);

	if (localized && locale::hasLocaleTags(str)) {
		auto u16str = locale::resolveLocaleTags(str);
		spec.reserve(u16str.size());
		fmt.read(style.font, style.text, u16str.data(), u16str.size());
	} else {
		spec.reserve(str.size());
		fmt.read(style.font, style.text, str.data(), str.size());
	}

	fmt.finalize();
	return Size2(spec.maxAdvance / style.font.density, spec.height / style.font.density);
}

void LabelBase::setCommonStyle(const DescriptionStyle &style) {
	if (_style != style) {
		_style = style;
		setLabelDirty();
	}
}

const LabelBase::DescriptionStyle &LabelBase::getCommonStyle() const { return _style; }

void LabelBase::setAlignment(TextAlign alignment) {
	if (_alignment != alignment) {
		_alignment = alignment;
		setLabelDirty();
	}
}

TextAlign LabelBase::getAlignment() const { return _alignment; }

void LabelBase::setWidth(float width) {
	if (_width != width) {
		_width = width;
		setLabelDirty();
	}
}

float LabelBase::getWidth() const { return _width; }

void LabelBase::setTextIndent(float value) {
	if (_textIndent != value) {
		_textIndent = value;
		setLabelDirty();
	}
}
float LabelBase::getTextIndent() const { return _textIndent; }

void LabelBase::setTextTransform(const TextTransform &value) {
	if (value != _style.text.textTransform) {
		_style.text.textTransform = value;
		setLabelDirty();
	}
}

TextTransform LabelBase::getTextTransform() const { return _style.text.textTransform; }

void LabelBase::setTextDecoration(const TextDecoration &value) {
	if (value != _style.text.textDecoration) {
		_style.text.textDecoration = value;
		setLabelDirty();
	}
}
TextDecoration LabelBase::getTextDecoration() const { return _style.text.textDecoration; }

void LabelBase::setHyphens(const Hyphens &value) {
	if (value != _style.text.hyphens) {
		_style.text.hyphens = value;
		setLabelDirty();
	}
}
Hyphens LabelBase::getHyphens() const { return _style.text.hyphens; }

void LabelBase::setVerticalAlign(const VerticalAlign &value) {
	if (value != _style.text.verticalAlign) {
		_style.text.verticalAlign = value;
		setLabelDirty();
	}
}
VerticalAlign LabelBase::getVerticalAlign() const { return _style.text.verticalAlign; }

void LabelBase::setFontSize(const uint16_t &value) { setFontSize(FontSize(value)); }

void LabelBase::setFontSize(const FontSize &value) {
	auto realTargetValue = value.scale(_labelDensity).get();
	auto realSourceValue = _style.font.fontSize.scale(_labelDensity).get();

	if (realTargetValue != realSourceValue) {
		_style.font.fontSize = value;
		setLabelDirty();
	}
}
FontSize LabelBase::getFontSize() const { return _style.font.fontSize; }

void LabelBase::setFontStyle(const FontStyle &value) {
	if (value != _style.font.fontStyle) {
		_style.font.fontStyle = value;
		setLabelDirty();
	}
}
FontStyle LabelBase::getFontStyle() const { return _style.font.fontStyle; }

void LabelBase::setFontWeight(const FontWeight &value) {
	if (value != _style.font.fontWeight) {
		_style.font.fontWeight = value;
		setLabelDirty();
	}
}
FontWeight LabelBase::getFontWeight() const { return _style.font.fontWeight; }

void LabelBase::setFontStretch(const FontStretch &value) {
	if (value != _style.font.fontStretch) {
		_style.font.fontStretch = value;
		setLabelDirty();
	}
}
FontStretch LabelBase::getFontStretch() const { return _style.font.fontStretch; }

void LabelBase::setFontGrade(const FontGrade &value) {
	if (value != _style.font.fontGrade) {
		_style.font.fontGrade = value;
		setLabelDirty();
	}
}

FontGrade LabelBase::getFontGrade() const { return _style.font.fontGrade; }

void LabelBase::setFontFamily(const StringView &value) {
	if (value != _style.font.fontFamily) {
		_fontFamilyStorage = value.str<Interface>();
		_style.font.fontFamily = _fontFamilyStorage;
		setLabelDirty();
	}
}
StringView LabelBase::getFontFamily() const { return _style.font.fontFamily; }

void LabelBase::setLineHeightAbsolute(float value) {
	if (!_isLineHeightAbsolute || _lineHeight != value) {
		_isLineHeightAbsolute = true;
		_lineHeight = value;
		setLabelDirty();
	}
}
void LabelBase::setLineHeightRelative(float value) {
	if (_isLineHeightAbsolute || _lineHeight != value) {
		_isLineHeightAbsolute = false;
		_lineHeight = value;
		setLabelDirty();
	}
}
float LabelBase::getLineHeight() const { return _lineHeight; }
bool LabelBase::isLineHeightAbsolute() const { return _isLineHeightAbsolute; }

void LabelBase::setMaxWidth(float value) {
	if (_maxWidth != value) {
		_maxWidth = value;
		setLabelDirty();
	}
}
float LabelBase::getMaxWidth() const { return _maxWidth; }

void LabelBase::setMaxLines(size_t value) {
	if (_maxLines != value) {
		_maxLines = value;
		setLabelDirty();
	}
}
size_t LabelBase::getMaxLines() const { return _maxLines; }

void LabelBase::setMaxChars(size_t value) {
	if (_maxChars != value) {
		_maxChars = value;
		setLabelDirty();
	}
}
size_t LabelBase::getMaxChars() const { return _maxChars; }

void LabelBase::setOpticalAlignment(bool value) {
	if (_opticalAlignment != value) {
		_opticalAlignment = value;
		setLabelDirty();
	}
}
bool LabelBase::isOpticallyAligned() const { return _opticalAlignment; }

void LabelBase::setFillerChar(char32_t c) {
	if (c != _fillerChar) {
		_fillerChar = c;
		setLabelDirty();
	}
}
char32_t LabelBase::getFillerChar() const { return _fillerChar; }

void LabelBase::setLocaleEnabled(bool value) {
	if (_localeEnabled != value) {
		_localeEnabled = value;
		setLabelDirty();
	}
}
bool LabelBase::isLocaleEnabled() const { return _localeEnabled; }

void LabelBase::setPersistentGlyphData(bool value) {
	if (_persistentGlyphData != value) {
		_persistentGlyphData = value;
		setLabelDirty();
	}
}

bool LabelBase::isPersistentGlyphData() const { return _persistentGlyphData; }

void LabelBase::setString(const StringView &newString) {
	if (newString == _string8) {
		return;
	}

	_string8 = newString.str<Interface>();
	_string16 = string::toUtf16<Interface>(newString);
	if (!_localeEnabled && locale::hasLocaleTagsFast(_string16)) {
		setLocaleEnabled(true);
	}
	setLabelDirty();
	clearStyles();
}

void LabelBase::setString(const WideStringView &newString) {
	if (newString == _string16) {
		return;
	}

	_string8 = string::toUtf8<Interface>(newString);
	_string16 = newString.str<Interface>();
	if (!_localeEnabled && locale::hasLocaleTagsFast(_string16)) {
		setLocaleEnabled(true);
	}
	setLabelDirty();
	clearStyles();
}

void LabelBase::setLocalizedString(size_t idx) {
	setString(localeIndex(idx));
	setLocaleEnabled(true);
}

WideStringView LabelBase::getString() const { return _string16; }

StringView LabelBase::getString8() const { return _string8; }

void LabelBase::erase16(size_t start, size_t len) {
	if (start >= _string16.length()) {
		return;
	}

	_string16.erase(start, len);
	_string8 = string::toUtf8<Interface>(_string16);
	setLabelDirty();
}

void LabelBase::erase8(size_t start, size_t len) {
	if (start >= _string8.length()) {
		return;
	}

	_string8.erase(start, len);
	_string16 = string::toUtf16<Interface>(_string8);
	setLabelDirty();
}

void LabelBase::append(const StringView &value) {
	_string8.append(value.str<Interface>());
	_string16 = string::toUtf16<Interface>(_string8);
	setLabelDirty();
}
void LabelBase::append(const WideStringView &value) {
	_string16.append(value.str<Interface>());
	_string8 = string::toUtf8<Interface>(_string16);
	setLabelDirty();
}

void LabelBase::prepend(const StringView &value) {
	_string8 = toString(value, _string8);
	_string16 = string::toUtf16<Interface>(_string8);
	setLabelDirty();
}
void LabelBase::prepend(const WideStringView &value) {
	_string16 = value.str<Interface>() + _string16;
	_string8 = string::toUtf8<Interface>(_string16);
	setLabelDirty();
}

void LabelBase::setTextRangeStyle(size_t start, size_t length, Style &&style) {
	if (length > 0) {
		_styles.emplace_back(start, length, sp::move(style));
		setLabelDirty();
	}
}

void LabelBase::appendTextWithStyle(const StringView &str, Style &&style) {
	auto start = _string16.length();
	append(str);
	setTextRangeStyle(start, _string16.length() - start, sp::move(style));
}

void LabelBase::appendTextWithStyle(const WideStringView &str, Style &&style) {
	auto start = _string16.length();
	append(str);
	setTextRangeStyle(start, str.size(), sp::move(style));
}

void LabelBase::prependTextWithStyle(const StringView &str, Style &&style) {
	auto len = _string16.length();
	prepend(str);
	setTextRangeStyle(0, _string16.length() - len, sp::move(style));
}
void LabelBase::prependTextWithStyle(const WideStringView &str, Style &&style) {
	prepend(str);
	setTextRangeStyle(0, str.size(), sp::move(style));
}

void LabelBase::clearStyles() {
	_styles.clear();
	setLabelDirty();
}

const LabelBase::StyleVec &LabelBase::getStyles() const { return _styles; }

const LabelBase::StyleVec &LabelBase::getCompiledStyles() const { return _compiledStyles; }

void LabelBase::setStyles(StyleVec &&vec) {
	_styles = sp::move(vec);
	setLabelDirty();
}
void LabelBase::setStyles(const StyleVec &vec) {
	_styles = vec;
	setLabelDirty();
}

bool LabelBase::updateFormatSpec(TextLayout *format, const StyleVec &compiledStyles, float density,
		uint8_t _adjustValue) {
	bool success = true;
	uint16_t adjustValue = maxOf<uint16_t>();

	do {
		if (adjustValue == maxOf<uint16_t>()) {
			adjustValue = 0;
		} else {
			adjustValue += 1;
		}

		format->clear();

		font::Formatter formatter([format](const FontParameters &f) {
			return format->getLayout(f);
		}, format->getData());
		formatter.setWidth(static_cast<uint16_t>(roundf(_width * density)));
		formatter.setTextAlignment(_alignment);
		formatter.setMaxWidth(static_cast<uint16_t>(roundf(_maxWidth * density)));
		formatter.setMaxLines(_maxLines);
		formatter.setOpticalAlignment(_opticalAlignment);
		formatter.setFillerChar(_fillerChar);
		formatter.setEmplaceAllChars(_emplaceAllChars);

		if (_lineHeight != 0.0f) {
			if (_isLineHeightAbsolute) {
				formatter.setLineHeightAbsolute(static_cast<uint16_t>(_lineHeight * density));
			} else {
				formatter.setLineHeightRelative(_lineHeight);
			}
		}

		formatter.begin(static_cast<uint16_t>(roundf(_textIndent * density)));

		size_t drawedChars = 0;
		for (auto &it : compiledStyles) {
			DescriptionStyle params = _style.merge(
					dynamic_cast<font::FontController *>(format->getController()), it.style);
			specializeStyle(params, density);
			if (adjustValue > 0) {
				params.font.fontSize -= FontSize(adjustValue);
			}

			auto start = _string16.c_str() + it.start;
			auto len = it.length;

			if (_localeEnabled && hasLocaleTags(WideStringView(start, len))) {
				WideString str(resolveLocaleTags(WideStringView(start, len)));

				start = str.c_str();
				len = str.length();

				if (_maxChars > 0 && drawedChars + len > _maxChars) {
					len = _maxChars - drawedChars;
				}
				if (!formatter.read(params.font, params.text, start, len)) {
					drawedChars += len;
					success = false;
					break;
				}
			} else {
				if (_maxChars > 0 && drawedChars + len > _maxChars) {
					len = _maxChars - drawedChars;
				}
				if (!formatter.read(params.font, params.text, start, len)) {
					drawedChars += len;
					success = false;
					break;
				}
			}

			if (!format->getData()->ranges.empty()) {
				format->getData()->ranges.back().colorDirty = params.colorDirty;
				format->getData()->ranges.back().opacityDirty = params.opacityDirty;
			}
		}
		formatter.finalize();
	} while (format->isOverflow() && adjustValue < _adjustValue);

	return success;
}

LabelBase::~LabelBase() { }

bool LabelBase::isLabelDirty() const { return _labelDirty; }

static void LabelBase_dumpStyle(LabelBase::StyleVec &ret, size_t pos, size_t len,
		const LabelBase::Style &style) {
	if (len > 0) {
		ret.emplace_back(pos, len, style);
	}
}

LabelBase::StyleVec LabelBase::compileStyle() const {
	size_t pos = 0;
	size_t max = _string16.length();

	StyleVec ret;
	StyleVec vec = _styles;

	Style compiledStyle;

	size_t dumpPos = 0;

	for (pos = 0; pos < max; pos++) {
		auto it = vec.begin();

		bool cleaned = false;
		// check for endings
		while (it != vec.end()) {
			if (it->start + it->length <= pos) {
				LabelBase_dumpStyle(ret, dumpPos, pos - dumpPos, compiledStyle);
				compiledStyle.clear();
				cleaned = true;
				dumpPos = pos;
				it = vec.erase(it);
			} else {
				it++;
			}
		}

		// checks for continuations and starts
		it = vec.begin();
		while (it != vec.end()) {
			if (it->start == pos) {
				if (dumpPos != pos) {
					LabelBase_dumpStyle(ret, dumpPos, pos - dumpPos, compiledStyle);
					dumpPos = pos;
				}
				compiledStyle.merge(it->style);
			} else if (it->start <= pos && it->start + it->length > pos) {
				if (cleaned) {
					compiledStyle.merge(it->style);
				}
			}
			it++;
		}
	}

	LabelBase_dumpStyle(ret, dumpPos, pos - dumpPos, compiledStyle);

	return ret;
}

bool LabelBase::hasLocaleTags(const WideStringView &str) const {
	return locale::hasLocaleTags(str);
}

WideString LabelBase::resolveLocaleTags(const WideStringView &str) const {
	return locale::resolveLocaleTags(str);
}

void LabelBase::specializeStyle(DescriptionStyle &style, float density) const {
	style.font.density = density;
	style.font.persistent = _persistentGlyphData;
}

void LabelBase::setLabelDirty() { _labelDirty = true; }

} // namespace stappler::xenolith::font
