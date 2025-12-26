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

#ifndef XENOLITH_FONT_XLFONTLABELBASE_H_
#define XENOLITH_FONT_XLFONTLABELBASE_H_

#include "XLContextInfo.h"
#include "XLFontController.h"
#include "SPMetastring.h"
#include "SPFontFormatter.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class SP_PUBLIC TextLayout : public Ref, public InterfaceObject<memory::StandartInterface> {
public:
	virtual ~TextLayout();
	TextLayout(FontController *h, size_t = 0, size_t = 0);

	void reserve(size_t, size_t = 1);
	void clear();

	bool empty() const { return _data.chars.empty(); }

	TextLayoutData<Interface> *getData() { return &_data; }
	const TextLayoutData<Interface> *getData() const { return &_data; }

	uint16_t getWidth() const { return _data.width; }
	uint16_t getHeight() const { return _data.height; }
	uint16_t getMaxAdvance() const { return _data.maxAdvance; }
	bool isOverflow() const { return _data.overflow; }

	FontController *getController() const { return _handle; }

	Rc<FontFaceSet> getLayout(const FontParameters &f);

	RangeLineIterator begin() const;
	RangeLineIterator end() const;

	WideString str(bool filterAlign = true) const;
	WideString str(uint32_t, uint32_t, size_t maxWords = maxOf<size_t>(), bool ellipsis = true,
			bool filterAlign = true) const;

	// on error maxOf<uint32_t> returned
	Pair<uint32_t, CharSelectMode> getChar(int32_t x, int32_t y,
			CharSelectMode = CharSelectMode::Center) const;
	const LineLayoutData *getLine(uint32_t charIndex) const;
	uint32_t getLineForChar(uint32_t charIndex) const;

	Pair<uint32_t, uint32_t> selectWord(uint32_t originChar) const;

	geom::Rect getLineRect(uint32_t lineId, float density, const geom::Vec2 & = geom::Vec2()) const;
	geom::Rect getLineRect(const LineLayoutData &, float density,
			const geom::Vec2 & = geom::Vec2()) const;

	uint16_t getLineForCharId(uint32_t id) const;

	Vector<geom::Rect> getLabelRects(uint32_t first, uint32_t last, float density,
			const geom::Vec2 & = geom::Vec2(), const geom::Padding &p = geom::Padding()) const;

	void getLabelRects(Vector<geom::Rect> &, uint32_t first, uint32_t last, float density,
			const geom::Vec2 & = geom::Vec2(), const geom::Padding &p = geom::Padding()) const;

protected:
	TextLayoutData<memory::StandartInterface> _data;
	Rc<FontController> _handle;
	Set<Rc<FontFaceSet>> _fonts;
};

class SP_PUBLIC LabelBase {
public:
	using FontFamily = ValueWrapper<uint32_t, class FontFamilyTag>;
	using Opacity = OpacityValue;

	struct Style {
		enum class Name : uint16_t {
			TextTransform,
			TextDecoration,
			Hyphens,
			VerticalAlign,
			Color,
			Opacity,
			FontSize,
			FontStyle,
			FontWeight,
			FontStretch,
			FontFamily,
			FontGrade,
		};

		union Value {
			TextTransform textTransform;
			TextDecoration textDecoration;
			Hyphens hyphens;
			VerticalAlign verticalAlign;
			Color3B color;
			uint8_t opacity;
			FontSize fontSize;
			FontStyle fontStyle;
			FontWeight fontWeight;
			FontStretch fontStretch;
			FontGrade fontGrade;
			uint32_t fontFamily;

			Value();
		};

		struct Param {
			Name name;
			Value value;

			Param(const TextTransform &val) : name(Name::TextTransform) {
				value.textTransform = val;
			}
			Param(const TextDecoration &val) : name(Name::TextDecoration) {
				value.textDecoration = val;
			}
			Param(const Hyphens &val) : name(Name::Hyphens) { value.hyphens = val; }
			Param(const VerticalAlign &val) : name(Name::VerticalAlign) {
				value.verticalAlign = val;
			}
			Param(const Color3B &val) : name(Name::Color) { value.color = val; }
			Param(const Color &val) : name(Name::Color) { value.color = val; }
			Param(const Opacity &val) : name(Name::Opacity) { value.opacity = val.get(); }
			Param(const FontSize &val) : name(Name::FontSize) { value.fontSize = val; }
			Param(const FontStyle &val) : name(Name::FontStyle) { value.fontStyle = val; }
			Param(const FontWeight &val) : name(Name::FontWeight) { value.fontWeight = val; }
			Param(const FontStretch &val) : name(Name::FontStretch) { value.fontStretch = val; }
			Param(const FontFamily &val) : name(Name::FontFamily) { value.fontFamily = val.get(); }
			Param(const FontGrade &val) : name(Name::FontGrade) { value.fontGrade = val; }
		};

