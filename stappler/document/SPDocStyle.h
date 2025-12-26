/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_DOCUMENT_SPDOCSTYLE_H_
#define CORE_DOCUMENT_SPDOCSTYLE_H_

#include "SPCore.h"
#include "SPFontStyle.h"
#include "SPColor.h"
#include "SPGeometry.h"
#include "SPPadding.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct StyleParameter;

using font::EnumSize;
using NameSize = uint16_t;

enum class StyleRule : EnumSize {
	None,
	Important
};

using MediaQueryId = ValueWrapper<uint16_t, class MediaQueryIdType>;
constexpr MediaQueryId MediaQueryIdNone = MediaQueryId(maxOf<uint16_t>());

using StringId = uint32_t;
constexpr StringId StringIdNone = maxOf<StringId>();

using StringCallback = Callback<StringId(const StringView &)>;
using StyleCallback = Callback<bool(StyleParameter &&)>;

using StyleFunctionPtr = bool (*)(const StringView &, const StyleCallback &,
		const StringCallback &);

using geom::Color3B;
using geom::Color4B;
using geom::Color;
using geom::Vec2;
using geom::Size2;
using geom::Padding;
using geom::Margin;
using geom::Rect;

using font::FontVariations;

using geom::Metric;
using font::FontStyle;
using font::FontWeight;
using font::FontStretch;
using font::FontSize;
using font::FontVariant;
using font::TextTransform;
using font::TextDecoration;
using font::TextAlign;
using font::WhiteSpace;
using font::Hyphens;
using font::VerticalAlign;
using font::ListStyleType;
using font::Autofit;
using font::FontVariableAxis;

enum class RenderFlags : uint32_t {
	None,
	NoImages = 1 << 0, // do not render images
	NoHeightCheck = 1 << 1, // disable re-rendering when viewport height is changed
	RenderById = 1 << 2, // render nodes by html id instead of spine name
	PaginatedLayout = 1 << 3, // render as book pages instead of html linear layout
	SplitPages = 1 << 4,
};

SP_DEFINE_ENUM_AS_MASK(RenderFlags)

enum class Display : EnumSize {
	None,
	Default,
	RunIn,
	Inline,
	InlineBlock,
	Block,
	ListItem,
	Table,
	TableCell,
	TableColumn,
	TableCaption,
};

enum class Float : EnumSize {
	None,
	Left,
	Right,
};

enum class Clear : EnumSize {
	None,
	Left,
	Right,
	Both,
};

enum class BackgroundRepeat : EnumSize {
	NoRepeat,
	Repeat,
	RepeatX,
	RepeatY,
};

enum class BorderStyle : EnumSize {
	None,
	Solid,
	Dotted,
	Dashed,
};

enum class MediaType : EnumSize {
	All,
	Screen,
	Print,
	Speech,
};

enum class Orientation : EnumSize {
	Landscape,
	Portrait,
};

enum class Pointer : EnumSize {
	Fine,
	Coarse,
	None,
};

enum class Hover : EnumSize {
	Hover,
	OnDemand,
	None,
};

enum class LightLevel : EnumSize {
	Normal,
	Dim,
	Washed,
};

enum class Scripting : EnumSize {
	None,
	InitialOnly,
	Enabled,
};

enum class ListStylePosition : EnumSize {
	Outside,
	Inside,
};

enum class PageBreak : EnumSize {
	Always,
	Auto,
	Avoid,
	Left,
	Right
};

enum class BorderCollapse : EnumSize {
	Separate,
	Collapse,
};

enum class CaptionSide : EnumSize {
	Top,
	Bottom,
};

enum class ParameterName : NameSize {
	/* css-selectors */

	Unknown = 0,

