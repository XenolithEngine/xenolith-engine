/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef CORE_FONT_SPFONTSTYLE_H_
#define CORE_FONT_SPFONTSTYLE_H_

#include "SPFont.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::font {

using EnumSize = uint8_t;

enum class FontVariableAxis : uint32_t {
	None,
	Weight = 1 << 0, // wght
	Width = 1 << 1, // wdth
	Italic = 1 << 2, // ital
	Slant = 1 << 3, // slnt
	OpticalSize = 1 << 4, // opsz
	Grade = 1 << 5, // GRAD

	Stretch = Width,
};

SP_DEFINE_ENUM_AS_MASK(FontVariableAxis)

enum class Autofit : EnumSize {
	None,
	Width,
	Height,
	Cover,
	Contain,
};

enum class TextTransform : EnumSize {
	None,
	Uppercase,
	Lowercase,
};

enum class TextDecoration : EnumSize {
	None,
	LineThrough,
	Overline,
	Underline,
};

enum class TextAlign : EnumSize {
	Left,
	Center,
	Right,
	Justify,
};

enum class WhiteSpace : EnumSize {
	Normal,
	Nowrap,
	Pre,
	PreLine,
	PreWrap,
};

enum class Hyphens : EnumSize {
	None,
	Manual,
	Auto,
};

enum class VerticalAlign : EnumSize {
	Baseline,
	Middle,
	Sub,
	Super,
	Top,
	Bottom
};

// slnt axis or special value for Italic
struct SP_PUBLIC FontStyle : ValueWrapper<int16_t, class FontStyleFlag> {
	static const FontStyle Normal;
	static const FontStyle Italic;
	static const FontStyle Oblique;

	static constexpr FontStyle FromDegrees(float d) { return FontStyle(std::floor(d * 64.0f)); }

	using ValueWrapper::ValueWrapper;
};

struct SP_PUBLIC FontWeight : ValueWrapper<uint16_t, class FontWeightFlag> {
	static const FontWeight Thin;
	static const FontWeight ExtraLight;
	static const FontWeight Light;
	static const FontWeight Normal;
	static const FontWeight Regular;
	static const FontWeight Medium;
	static const FontWeight SemiBold;
	static const FontWeight Bold;
	static const FontWeight ExtraBold;
	static const FontWeight Heavy;
	static const FontWeight Black;

	using ValueWrapper::ValueWrapper;
};

struct SP_PUBLIC FontStretch : ValueWrapper<uint16_t, class FontStretchFlag> {
	static const FontStretch UltraCondensed;
	static const FontStretch ExtraCondensed;
	static const FontStretch Condensed;
	static const FontStretch SemiCondensed;
	static const FontStretch Normal;
	static const FontStretch SemiExpanded;
	static const FontStretch Expanded;
	static const FontStretch ExtraExpanded;
	static const FontStretch UltraExpanded;

	using ValueWrapper::ValueWrapper;
};

struct SP_PUBLIC FontGrade : ValueWrapper<int16_t, class FontGradeFlag> {
	static const FontGrade Thin;
	static const FontGrade Reduced;
	static const FontGrade Normal;
	static const FontGrade Heavy;

	using ValueWrapper::ValueWrapper;
};

enum class FontVariant : EnumSize {
	Normal,
	SmallCaps,
};

enum class ListStyleType : EnumSize {
	None,
	Circle,
	Disc,
	Square,
	XMdash,
	Decimal,
	DecimalLeadingZero,
	LowerAlpha,
	LowerGreek,
	LowerRoman,
	UpperAlpha,
	UpperRoman
};

struct SP_PUBLIC FontSize {
	static const FontSize XXSmall;
	static const FontSize XSmall;
	static const FontSize Small;
	static const FontSize Medium;
	static const FontSize Large;
	static const FontSize XLarge;
	static const FontSize XXLarge;

