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

#include <sprt/runtime/int.h>
#include <sprt/runtime/array.h>
#include <sprt/runtime/notnull.h>

#include <sprt/c/__sprt_string.h>

namespace sprt {

SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, const char SPRT_NONNULL *str,
		size_t strSize);

SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, int64_t);
SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, uint64_t);
SPRT_API char *strappend(char SPRT_NONNULL *buf, size_t *bufSize, double);

// fast itoa implementation
// data will be written at the end of buffer, no trailing zero (do not try to use strlen on it!)
// designed to be used with StringView: StringView(buf + bufSize - ret, ret)

// use nullptr buffer to calculate expected buffer length

SPRT_API size_t itoa(int64_t number, char *buffer, size_t bufSize);
SPRT_API size_t itoa(uint64_t number, char *buffer, size_t bufSize);

SPRT_API size_t itoa(int64_t number, char16_t *buffer, size_t bufSize);
SPRT_API size_t itoa(uint64_t number, char16_t *buffer, size_t bufSize);

// fast dtoa implementation
// data will be written from beginning, no trailing zero (do not try to use strlen on it!)
// designed to be used with StringView: StringView(buf, ret)

// use nullptr buffer to calculate expected buffer length

static constexpr size_t INT_MAX_DIGITS = 22;
static constexpr size_t DOUBLE_MAX_DIGITS = 27;

SPRT_API size_t dtoa(double number, char *buffer, size_t bufSize);
SPRT_API size_t dtoa(double number, char16_t *buffer, size_t bufSize);

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

SPRT_API char tolower_c(char);
SPRT_API char16_t tolower_c(char16_t);
SPRT_API char32_t tolower_c(char32_t);

SPRT_API char toupper_c(char);
SPRT_API char16_t toupper_c(char16_t);
SPRT_API char32_t toupper_c(char32_t);

inline constexpr size_t strlen(const char *str) {
	if (is_constant_evaluated()) {
		if (str == nullptr) {
			return 0;
		}
		const char *end = str;
		while (*end != u'\0') { ++end; }
		return static_cast<size_t>(end - str);
	} else {
		return ::__sprt_strlen(str);
	}
}

inline constexpr size_t strlen(const char16_t *str) {
	if (str == nullptr) {
		return 0;
	}
	const char16_t *end = str;
	while (*end != u'\0') { ++end; }
	return static_cast<size_t>(end - str);
}

inline constexpr size_t strlen(const char32_t *str) {
	if (str == nullptr) {
		return 0;
	}
	const char32_t *end = str;
	while (*end != u'\0') { ++end; }
	return static_cast<size_t>(end - str);
}

SPRT_API void nullify(const char *, size_t);
SPRT_API void nullify(const char16_t *, size_t);

SPRT_API char *_makeCharBuffer(size_t);
SPRT_API char16_t *_makeChar16Buffer(size_t);

template <typename Type>
auto newCharBuffer(size_t) -> Type *;

template <>
inline auto newCharBuffer<char>(size_t size) -> char * {
	return _makeCharBuffer(size);
}

template <>
inline auto newCharBuffer<char16_t>(size_t size) -> char16_t * {
	return _makeChar16Buffer(size);
}

SPRT_API void freeCharBuffer(char *);
SPRT_API void freeCharBuffer(char16_t *);

template <typename CharType>
constexpr inline bool chareq(CharType c1, CharType c2) {
	return c1 == c2;
}

template <>
constexpr inline bool chareq<char>(char c1, char c2) {
	return static_cast<unsigned char>(c1) == static_cast<unsigned char>(c2);
}

template <typename CharType>
constexpr inline bool charlt(CharType c1, CharType c2) {
	return c1 < c2;
}

template <>
constexpr inline bool charlt<char>(char c1, char c2) {
	return static_cast<unsigned char>(c1) < static_cast<unsigned char>(c2);
}

template <typename CharType>
constexpr inline int strcompare(const CharType *s1, const CharType *s2, size_t count) {
	return __sprt_memcmp(s1, s2, count);
}

template <typename CharType>
constexpr inline const CharType *strfind(const CharType *ptr, size_t count, const CharType &ch) {
	auto end = ptr + count;

	while (ptr != end) {
		if (*ptr++ == ch) {
			return ptr - 1;
		}
	}
	return nullptr;
}

template <>
constexpr inline const char *strfind<char>(const char *ptr, size_t count, const char &ch) {
	return (const char *)__sprt_memchr((void *)ptr, ch, count);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STRING_H_