	__BeginCssParameters,
	CssFontStyle, // enum
	CssFontWeight, // enum
	CssFontSize, // enum
	CssFontVariant, // enum
	CssFontSizeIncrement, // metric
	CssFontStretch, // enum
	CssFontSizeNumeric, // size
	CssTextTransform, // enum
	CssTextDecoration, // enum
	CssTextAlign, // enum
	CssWhiteSpace, // enum
	CssHyphens, // enum
	CssDisplay, // enum
	CssFloat, // enum
	CssClear, // enum
	CssColor, // color
	CssOpacity, // opacity
	CssTextIndent, // size
	CssLineHeight, // size
	CssMarginTop, // size
	CssMarginRight, // size
	CssMarginBottom, // size
	CssMarginLeft, // size
	CssWidth, // size
	CssHeight, // size
	CssMinWidth, // size
	CssMinHeight, // size
	CssMaxWidth, // size
	CssMaxHeight, // size
	CssPaddingTop, // size
	CssPaddingRight, // size
	CssPaddingBottom, // size
	CssPaddingLeft, // size
	CssFontFamily, // string id
	CssBackgroundColor, // color4
	CssBackgroundImage, // string id
	CssBackgroundPositionX, // size
	CssBackgroundPositionY, // size
	CssBackgroundRepeat, // enum
	CssBackgroundSizeWidth, // size
	CssBackgroundSizeHeight, // size
	CssVerticalAlign, // enum
	CssBorderTopStyle, // enum
	CssBorderTopWidth, // size
	CssBorderTopColor, // color4
	CssBorderRightStyle, // enum
	CssBorderRightWidth, // size
	CssBorderRightColor, // color4
	CssBorderBottomStyle, // enum
	CssBorderBottomWidth, // size
	CssBorderBottomColor, // color4
	CssBorderLeftStyle, // enum
	CssBorderLeftWidth, // size
	CssBorderLeftColor, // color4
	CssOutlineStyle, // enum
	CssOutlineWidth, // size
	CssOutlineColor, // color4
	CssListStyleType, // enum
	CssListStylePosition, // enum
	CssXListStyleOffset, // size
	CssPageBreakBefore, // enum
	CssPageBreakAfter, // enum
	CssPageBreakInside, // enum
	CssAutofit, // enum
	CssBorderCollapse, // enum
	CssCaptionSide, // enum
	CssOrphans, // uint
	CssWidows, // uint
	__EndCssParameters,

	/* media - specific */
	__BeginCssMediaParameters,
	CssMediaType,
	CssMediaOrientation,
	CssMediaPointer,
	CssMediaHover,
	CssMediaLightLevel,
	CssMediaScripting,
	CssMediaAspectRatio,
	CssMediaMinAspectRatio,
	CssMediaMaxAspectRatio,
	CssMediaResolution,
	CssMediaMinResolution,
	CssMediaMaxResolution,
	CssMediaOption,
	__EndCssMediaParameters,
};

using FontStyleParameters = font::FontParameters;
using TextLayoutParameters = font::TextParameters;

struct SP_PUBLIC ParagraphLayoutParameters {
	TextAlign textAlign = TextAlign::Left;
	Metric textIndent;
	Metric lineHeight;
	Metric listOffset;

	inline bool operator==(const ParagraphLayoutParameters &other) const = default;
	inline bool operator!=(const ParagraphLayoutParameters &other) const = default;
};

struct SP_PUBLIC BlockModelParameters {
	Display display = Display::Default;
	Float floating = Float::None;
	Clear clear = Clear::None;
	ListStyleType listStyleType = ListStyleType::None;
	ListStylePosition listStylePosition = ListStylePosition::Outside;

	PageBreak pageBreakBefore = PageBreak::Auto;
	PageBreak pageBreakAfter = PageBreak::Auto;
	PageBreak pageBreakInside = PageBreak::Auto;
	BorderCollapse borderCollapse = BorderCollapse::Collapse;
	CaptionSide captionSide = CaptionSide::Top;

	Metric marginTop;
	Metric marginRight;
	Metric marginBottom;
	Metric marginLeft;

	Metric paddingTop;
	Metric paddingRight;
	Metric paddingBottom;
	Metric paddingLeft;

	Metric width;
	Metric height;
	Metric minWidth;
	Metric minHeight;
	Metric maxWidth;
	Metric maxHeight;

	uint32_t orphans = 2;
	uint32_t widows = 2;

	inline bool operator==(const BlockModelParameters &other) const = default;
	inline bool operator!=(const BlockModelParameters &other) const = default;
};

struct SP_PUBLIC InlineModelParameters {
	Metric marginRight;
	Metric marginLeft;
	Metric paddingRight;
	Metric paddingLeft;

	inline bool operator==(const InlineModelParameters &other) const = default;
	inline bool operator!=(const InlineModelParameters &other) const = default;
};

struct SP_PUBLIC BackgroundParameters {
	Display display = Display::Default;
	Color4B backgroundColor;
	BackgroundRepeat backgroundRepeat = BackgroundRepeat::NoRepeat;
	Metric backgroundPositionX;
	Metric backgroundPositionY;
	Metric backgroundSizeWidth;
	Metric backgroundSizeHeight;

	StringView backgroundImage;

	BackgroundParameters() = default;
	BackgroundParameters(Autofit);

	inline bool operator==(const BackgroundParameters &other) const = default;
	inline bool operator!=(const BackgroundParameters &other) const = default;
};

