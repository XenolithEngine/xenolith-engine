/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPCharGroup.h"

namespace STAPPLER_VERSIONIZED stappler {

bool inCharGroup(CharGroupId mask, char16_t c) {
	switch (mask) {
	case CharGroupId::None: return false; break;
	case CharGroupId::PunctuationBasic:
		return chars::CharGroup<char16_t, CharGroupId::PunctuationBasic>::match(c);
		break;
	case CharGroupId::Numbers:
		return chars::CharGroup<char16_t, CharGroupId::Numbers>::match(c);
		break;
	case CharGroupId::Latin: return chars::CharGroup<char16_t, CharGroupId::Latin>::match(c); break;
	case CharGroupId::Cyrillic:
		return chars::CharGroup<char16_t, CharGroupId::Cyrillic>::match(c);
		break;
	case CharGroupId::Currency:
		return chars::CharGroup<char16_t, CharGroupId::Currency>::match(c);
		break;
	case CharGroupId::GreekBasic:
		return chars::CharGroup<char16_t, CharGroupId::GreekBasic>::match(c);
		break;
	case CharGroupId::Math: return chars::CharGroup<char16_t, CharGroupId::Math>::match(c); break;
	case CharGroupId::Arrows:
		return chars::CharGroup<char16_t, CharGroupId::Arrows>::match(c);
		break;
	case CharGroupId::Fractionals:
		return chars::CharGroup<char16_t, CharGroupId::Fractionals>::match(c);
		break;
	case CharGroupId::LatinSuppl1:
		return chars::CharGroup<char16_t, CharGroupId::LatinSuppl1>::match(c);
		break;
	case CharGroupId::PunctuationAdvanced:
		return chars::CharGroup<char16_t, CharGroupId::PunctuationAdvanced>::match(c);
		break;
	case CharGroupId::GreekAdvanced:
		return chars::CharGroup<char16_t, CharGroupId::GreekAdvanced>::match(c);
		break;
	case CharGroupId::WhiteSpace:
		return chars::CharGroup<char16_t, CharGroupId::WhiteSpace>::match(c);
		break;
	case CharGroupId::Controls:
		return chars::CharGroup<char16_t, CharGroupId::Controls>::match(c);
		break;
	case CharGroupId::NonPrintable:
		return chars::CharGroup<char16_t, CharGroupId::NonPrintable>::match(c);
		break;
	case CharGroupId::LatinLowercase:
		return chars::CharGroup<char16_t, CharGroupId::LatinLowercase>::match(c);
		break;
	case CharGroupId::LatinUppercase:
		return chars::CharGroup<char16_t, CharGroupId::LatinUppercase>::match(c);
		break;
	case CharGroupId::Alphanumeric:
		return chars::CharGroup<char16_t, CharGroupId::Alphanumeric>::match(c);
		break;
	case CharGroupId::Hexadecimial:
		return chars::CharGroup<char16_t, CharGroupId::Hexadecimial>::match(c);
		break;
	case CharGroupId::Base64:
		return chars::CharGroup<char16_t, CharGroupId::Base64>::match(c);
		break;
	case CharGroupId::BreakableWhiteSpace:
		return chars::CharGroup<char16_t, CharGroupId::BreakableWhiteSpace>::match(c);
		break;
	case CharGroupId::OpticalAlignmentSpecial:
		return chars::CharGroup<char16_t, CharGroupId::OpticalAlignmentSpecial>::match(c);
		break;
	case CharGroupId::OpticalAlignmentBullet:
		return chars::CharGroup<char16_t, CharGroupId::OpticalAlignmentBullet>::match(c);
		break;
	case CharGroupId::TextPunctuation:
		return chars::CharGroup<char16_t, CharGroupId::TextPunctuation>::match(c);
		break;
	}
	return false;
}

bool inCharGroupMask(CharGroupId mask, char16_t c) {
	for (size_t i = 1; i < sizeof(CharGroupId) * 8; i++) {
		CharGroupId val = CharGroupId(1 << i);
		if ((mask & val) != CharGroupId::None) {
			if (inCharGroup(mask, c)) {
				return true;
			}
		}
	}
	return false;
}

namespace chars {

enum class SmartType : uint8_t {
	PunctuationBasic = 1 << 0,
	Numbers = 1 << 1,
	WhiteSpace = 1 << 2,
	LatinLowercase = 1 << 3,
	LatinUppercase = 1 << 4,
	Hexadecimial = 1 << 5,
	Base64 = 1 << 6,
	TextPunctuation = 1 << 7
};

// clang-format off
static uint8_t smart_lookup_table[256] = {
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   4,   4,   4,   4,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   4, 129, 129, 129, 129, 129,   1, 129, 129, 129, 129, 193, 129, 193, 129, 193,
	  98,  98,  98,  98,  98,  98,  98,  98,  98,  98, 129, 129,   1, 193,   1, 129,
	 129, 112, 112, 112, 112, 112, 112,  80,  80,  80,  80,  80,  80,  80,  80,  80,
	  80,  80,  80,  80,  80,  80,  80,  80,  80,  80,  80, 129, 129, 129, 129, 193,
	   1, 105, 105, 105, 105, 105, 105,  73,  73,  73,  73,  73,  73,  73,  73,  73,
	  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,   1,   1,   1,   1,   1,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
// clang-format on

bool CharGroup<char, GroupId::PunctuationBasic>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::PunctuationBasic);
}

bool CharGroup<char, GroupId::Numbers>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Numbers);
}

bool CharGroup<char, GroupId::Latin>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase));
}

bool CharGroup<char, GroupId::WhiteSpace>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::WhiteSpace);
}

bool CharGroup<char, GroupId::LatinLowercase>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinLowercase);
}

bool CharGroup<char, GroupId::LatinUppercase>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinUppercase);
}

bool CharGroup<char, GroupId::Alphanumeric>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase)
					| toInt(SmartType::Numbers));
}

bool CharGroup<char, GroupId::Hexadecimial>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Hexadecimial);
}

bool CharGroup<char, GroupId::Base64>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Base64);
}

bool CharGroup<char, GroupId::TextPunctuation>::match(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::TextPunctuation);
}

} // namespace chars

} // namespace STAPPLER_VERSIONIZED stappler
