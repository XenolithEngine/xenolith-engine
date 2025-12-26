/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFont.h"

namespace STAPPLER_VERSIONIZED stappler::font {

void CharVector::addChar(char32_t c) { mem_std::emplace_ordered(chars, c); }

void CharVector::addString(const StringView &str) {
	StringViewUtf8 r(str);
	r.foreach ([&](char32_t c) { addChar(c); });
}

void CharVector::addString(const WideStringView &str) {
	auto ptr = str.data();
	auto size = str.size();
	uint8_t offset = 0;

	while (size > 0) {
		auto c = sprt::unicode::utf16Decode32(ptr, offset);
		if (c) {
			addChar(c);
		}
		ptr += offset;
		size -= offset;
	}
}

void CharVector::addString(const CharVector &str) {
	for (auto &c : str.chars) { mem_std::emplace_ordered(chars, c); }
}

uint32_t CharId::getCharId(uint16_t sourceId, char32_t ch, CharAnchor a) {
	uint32_t ret = ch & 0xFFFF;
	ret |= (toInt(a) << (sizeof(char16_t) * 8));
	ret |= (sourceId << ((sizeof(char16_t) * 8) + 2));
	return ret;
}

uint32_t CharId::rebindCharId(uint32_t ret, CharAnchor a) {
	return (ret & (~(3 << (sizeof(char16_t) * 8)))) | (toInt(a) << (sizeof(char16_t) * 8));
}

CharAnchor CharId::getAnchorForChar(uint32_t obj) {
	return CharAnchor((obj >> sizeof(char16_t) * 8) & 0b11);
}

} // namespace stappler::font