struct SP_PUBLIC OutlineParameters {
	struct Params {
		BorderStyle style = BorderStyle::None;
		Metric width;
		Color4B color;

		inline bool operator==(const Params &other) const = default;
		inline bool operator!=(const Params &other) const = default;
	};

	Params left;
	Params top;
	Params right;
	Params bottom;
	Params outline;

	inline bool operator==(const OutlineParameters &other) const = default;
	inline bool operator!=(const OutlineParameters &other) const = default;
};

class SP_PUBLIC StyleInterface {
public:
	virtual ~StyleInterface() = default;

	virtual bool resolveMediaQuery(MediaQueryId queryId) const = 0;
	virtual StringView resolveString(StringId) const = 0;

	virtual float getDensity() const = 0;
	virtual float getFontScale() const = 0;

	virtual Color4B getTextColor() const = 0;
};

class SP_PUBLIC SimpleStyleInterface : public StyleInterface {
public:
	virtual ~SimpleStyleInterface() = default;

	SimpleStyleInterface();
	SimpleStyleInterface(SpanView<bool>, SpanView<StringView>, float density, float fontScale);

	virtual bool resolveMediaQuery(MediaQueryId queryId) const override;
	virtual StringView resolveString(StringId) const override;

	virtual float getDensity() const override;
	virtual float getFontScale() const override;
	virtual Color4B getTextColor() const override;

protected:
	float _density = 1.0f;
	float _fontScale = 1.0f;
	SpanView<bool> _media;
	SpanView<StringView> _strings;
};

union SP_PUBLIC StyleValue {
	FontStyle fontStyle;
	FontWeight fontWeight;
	FontStretch fontStretch;
	FontVariant fontVariant;
	TextTransform textTransform;
	TextDecoration textDecoration;
	TextAlign textAlign;
	WhiteSpace whiteSpace;
	Hyphens hyphens;
	Display display;
	Float floating;
	Clear clear;
	MediaType mediaType;
	Orientation orientation;
	Pointer pointer;
	Hover hover;
	LightLevel lightLevel;
	Scripting scripting;
	BackgroundRepeat backgroundRepeat;
	VerticalAlign verticalAlign;
	BorderStyle borderStyle;
	ListStyleType listStyleType;
	ListStylePosition listStylePosition;
	PageBreak pageBreak;
	Autofit autofit;
	BorderCollapse borderCollapse;
	CaptionSide captionSide;
	Color3B color;
	FontSize fontSize;
	uint8_t opacity;
	Color4B color4;
	Metric sizeValue;
	float floatValue;
	uint32_t uintValue;
	int32_t intValue;
	StringId stringId;

	StyleValue();
};

struct SP_PUBLIC StyleParameter {
	ParameterName name = ParameterName::Unknown;
	MediaQueryId mediaQuery = MediaQueryIdNone;
	StyleValue value;
	StyleRule rule = StyleRule::None;

	template <ParameterName Name, class Value>
	static StyleParameter create(const Value &v, MediaQueryId query = MediaQueryIdNone,
			StyleRule r = StyleRule::None);

	StyleParameter() = default;
	StyleParameter(ParameterName n, MediaQueryId query, StyleRule r);
	StyleParameter(const StyleParameter &, MediaQueryId query, StyleRule r);

	template <ParameterName Name, class Value>
	void set(const Value &);
};

struct SP_PUBLIC StyleList : public memory::AllocPool {
	template <typename T>
	using Vector = memory::PoolInterface::VectorType<T>;

	using String = memory::PoolInterface::StringType;

	using StyleVec = Vector<Pair<String, String>>;

	static bool isInheritable(ParameterName name);

	template <ParameterName Name, class Value>
	void set(const Value &value, MediaQueryId mediaQuery = MediaQueryIdNone);
	void set(const StyleParameter &p, bool force = false);

	void merge(const StyleList &, bool inherit = false);
	void merge(const StyleList &, const SpanView<bool> &, bool inherit = false);

	Vector<StyleParameter> get(ParameterName) const;
	Vector<StyleParameter> get(ParameterName, const StyleInterface *) const;
	StyleParameter get(ParameterName, const Vector<bool> &) const;

	FontStyleParameters compileFontStyle(const StyleInterface *) const;
	TextLayoutParameters compileTextLayout(const StyleInterface *) const;
	ParagraphLayoutParameters compileParagraphLayout(const StyleInterface *) const;
	BlockModelParameters compileBlockModel(const StyleInterface *) const;
	InlineModelParameters compileInlineModel(const StyleInterface *) const;
	BackgroundParameters compileBackground(const StyleInterface *) const;
	OutlineParameters compileOutline(const StyleInterface *) const;

