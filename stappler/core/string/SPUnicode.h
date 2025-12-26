/**
 Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_STRING_SPUNICODE_H_
#define STAPPLER_CORE_STRING_SPUNICODE_H_

#include "SPRuntimeUnicode.h"
#include "SPMemString.h"

namespace STAPPLER_VERSIONIZED stappler::unicode {

template <typename PutCharFn>
inline uint8_t utf8EncodeCb(const PutCharFn &cb, char16_t c) {
	static_assert(std::is_invocable_v<PutCharFn, char>, "Argumnet should be invokable with char");
	return sprt::unicode::utf8EncodeCb(cb, c);
}

template <typename PutCharFn>
inline uint8_t utf8EncodeCb(const PutCharFn &cb, char32_t c) {
	static_assert(std::is_invocable_v<PutCharFn, char>, "Argumnet should be invokable with char");
	return sprt::unicode::utf8EncodeCb(cb, c);
}

inline uint8_t utf8Encode(std::string &str, char16_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str.push_back(c); }, ch);
}

inline uint8_t utf8Encode(std::string &str, char32_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str.push_back(c); }, ch);
}

inline uint8_t utf8Encode(memory::string &str, char16_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str.push_back(c); }, ch);
}

inline uint8_t utf8Encode(memory::string &str, char32_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str.push_back(c); }, ch);
}

inline uint8_t utf8Encode(std::ostream &str, char16_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str << c; }, ch);
}

inline uint8_t utf8Encode(std::ostream &str, char32_t ch) {
	return utf8EncodeCb([&](char c) SPINLINE { str << c; }, ch);
}

template <typename PutCharFn>
inline uint8_t utf16EncodeCb(const PutCharFn &cb, char32_t c) {
	static_assert(std::is_invocable_v<PutCharFn, char16_t>,
			"Argumnet should be invokable with char16_t");
	return sprt::unicode::utf16EncodeCb(cb, c);
}

inline uint8_t utf16Encode(std::u16string &str, char32_t ch) {
	return utf16EncodeCb([&](char16_t c) SPINLINE { str.push_back(c); }, ch);
}

inline uint8_t utf16Encode(memory::u16string &str, char32_t ch) {
	return utf16EncodeCb([&](char16_t c) SPINLINE { str.push_back(c); }, ch);
}

template <typename std::enable_if<std::is_class<std::ctype<char16_t>>::value>::type * = nullptr>
inline uint8_t utf16Encode(std::basic_ostream<char16_t> &out, char32_t ch) {
	return utf16EncodeCb([&](char16_t c) SPINLINE { out << c; }, ch);
}

} // namespace stappler::unicode

#endif /* STAPPLER_CORE_STRING_SPUNICODE_H_ */
