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

class SP_PUBLIC TextLayout : public Ref, public InterfaceObject<mem_std::Interface> {
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

	Rect getLineRect(uint32_t lineId, float density, const Vec2 & = Vec2()) const;
	Rect getLineRect(const LineLayoutData &, float density, const Vec2 & = Vec2()) const;

	uint16_t getLineForCharId(uint32_t id) const;

	Vector<Rect> getLabelRects(uint32_t first, uint32_t last, float density, const Vec2 & = Vec2(),
			const Padding &p = Padding()) const;

	/* WHERE A RESERVED BOX ENDED UP, by the index of the range that reserved it.

	A box is not a glyph: the formatter records it as one cell with no character and an advance of
	the requested width, plus a range of its own carrying the requested height. Nothing else in
	the layout remembers it, so the range index is the handle, and this is the only way back from
	it to a rectangle.

	Empty when the index names no range, or when the layout no longer holds the cell (a box that
	fell outside `maxLines`). The rectangle is in the same space `getLineRect` answers in. */
	Rect getObjectRect(uint32_t rangeIndex, float density, const Vec2 & = Vec2()) const;

	void getLabelRects(Vector<Rect> &, uint32_t first, uint32_t last, float density,
			const Vec2 & = Vec2(), const Padding &p = Padding()) const;

protected:
	TextLayoutData<mem_std::Interface> _data;
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

		Style(sprt::initializer_list<Param> il) : params(il) { }

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
			readParameters(p, sprt::forward<Args>(args)...);
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
			readParameters(p, sprt::forward<Args>(args)...);
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

	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	virtual ~LabelBase();
	__SPRT_POP_ALLOW_CXXABI_ALLOC

	virtual bool isLabelDirty() const;
	virtual StyleVec compileStyle() const;