	bool operator==(const StyleList &other);
	bool operator!=(const StyleList &other);

	// for debug purposes - can write non-css values
	String css(const StyleInterface * = nullptr) const;

	Vector<StyleParameter> data;
};

struct SP_PUBLIC MediaQuery : public memory::AllocPool {
	template <typename T, typename V>
	using Map = memory::PoolInterface::MapType<T, V>;

	template <typename T>
	using Vector = memory::PoolInterface::VectorType<T>;

	using String = memory::PoolInterface::StringType;

	struct Query {
		bool negative = false;
		Vector<StyleParameter> params;

		bool setMediaType(StringView);
	};

	static constexpr MediaQueryId IsScreenLayout = MediaQueryId(0);
	static constexpr MediaQueryId IsPrintLayout = MediaQueryId(1);
	static constexpr MediaQueryId NoTooltipOption = MediaQueryId(2);
	static constexpr MediaQueryId IsTooltipOption = MediaQueryId(3);

	Vector<Query> list;

	void clear();

	explicit operator bool() const { return !list.empty(); }
};

struct SP_PUBLIC MediaParameters {
	template <typename T, typename V>
	using Map = memory::StandartInterface::MapType<T, V>;

	template <typename T>
	using Vector = memory::StandartInterface::VectorType<T>;

	using String = memory::StandartInterface::StringType;

	Size2 surfaceSize;

	int dpi = 92;
	float density = 1.0f;
	float fontScale = 1.0f;

	MediaType mediaType = MediaType::Screen;
	Orientation orientation = Orientation::Landscape;
	Pointer pointer = Pointer::Coarse;
	Hover hover = Hover::None;
	LightLevel lightLevel = LightLevel::Normal;
	Scripting scripting = Scripting::None;
	Margin pageMargin;
	Color4B defaultBackground = Color4B(0xfa, 0xfa, 0xfa, 255);

	RenderFlags flags = RenderFlags::None;

	Map<StringId, String> _options;

	float getDefaultFontSize() const;

	void addOption(const StringView &);
	void removeOption(const StringView &);
	bool hasOption(const StringView &) const;
	bool hasOption(StringId) const;

	bool resolveQuery(const MediaQuery &) const;

	template <typename Interface>
	auto resolveMediaQueries(const SpanView<MediaQuery> &) const ->
			typename Interface::template VectorType<bool>;

	bool shouldRenderImages() const;

	float computeValueStrong(Metric, float base, float fontSize = nan()) const;
	float computeValueAuto(Metric, float base, float fontSize = nan()) const;
};

struct SP_PUBLIC FontFace {
	template <typename T, typename V>
	using Map = memory::PoolInterface::MapType<T, V>;

	template <typename T>
	using Vector = memory::PoolInterface::VectorType<T>;

	using String = memory::PoolInterface::StringType;

	struct FontFaceSource {
		String url;
		String format;
		String tech;
		bool isLocal = false;
	};

	String fontFamily;
	FontVariations variations;
	Vector<FontFaceSource> src;
	Vector<StyleParameter> style;

	String getConfigName(const StringView &family, FontSize size) const;

	FontStyleParameters getStyle(const StringView &family, FontSize size) const;

	FontFace() = default;
};

SP_PUBLIC void getStyleForTag(StyleList &list, StringView, StringView parent = StringView());

/*String getFontConfigName(const StringView &, FontSize, FontStyle, FontWeight, FontStretch, FontVariant, bool caps);*/

inline bool isStringCaseEqual(StringView l, StringView r) {
	return string::detail::caseCompare_c(l, r) == 0;
}

template <ParameterName Name, class Value>
inline StyleParameter StyleParameter::create(const Value &v, MediaQueryId query, StyleRule r) {
	static_assert(Name != ParameterName::CssFontSize || !std::is_same_v<Value, uint8_t>,
			"uint8_t as FontSize is deprecated");
	StyleParameter p(Name, query, r);
	p.set<Name>(v);
	return p;
}

template <ParameterName Name, class Value>
inline void StyleList::set(const Value &value, MediaQueryId mediaQuery) {
	for (auto &it : data) {
		if (it.name == Name && it.mediaQuery == mediaQuery) {
			it.set<Name>(value);
			return;
		}
	}

	data.push_back(StyleParameter::create<Name>(value, mediaQuery));
}

} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOCSTYLE_H_ */