	static FontSize progress(FontSize source, FontSize target, float p) {
		auto v = source.val() * (1.0f - p) + target.val() * p;
		return make(v);
	}

	static constexpr FontSize make(float v) {
		FontSize ret;
		ret.value = static_cast<uint16_t>(std::floor(v * 16.0f));
		return ret;
	}

	inline constexpr FontSize() = default;
	inline constexpr FontSize(const FontSize &) = default;

	inline explicit constexpr FontSize(uint16_t val) : value(val << 4) { }

	constexpr FontSize scale(float density) const { return FontSize::make(val() * density); }

	constexpr FontSize operator*(float v) const { return scale(v); }
	constexpr FontSize operator/(float v) const { return scale(1.0f / v); }

	constexpr uint16_t get() const { return value >> 4; }
	constexpr float val() const { return static_cast<float>(value) / 16.0f; }

	constexpr FontSize &operator-=(FontSize v) {
		value -= v.value;
		return *this;
	}

	constexpr bool operator==(const FontSize &) const = default;
	constexpr bool operator!=(const FontSize &) const = default;

	uint16_t value = 0;
};

struct SP_PUBLIC TextParameters {
	TextTransform textTransform = TextTransform::None;
	TextDecoration textDecoration = TextDecoration::None;
	WhiteSpace whiteSpace = WhiteSpace::Normal;
	Hyphens hyphens = Hyphens::Manual;
	VerticalAlign verticalAlign = VerticalAlign::Baseline;
	geom::Color3B color = geom::Color3B::BLACK;
	uint8_t opacity = 222;

	inline bool operator==(const TextParameters &other) const = default;
	inline bool operator!=(const TextParameters &other) const = default;
};

struct SP_PUBLIC FontLayoutParameters {
	FontStyle fontStyle = FontStyle::Normal;
	FontWeight fontWeight = FontWeight::Normal;
	FontStretch fontStretch = FontStretch::Normal;
	FontGrade fontGrade = FontGrade::Normal;

	inline bool operator==(const FontLayoutParameters &other) const = default;
	inline bool operator!=(const FontLayoutParameters &other) const = default;
};

struct SP_PUBLIC FontSpecializationVector : FontLayoutParameters {
	FontSize fontSize = FontSize(14);
	float density = 1.0f;

	template <typename Interface>
	auto getSpecializationArgs() const -> typename Interface::StringType;

	inline bool operator==(const FontSpecializationVector &other) const = default;
	inline bool operator!=(const FontSpecializationVector &other) const = default;
};

struct SP_PUBLIC FontParameters : FontSpecializationVector {
	static FontParameters create(StringView, memory::pool_t * = nullptr);

	template <typename Interface>
	static auto getFontConfigName(StringView fontFamily, FontSize fontSize, FontStyle fontStyle,
			FontWeight fontWeight, FontStretch fontStretch, FontGrade fontGrade,
			FontVariant fontVariant, bool caps) -> typename Interface::StringType;

	FontVariant fontVariant = FontVariant::Normal;
	ListStyleType listStyleType = ListStyleType::None;
	StringView fontFamily;
	bool persistent = false;

	template <typename Interface>
	auto getConfigName(bool caps = false) const -> typename Interface::StringType {
		return getFontConfigName<Interface>(fontFamily, fontSize, fontStyle, fontWeight,
				fontStretch, fontGrade, fontVariant, caps);
	}

	FontParameters getSmallCaps() const;

	inline bool operator==(const FontParameters &other) const = default;
	inline bool operator!=(const FontParameters &other) const = default;
};

struct SP_PUBLIC FontVariations {
	template <typename T>
	struct Variations {
		T min;
		T max;

		Variations &operator=(const T &v) {
			min = v;
			max = v;
			return *this;
		}

		T clamp(T val) const { return math::clamp(val, min, max); }
	};

