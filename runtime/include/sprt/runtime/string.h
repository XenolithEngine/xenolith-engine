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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STRING_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_STRING_H_

#include <sprt/runtime/callback.h>
#include <sprt/runtime/int.h>
#include <sprt/runtime/array.h>
#include <sprt/runtime/notnull.h>
#include <sprt/runtime/detail/itoa.h>
#include <sprt/runtime/detail/dtoa.h>

#include <sprt/c/__sprt_string.h>

namespace sprt {

/*
	Fast itoa implementation

	Data will be written at the end of buffer, no trailing zero (do not try to use strlen on it!);
	Designed to be used with StringView: StringView(buf + bufSize - ret, ret)

	Returns number size on symbols
	Use nullptr buffer to calculate expected buffer length.

	No buffer size control - use INT_MAX_DIGITS or precalculated length
*/

static constexpr size_t INT_MAX_DIGITS = 22; // Safe buffer size

template <typename CharType>
constexpr inline size_t itoa(int64_t number, CharType *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa::_itoa_len(number);
	}

	if (number < 0) {
		auto ret = _itoa::unsigned_to_decimal(buffer, uint64_t(-number), bufSize);
		buffer[bufSize - ret - 1] = '-';
		return ret + 1;
	} else {
		return _itoa::unsigned_to_decimal(buffer, uint64_t(number), bufSize);
	}
}

template <typename CharType>
constexpr inline size_t itoa(uint64_t number, CharType *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _itoa::_itoa_len(number);
	}

	return _itoa::unsigned_to_decimal(buffer, uint64_t(number), bufSize);
}

/*
	Fast dtoa implementation

	Data will be written from beginning, no trailing zero (do not try to use strlen on it!);
	Designed to be used with StringView: StringView(buf, ret)

	Returns number size on symbols
	Use nullptr buffer to calculate expected buffer length.

	No buffer size control - use DOUBLE_MAX_DIGITS or precalculated length
*/

static constexpr size_t DOUBLE_MAX_DIGITS = 27; // Safe buffer size

template <typename CharType>
constexpr inline size_t dtoa(double number, CharType *buffer, size_t bufSize) {
	if (buffer == nullptr) {
		return _dtoa::dtoa_len(number);
	}

	return _dtoa::dtoa_milo(number, buffer);
}

SPRT_API bool ispunct(char c);
SPRT_API bool isdigit(char c);
SPRT_API bool isalpha(char c);
SPRT_API bool isspace(char c);
SPRT_API bool islower(char c);
SPRT_API bool isupper(char c);
SPRT_API bool isalnum(char c);
SPRT_API bool isxdigit(char c);
SPRT_API bool isbase64(char c);
SPRT_API bool istpunct(char c);

SPRT_API constexpr inline char tolower_c(char c) { return __constexpr_tolower_c(c); }
SPRT_API constexpr inline char16_t tolower_c(char16_t c) { return __constexpr_tolower_c(c); }
SPRT_API constexpr inline char32_t tolower_c(char32_t c) { return __constexpr_tolower_c(c); }

SPRT_API constexpr inline char toupper_c(char c) { return __constexpr_toupper_c(c); }
SPRT_API constexpr inline char16_t toupper_c(char16_t c) { return __constexpr_toupper_c(c); }
SPRT_API constexpr inline char32_t toupper_c(char32_t c) { return __constexpr_toupper_c(c); }

SPRT_API void nullify(const char *, size_t);
SPRT_API void nullify(const char16_t *, size_t);

/*
	Safe strcat-like function for char buffer filling.
	Writes buffer str with strSize chars into buf with initial free space in *bufSize
	*bufSize then decremented for number of chars written.

	Buffer should contain space for null-termination

	Returns: pointer to next free space in buffer

	On overflow, *bufSize set to 0, and only non-overlow chars written

	Designed for chained usage like:

		auto buf = (char *)__sprt_malloca(bufSize + 1);

		auto target = buf;
		auto targetSize = bufSize;

		target = strappend(target, &targetSize, " ", 1);
		target = strappend(target, &targetSize, data, datalen);
		...
		target = strappend(target, &targetSize, "\n", 1);

		target[0] = 0; // null termination

		... // use filled buffer

		__sprt_freea(buf);
*/
template <typename CharType>
[[nodiscard("Use result as next buffer pointer")]]
constexpr inline auto strappend(CharType SPRT_NONNULL *buf, size_t *bufSize,
		const CharType SPRT_NONNULL *str, size_t strSize) -> CharType * {
	if (!bufSize) {
		return buf;
	}

	if (strSize < *bufSize) {
		__constexpr_memcpy(buf, str, strSize);
		buf += strSize;
		*bufSize -= strSize;
		*buf = 0;
		return buf;
	} else if (*bufSize > 1) {
		__constexpr_memcpy(buf, str, *bufSize - 1);
		buf += (*bufSize - 1);
		*bufSize = 1;
		*buf = 0;
		return buf;
	} else {
		return buf;
	}
}

/*
	strappend overload to write integer decimial value into string
*/
template <typename CharType>
[[nodiscard("Use result as next buffer pointer")]]
constexpr inline auto strappend(CharType SPRT_NONNULL *buf, size_t *bufSize, int64_t val)
		-> CharType * {
	CharType tmp[INT_MAX_DIGITS];
	auto ret = sprt::itoa(val, tmp, INT_MAX_DIGITS);
	return strappend(buf, bufSize, tmp + INT_MAX_DIGITS - ret, ret);
}

/*
	strappend overload to write integer decimial value into string
*/
template <typename CharType>
[[nodiscard("Use result as next buffer pointer")]]
constexpr inline auto strappend(char SPRT_NONNULL *buf, size_t *bufSize, uint64_t val)
		-> CharType * {
	CharType tmp[INT_MAX_DIGITS];
	auto ret = sprt::itoa(val, tmp, INT_MAX_DIGITS);
	return strappend(buf, bufSize, tmp + INT_MAX_DIGITS - ret, ret);
}

/*
	strappend overload to write float value into string
*/
template <typename CharType>
[[nodiscard("Use result as next buffer pointer")]]
constexpr inline auto strappend(char SPRT_NONNULL *buf, size_t *bufSize, double val) -> CharType * {
	CharType tmp[DOUBLE_MAX_DIGITS];
	auto ret = sprt::dtoa(val, tmp, DOUBLE_MAX_DIGITS);
	return strappend(buf, bufSize, tmp, ret);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STRING_H_