		Style() { }
		Style(const Style &) = default;
		Style(Style &&) = default;
		Style &operator=(const Style &) = default;
		Style &operator=(Style &&) = default;

		Style(std::initializer_list<Param> il) : params(il) { }

		template <class T>
		Style(const T &value) {
			params.push_back(value);
		}
		template <class T>
		Style &set(const T &value) {
			set(Param(value), true);
			return *this;
		}

		void set(const Param &, bool force = false);
		void merge(const Style &);
		void clear();

		Vector<Param> params;
	};

	struct StyleSpec {
		size_t start = 0;
		size_t length = 0;
		Style style;

		StyleSpec(size_t s, size_t l, Style &&style)
		: start(s), length(l), style(sp::move(style)) { }

		StyleSpec(size_t s, size_t l, const Style &style) : start(s), length(l), style(style) { }
	};

	struct DescriptionStyle {
		FontParameters font;
		TextParameters text;

		bool colorDirty = false;
		bool opacityDirty = false;
		uint32_t tag = 0;

		DescriptionStyle();

		String getConfigName(bool caps) const;

		DescriptionStyle merge(const Rc<font::FontController> &, const Style &style) const;

		bool operator==(const DescriptionStyle &) const;
		bool operator!=(const DescriptionStyle &) const;

		template <typename... Args>
		static DescriptionStyle construct(const StringView &family, FontSize size, Args &&...args) {
			DescriptionStyle p;
			p.font.fontFamily = family;
			p.font.fontSize = size;
			readParameters(p, std::forward<Args>(args)...);
			return p;
		}

		static void readParameter(DescriptionStyle &p, TextTransform value) {
			p.text.textTransform = value;
		}
		static void readParameter(DescriptionStyle &p, TextDecoration value) {
			p.text.textDecoration = value;
		}
		static void readParameter(DescriptionStyle &p, Hyphens value) { p.text.hyphens = value; }
		static void readParameter(DescriptionStyle &p, VerticalAlign value) {
			p.text.verticalAlign = value;
		}
		static void readParameter(DescriptionStyle &p, Opacity value) {
			p.text.opacity = value.get();
		}
		static void readParameter(DescriptionStyle &p, const Color3B &value) {
			p.text.color = value;
		}
		static void readParameter(DescriptionStyle &p, FontSize value) { p.font.fontSize = value; }
		static void readParameter(DescriptionStyle &p, FontStyle value) {
			p.font.fontStyle = value;
		}
		static void readParameter(DescriptionStyle &p, FontWeight value) {
			p.font.fontWeight = value;
		}
		static void readParameter(DescriptionStyle &p, FontStretch value) {
			p.font.fontStretch = value;
		}
		static void readParameter(DescriptionStyle &p, FontGrade value) {
			p.font.fontGrade = value;
		}

		template <typename T, typename... Args>
		static void readParameters(DescriptionStyle &p, T &&t, Args &&...args) {
			readParameter(p, t);
			readParameters(p, std::forward<Args>(args)...);
		}

		template <typename T>
		static void readParameters(DescriptionStyle &p, T &&t) {
			readParameter(p, t);
		}

		static void readParameters(DescriptionStyle &p) { }
	};

	class ExternalFormatter : public Ref {
	public:
		virtual ~ExternalFormatter();

		bool init(font::FontController *, float w = 0.0f, float density = 0.0f);

		void setLineHeightAbsolute(float value);
		void setLineHeightRelative(float value);

		void reserve(size_t chars, size_t ranges = 1);

		void addString(const DescriptionStyle &, const StringView &, bool localized = false);
		void addString(const DescriptionStyle &, const WideStringView &, bool localized = false);

		Size2 finalize();

	protected:
		bool begin = false;
		Rc<font::TextLayout> _spec;
		font::Formatter _formatter;
		float _density = 1.0f;
	};

	using StyleVec = Vector< StyleSpec >;

public:
	static WideString getLocalizedString(const StringView &);
	static WideString getLocalizedString(const WideStringView &);

	static Size2 getLabelSize(font::FontController *, const DescriptionStyle &, const StringView &,
			float w = 0.0f, bool localized = false);
	static Size2 getLabelSize(font::FontController *, const DescriptionStyle &,
			const WideStringView &, float w = 0.0f, bool localized = false);

	static float getStringWidth(font::FontController *, const DescriptionStyle &,
			const StringView &, bool localized = false);
	static float getStringWidth(font::FontController *, const DescriptionStyle &,
			const WideStringView &, bool localized = false);

	virtual ~LabelBase();

	virtual bool isLabelDirty() const;
	virtual StyleVec compileStyle() const;

