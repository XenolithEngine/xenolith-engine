/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFontStyle.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::font {

static void s_getSpecializationArgs(std::ostream &out, const FontSpecializationVector &vec) {
	out << "?size=" << vec.fontSize.get();
	out << "&weight=" << vec.fontWeight.get();
	out << "&width=" << vec.fontStretch.get();
	switch (vec.fontStyle.get()) {
	case FontStyle::Normal.get(): out << "&style=normal"; break;
	case FontStyle::Italic.get(): out << "&style=italic"; break;
	default: out << "&style=" << vec.fontStyle.get(); break;
	}
	out << "&density=" << vec.density;
	if (vec.fontGrade != FontGrade::Normal) {
		out << "&grade=" << vec.fontGrade.get();
	}
}

template <>
auto FontSpecializationVector::getSpecializationArgs<memory::PoolInterface>() const
		-> memory::PoolInterface::StringType {
	memory::PoolInterface::StringStreamType out;
	s_getSpecializationArgs(out, *this);
	return out.str();
}

template <>
auto FontSpecializationVector::getSpecializationArgs<memory::StandartInterface>() const
		-> memory::StandartInterface::StringType {
	memory::StandartInterface::StringStreamType out;
	s_getSpecializationArgs(out, *this);
	return out.str();
}

FontParameters FontParameters::create(StringView str, memory::pool_t *pool) {
	FontParameters ret;

	enum State {
		Family,
		Size,
		Style,
		Weight,
		Stretch,
		Overflow,
	} state = Family;

	str.split<StringView::Chars<'.'>>([&](const StringView &ir) {
		StringView r(ir);
		switch (state) {
		case Family:
			ret.fontFamily = r.pdup(pool);
			state = Size;
			break;
		case Size:
			if (r.is("xxs")) {
				ret.fontSize = FontSize::XXSmall;
			} else if (r.is("xs")) {
				ret.fontSize = FontSize::XSmall;
			} else if (r.is("s")) {
				ret.fontSize = FontSize::Small;
			} else if (r.is("m")) {
				ret.fontSize = FontSize::Medium;
			} else if (r.is("l")) {
				ret.fontSize = FontSize::Large;
			} else if (r.is("xl")) {
				ret.fontSize = FontSize::XLarge;
			} else if (r.is("xxl")) {
				ret.fontSize = FontSize::XXLarge;
			} else {
				r.readInteger().unwrap([&](int64_t value) { ret.fontSize = FontSize(value); });
			}
			state = Style;
			break;
		case Style:
			if (r.is("i")) {
				ret.fontStyle = FontStyle::Italic;
			} else if (r.is("o")) {
				ret.fontStyle = FontStyle::Oblique;
			} else if (r.is("n")) {
				ret.fontStyle = FontStyle::Normal;
			}
			state = Weight;
			break;
		case Weight:
			ret.fontWeight = FontWeight(r.readInteger(10).get(400));
			state = Stretch;
			break;
		case Stretch:
			ret.fontStretch = FontStretch(r.readInteger(10).get(100 << 1));
			state = Overflow;
			break;
		default: break;
		}
	});
	return ret;
}

template <>
auto FontParameters::getFontConfigName<memory::PoolInterface>(StringView fontFamily,
		FontSize fontSize, FontStyle fontStyle, FontWeight fontWeight, FontStretch fontStretch,
		FontGrade fontGrade, FontVariant fontVariant, bool caps)
		-> memory::PoolInterface::StringType {
	auto size = fontSize;
	memory::PoolInterface::StringType name;
	name.reserve(fontFamily.size() + 14);
	name += fontFamily.str<memory::PoolInterface>();

	if (caps && fontVariant == FontVariant::SmallCaps) {
		size -= size / 5.0f;
	}

	name += "." + mem_pool::toString(size.get());

	switch (fontStyle.get()) {
	case FontStyle::Normal.get(): name += ".n"; break;
	case FontStyle::Italic.get(): name += ".i"; break;
	default:
		name += ".";
		name += mem_pool::toString(fontStyle.get());
		break;
	}

	name += mem_pool::toString(".", fontWeight.get());
	name += mem_pool::toString(".", fontStretch.get());
	name += mem_pool::toString(".", fontGrade.get());
	return name;
}

template <>
auto FontParameters::getFontConfigName<memory::StandartInterface>(StringView fontFamily,
		FontSize fontSize, FontStyle fontStyle, FontWeight fontWeight, FontStretch fontStretch,
		FontGrade fontGrade, FontVariant fontVariant, bool caps)
		-> memory::StandartInterface::StringType {
	auto size = fontSize;
	memory::StandartInterface::StringType name;
	name.reserve(fontFamily.size() + 14);
	name += fontFamily.str<memory::StandartInterface>();

	if (caps && fontVariant == FontVariant::SmallCaps) {
		size -= size / 5.0f;
	}

	name += "." + mem_std::toString(size.get());

	switch (fontStyle.get()) {
	case FontStyle::Normal.get(): name += ".n"; break;
	case FontStyle::Italic.get(): name += ".i"; break;
	default:
		name += ".";
		name += mem_std::toString(fontStyle.get());
		break;
	}

	name += mem_std::toString(".", fontWeight.get());
	name += mem_std::toString(".", fontStretch.get());
	name += mem_std::toString(".", fontGrade.get());
	return name;
}

FontParameters FontParameters::getSmallCaps() const {
	FontParameters ret = *this;
	ret.fontSize -= ret.fontSize / 5.0f;
	return ret;
}

FontSpecializationVector FontVariations::getSpecialization(
		const FontSpecializationVector &vec) const {
	FontSpecializationVector ret = vec;
	if ((axisMask & FontVariableAxis::Weight) != FontVariableAxis::None) {
		ret.fontWeight = weight.clamp(vec.fontWeight);
	} else {
		ret.fontWeight = weight.min;
	}
	if ((axisMask & FontVariableAxis::Stretch) != FontVariableAxis::None) {
		ret.fontStretch = stretch.clamp(vec.fontStretch);
	} else {
		ret.fontStretch = stretch.min;
	}
	if ((axisMask & FontVariableAxis::Grade) != FontVariableAxis::None) {
		ret.fontGrade = grade.clamp(vec.fontGrade);
	} else {
		ret.fontGrade = grade.min;
	}

	switch (vec.fontStyle.get()) {
	case FontStyle::Normal.get():
		// we should have 0 on italic and slant
		if (italic.min == 0 && slant.min <= FontStyle::Normal && slant.max >= FontStyle::Normal) {
			ret.fontStyle = FontStyle::Normal;
		} else {
			if (italic.min > 0) {
				ret.fontStyle = FontStyle::Italic;
			} else {
				ret.fontStyle = slant.clamp(FontStyle::Normal);
			}
		}
		break;
	case FontStyle::Italic.get():
		// try true italic or slant emulation
		if (italic.min > 0) {
			ret.fontStyle = FontStyle::Italic;
		} else {
			ret.fontStyle = slant.clamp(FontStyle::Oblique);
		}
		break;
	default:
		if ((axisMask & FontVariableAxis::Slant) != FontVariableAxis::None) {
			ret.fontStyle = slant.clamp(vec.fontStyle);
		} else if ((axisMask & FontVariableAxis::Italic) != FontVariableAxis::None
				&& italic.min != italic.max) {
			ret.fontStyle = FontStyle::Italic;
		} else {
			if (italic.min == 1) {
				ret.fontStyle = FontStyle::Italic;
			} else {
				ret.fontStyle = slant.min;
			}
		}
		break;
	}

	return ret;
}

} // namespace stappler::font