	FontVariableAxis axisMask = FontVariableAxis::None;
	Variations<FontWeight> weight = Variations<FontWeight>{FontWeight::Normal, FontWeight::Normal};
	Variations<FontStretch> stretch =
			Variations<FontStretch>{FontStretch::Normal, FontStretch::Normal};
	Variations<FontStyle> slant = Variations<FontStyle>{FontStyle::Normal, FontStyle::Normal};
	Variations<uint32_t> opticalSize = Variations<uint32_t>{0, 0};
	Variations<uint32_t> italic = Variations<uint32_t>{0, 0};
	Variations<FontGrade> grade = Variations<FontGrade>{FontGrade::Normal, FontGrade::Normal};

	FontSpecializationVector getSpecialization(const FontSpecializationVector &vec) const;
};

constexpr FontStretch FontStretch::UltraCondensed = FontStretch(50 << 1);
constexpr FontStretch FontStretch::ExtraCondensed = FontStretch((62 << 1) | 1);
constexpr FontStretch FontStretch::Condensed = FontStretch(75 << 1);
constexpr FontStretch FontStretch::SemiCondensed = FontStretch((87 << 1) | 1);
constexpr FontStretch FontStretch::Normal = FontStretch(100 << 1);
constexpr FontStretch FontStretch::SemiExpanded = FontStretch((112 << 1) | 1);
constexpr FontStretch FontStretch::Expanded = FontStretch(125 << 1);
constexpr FontStretch FontStretch::ExtraExpanded = FontStretch(150 << 1);
constexpr FontStretch FontStretch::UltraExpanded = FontStretch(200 << 1);

constexpr FontWeight FontWeight::Thin = FontWeight(100);
constexpr FontWeight FontWeight::ExtraLight = FontWeight(200);
constexpr FontWeight FontWeight::Light = FontWeight(300);
constexpr FontWeight FontWeight::Normal = FontWeight(400);
constexpr FontWeight FontWeight::Regular = FontWeight(400);
constexpr FontWeight FontWeight::Medium = FontWeight(500);
constexpr FontWeight FontWeight::SemiBold = FontWeight(600);
constexpr FontWeight FontWeight::Bold = FontWeight(700);
constexpr FontWeight FontWeight::ExtraBold = FontWeight(800);
constexpr FontWeight FontWeight::Heavy = FontWeight(900);
constexpr FontWeight FontWeight::Black = FontWeight(1'000);

constexpr FontSize FontSize::XXSmall = FontSize(uint16_t(10));
constexpr FontSize FontSize::XSmall = FontSize(uint16_t(12));
constexpr FontSize FontSize::Small = FontSize(uint16_t(14));
constexpr FontSize FontSize::Medium = FontSize(uint16_t(16));
constexpr FontSize FontSize::Large = FontSize(uint16_t(18));
constexpr FontSize FontSize::XLarge = FontSize(uint16_t(22));
constexpr FontSize FontSize::XXLarge = FontSize(uint16_t(26));

constexpr FontStyle FontStyle::Normal = FontStyle(0);
constexpr FontStyle FontStyle::Italic = FontStyle(minOf<int16_t>());
constexpr FontStyle FontStyle::Oblique = FontStyle(-10 << 6);

constexpr FontGrade FontGrade::Thin = FontGrade(-200);
constexpr FontGrade FontGrade::Reduced = FontGrade(-50);
constexpr FontGrade FontGrade::Normal = FontGrade(0);
constexpr FontGrade FontGrade::Heavy = FontGrade(150);

} // namespace stappler::font

namespace STAPPLER_VERSIONIZED stappler {

inline font::FontSize progress(font::FontSize source, font::FontSize target, float p) {
	return font::FontSize::progress(source, target, p);
}

} // namespace STAPPLER_VERSIONIZED stappler

namespace std {

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::font::FontSize> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::font::FontSize &value) const noexcept {
		return hash<uint16_t>{}(value.get());
	}
};

} // namespace std

#endif /* CORE_GEOM_SPFONTSTYLE_H_ */
