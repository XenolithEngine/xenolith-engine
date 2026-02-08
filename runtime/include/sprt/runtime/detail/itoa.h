/**
Copyright (c) 2025 Xenolith Team <admin@stappler.org>

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

#include <sprt/runtime/int.h>
#include <sprt/runtime/detail/constexpr.h>

namespace sprt::_itoa {

// Copies two characters from src to dst.
template <typename Char>
constexpr inline void copy2(Char *dst, const Char *src) {
	__constexpr_memcpy(dst, src, 2 * sizeof(Char));
}

// Converts value in the range [0, 100) to a string.
template <typename Char>
constexpr inline auto digits2(size_t value) -> const Char * {
	if constexpr (sizeof(Char) == sizeof(char)) {
		return (Char *)&"0001020304050607080910111213141516171819"
				"2021222324252627282930313233343536373839"
				"4041424344454647484950515253545556575859"
				"6061626364656667686970717273747576777879"
				"8081828384858687888990919293949596979899"[value * 2];
	} else if constexpr (sizeof(Char) == sizeof(char16_t)) {
		return (Char *)&u"0001020304050607080910111213141516171819"
				u"2021222324252627282930313233343536373839"
				u"4041424344454647484950515253545556575859"
				u"6061626364656667686970717273747576777879"
				u"8081828384858687888990919293949596979899"[value * 2];
	} else if constexpr (sizeof(Char) == sizeof(char32_t)) {
		return (Char *)&U"0001020304050607080910111213141516171819"
				U"2021222324252627282930313233343536373839"
				U"4041424344454647484950515253545556575859"
				U"6061626364656667686970717273747576777879"
				U"8081828384858687888990919293949596979899"[value * 2];
	}
	return nullptr;
}

template <typename IntType, typename Char>
constexpr inline size_t unsigned_to_decimal(Char *out, IntType value, size_t size) {
	out += size;
	Char *end = out;
	while (value >= 100) {
		out -= 2;
		copy2(out, digits2<Char>(static_cast<size_t>(value % 100)));
		value /= 100;
	}
	if (value < 10) {
		*--out = static_cast<Char>('0' + value);
		return end - out;
	}
	out -= 2;
	copy2(out, digits2<Char>(static_cast<size_t>(value)));
	return end - out;
}

template <typename IntType>
constexpr inline size_t unsigned_to_decimal_len(IntType value) {
	size_t ret = 0;
	while (value >= 100) {
		ret += 2;
		value /= 100;
	}
	if (value < 10) {
		return ret + 1;
	}
	return ret + 2;
}

constexpr inline size_t _itoa_len(int64_t number) {
	if (number < 0) {
		auto ret = unsigned_to_decimal_len(uint64_t(-number));
		return ret + 1;
	} else {
		return unsigned_to_decimal_len(uint64_t(number));
	}
}

constexpr inline size_t _itoa_len(uint64_t number) {
	return unsigned_to_decimal_len(uint64_t(number));
}

} // namespace sprt::_itoa