	template <char... Chars>
	void setString(metastring::metastring<Chars...> &&str) {
		setString(str.to_std_string());
	}

	virtual void setString(const StringView &);
	virtual void setString(const WideStringView &newString);
	virtual void setLocalizedString(size_t);
	virtual WideStringView getString() const;
	virtual StringView getString8() const;

	virtual void erase16(size_t start = 0, size_t len = WideString::npos);
	virtual void erase8(size_t start = 0, size_t len = String::npos);

	virtual void append(const StringView &value);
	virtual void append(const WideStringView &value);

	virtual void prepend(const StringView &value);
	virtual void prepend(const WideStringView &value);

	virtual void setTextRangeStyle(size_t start, size_t length, Style &&);

	virtual void appendTextWithStyle(const StringView &, Style &&);
	virtual void appendTextWithStyle(const WideStringView &, Style &&);

	virtual void prependTextWithStyle(const StringView &, Style &&);
	virtual void prependTextWithStyle(const WideStringView &, Style &&);

	virtual void clearStyles();

	virtual const StyleVec &getStyles() const;
	virtual const StyleVec &getCompiledStyles() const;

	virtual void setStyles(StyleVec &&);
	virtual void setStyles(const StyleVec &);

	virtual bool updateFormatSpec(TextLayout *, const StyleVec &, float density,
			uint8_t adjustValue);

	virtual bool empty() const { return _string16.empty(); }

	void setCommonStyle(const DescriptionStyle &);
	const DescriptionStyle &getCommonStyle() const;

	void setAlignment(TextAlign alignment);
	TextAlign getAlignment() const;

	// line width for line wrapping
	void setWidth(float width);
	float getWidth() const;

	void setTextIndent(float value);
	float getTextIndent() const;

	void setTextTransform(const TextTransform &);
	TextTransform getTextTransform() const;

	void setTextDecoration(const TextDecoration &);
	TextDecoration getTextDecoration() const;

	void setHyphens(const Hyphens &);
	Hyphens getHyphens() const;

	void setVerticalAlign(const VerticalAlign &);
	VerticalAlign getVerticalAlign() const;

	void setFontSize(const uint16_t &);
	void setFontSize(const FontSize &);
	FontSize getFontSize() const;

	void setFontStyle(const FontStyle &);
	FontStyle getFontStyle() const;

	void setFontWeight(const FontWeight &);
	FontWeight getFontWeight() const;

	void setFontStretch(const FontStretch &);
	FontStretch getFontStretch() const;

	void setFontGrade(const FontGrade &);
	FontGrade getFontGrade() const;

	void setFontFamily(const StringView &);
	StringView getFontFamily() const;

	void setLineHeightAbsolute(float value);
	void setLineHeightRelative(float value);
	float getLineHeight() const;
	bool isLineHeightAbsolute() const;

	// line width for truncating label without wrapping
	void setMaxWidth(float);
	float getMaxWidth() const;

	void setMaxLines(size_t);
	size_t getMaxLines() const;

	void setMaxChars(size_t);
	size_t getMaxChars() const;

	void setOpticalAlignment(bool value);
	bool isOpticallyAligned() const;

	void setFillerChar(char32_t);
	char32_t getFillerChar() const;

	void setLocaleEnabled(bool);
	bool isLocaleEnabled() const;

	// Mark label's glyphs as persistent within GPU-side font cache
	// Useful for a static menu labels or persistent window names.
	//
	// Persistent glyphs can not be dropped from cache, so, no glyph rendering required when
	// label was removed from scene, then added again
	void setPersistentGlyphData(bool);
	bool isPersistentGlyphData() const;

protected:
	virtual bool hasLocaleTags(const WideStringView &) const;
	virtual WideString resolveLocaleTags(const WideStringView &) const;

	virtual void specializeStyle(DescriptionStyle &style, float density) const;

	virtual void setLabelDirty();

	WideString _string16;
	String _string8;

	float _width = 0.0f;
	float _textIndent = 0.0f;
	float _labelDensity = 1.0f;

	TextAlign _alignment = TextAlign::Left;

	bool _localeEnabled = false;
	bool _labelDirty = true;

	bool _isLineHeightAbsolute = false;
	float _lineHeight = 0;

	String _fontFamilyStorage;
	DescriptionStyle _style;
	StyleVec _styles;
	StyleVec _compiledStyles;

	uint16_t _charsWidth;
	uint16_t _charsHeight;

	float _maxWidth = 0.0f;
	size_t _maxLines = 0;
	size_t _maxChars = 0;

	bool _opticalAlignment = false;
	bool _emplaceAllChars = false;
	char32_t _fillerChar = u'…';

	bool _persistentGlyphData = false;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTLABELBASE_H_ */