	template <char... Chars>
	void setString(metastring::metastring<Chars...> &&str) {
		setString(StringView(str.template string<String>()));
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

	/* AN INLINE OBJECT: a box in the line where a glyph would be.

	The formatter can reserve a run of empty width and height instead of shaping a character
	(`Formatter::read(font, text, w, h)`), and a line breaks around that box like it would around
	a word. This is how something that is not text - an image - sits INSIDE a paragraph without
	cutting the paragraph into pieces.

	The object stands in for exactly ONE character of the string, which should be U+FFFC (OBJECT
	REPLACEMENT CHARACTER). One character, not zero, so that every index the label answers with
	keeps meaning the same thing: a caret can stand before or after the image, a selection can
	contain it, and whoever maps characters back to a source maps it to whatever wrote it.

	The label draws nothing for it. The caller draws over the box, asking `getObjectRect` for
	where it landed - which is knowable only after the text has been shaped and wrapped. */
	struct InlineObject {
		uint32_t charIndex = 0;
		Size2 size;

		// Filled in by the layout: the range the reservation produced, which is the handle
		// `TextLayout::getObjectRect` takes. maxOf when this object was not laid out.
		uint32_t rangeIndex = maxOf<uint32_t>();
	};

	// Ascending by `charIndex`; the caller keeps them so. Setting the string does not clear them,
	// because the two are written together and the string comes first.
	virtual void setInlineObjects(Vector<InlineObject> &&);
	const Vector<InlineObject> &getInlineObjects() const { return _inlineObjects; }
	virtual void clearInlineObjects();

	virtual const StyleVec &getStyles() const;
	virtual const StyleVec &getCompiledStyles() const;

	virtual void setStyles(StyleVec &&);
	virtual void setStyles(const StyleVec &);

	virtual bool updateFormatSpec(TextLayout *, const StyleVec &, float density,
			uint8_t adjustValue,
			font::Formatter::ContentRequest = font::Formatter::ContentRequest::Normal);

	virtual bool empty() const { return _string16.empty(); }

	void setCommonStyle(const DescriptionStyle &);
	const DescriptionStyle &getCommonStyle() const;

	void setAlignment(TextAlign alignment);
	TextAlign getAlignment() const;

	// base text direction (CSS `direction`) plus opt-in Unicode Bidirectional Algorithm (UAX #9) and
	// HarfBuzz shaping during layout
	void setTextDirection(TextDirection);
	TextDirection getTextDirection() const;
	void setBidiEnabled(bool);
	bool isBidiEnabled() const;
	void setShapingEnabled(bool);
	bool isShapingEnabled() const;
	void setBidiMode(BidiMode); // CSS `unicode-bidi`: Embed / Isolate / Override / Plaintext
	BidiMode getBidiMode() const;
	void setLetterSpacing(float); // CSS letter-spacing, in unscaled px
	float getLetterSpacing() const;
	void setWordSpacing(float); // CSS word-spacing, in unscaled px
	float getWordSpacing() const;
	void setLigaturesEnabled(bool); // font-variant-ligatures (false drops common ligatures)
	bool isLigaturesEnabled() const;

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

	// Latches: once called, `setString` stops deciding for itself. A widget that draws a person's own
	// text or a file's contents calls `setLocaleEnabled(false)` once and is done.
	void setLocaleEnabled(bool);
	bool isLocaleEnabled() const;

	// Mark label's glyphs as persistent within GPU-side font cache
	// Useful for a static menu labels or persistent window names.
	//
	// Persistent glyphs can not be dropped from cache, so, no glyph rendering required when
	// label was removed from scene, then added again
	void setPersistentGlyphData(bool);
	bool isPersistentGlyphData() const;

	// Effective label-wide layout inputs consumed by updateFormatSpec. The default
	// makeEffectiveStyle() mirrors the label's own stored fields (_style/_alignment/
	// _lineHeight); a subclass overrides it to overlay externally-provided values
	// (e.g. inherited style components) WITHOUT mutating the stored fields — the
	// stored explicit values stay intact and win again as soon as the overlay
	// source disappears.
	struct EffectiveStyle {
		DescriptionStyle style;
		TextAlign alignment = TextAlign::Left;

		/* The bidi settings IN FORCE, which is not always what the label was told.

		Three layers can have an opinion about a label's direction - the stylesheet, an explicit
		`setTextDirection` from the caller, and the locale - and they used to write the same field
		in whatever order they happened to run. They are resolved here instead, once, in that order
		of precedence: CSS wins, then the caller, then the locale's default. The stored members are
		never written by the cascade, so a sheet that stops declaring `direction` hands the label
		straight back to what its caller asked for. */
		TextDirection direction = TextDirection::LeftToRight;
		BidiMode bidiMode = BidiMode::Normal;
		bool bidiEnabled = false;
		bool shapingEnabled = false;

		float lineHeight = 0.0f;
		bool lineHeightAbsolute = false;
		// owning storage: when non-empty, updateFormatSpec re-points
		// style.font.fontFamily (a non-owning view) at it
		String fontFamilyStorage;
	};

protected:
	void enableLocaleIfTagged();

	virtual bool hasLocaleTags(const WideStringView &) const;
	virtual WideString resolveLocaleTags(const WideStringView &) const;

	virtual void specializeStyle(DescriptionStyle &style, float density) const;

	virtual void makeEffectiveStyle(EffectiveStyle &) const;

	virtual void setLabelDirty();

	/* A number that changes whenever a measurement of this label would answer differently.

	Everything that invalidates the shaping - the string, a style range, the font, an inherited
	component, the width - already goes through setLabelDirty, so this is the one key a cache of
	measured sizes can trust. */
	uint64_t getLabelRevision() const { return _labelRevision; }

	WideString _string16;
	String _string8;

	float _width = 0.0f;
	float _textIndent = 0.0f;
	float _labelDensity = 1.0f;

	TextAlign _alignment = TextAlign::Left;
	TextDirection _direction = TextDirection::LeftToRight;
	bool _bidiEnabled = false;
	bool _shapingEnabled = false;
	BidiMode _bidiMode = BidiMode::Normal;
	float _letterSpacing = 0.0f;
	float _wordSpacing = 0.0f;
	bool _enableLigatures = true;

	bool _localeEnabled = false;
	bool _localeAuto = true; // cleared by the first setLocaleEnabled() call, whichever way it went
	bool _labelDirty = true;
	uint64_t _labelRevision = 1;

	bool _isLineHeightAbsolute = false;
	float _lineHeight = 0;

	String _fontFamilyStorage;
	DescriptionStyle _style;
	StyleVec _styles;
	Vector<InlineObject> _inlineObjects;
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
