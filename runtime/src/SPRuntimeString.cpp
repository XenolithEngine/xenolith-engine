/**
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

#include <sprt/runtime/string.h>
#include <sprt/runtime/float.h>
#include <sprt/runtime/math.h>

#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_ctype.h>
#include <sprt/c/__sprt_wchar.h>

#include <math.h>

namespace sprt {

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

bool ispunct(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::PunctuationBasic);
}

bool isdigit(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Numbers);
}

bool isalpha(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase));
}

bool isspace(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::WhiteSpace);
}

bool islower(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinLowercase);
}

bool isupper(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::LatinUppercase);
}

bool isalnum(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]]
			& (toInt(SmartType::LatinLowercase) | toInt(SmartType::LatinUppercase)
					| toInt(SmartType::Numbers));
}

bool isxdigit(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Hexadecimial);
}

bool isbase64(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::Base64);
}

bool istpunct(char c) {
	return smart_lookup_table[((const uint8_t *)&c)[0]] & toInt(SmartType::TextPunctuation);
}

void nullify(const char *ptr, size_t size) { __sprt_memset((void *)ptr, 0, size * sizeof(char)); }

void nullify(const char16_t *ptr, size_t size) {
	__sprt_memset((void *)ptr, 0, size * sizeof(char16_t));
}

} // namespace sprt
