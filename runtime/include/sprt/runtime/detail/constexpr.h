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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTEXPR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTEXPR_H_

#include <sprt/runtime/int.h>
#include <sprt/c/__sprt_string.h>

namespace sprt {

template <typename _Tp, typename _Up>
inline constexpr const bool __is_trivially_lexicographically_comparable_v =
		is_same_v<remove_cv_t<_Tp>, remove_cv_t<_Up> >
		&& (is_unsigned_v<_Tp> || is_same_v<remove_cv_t<_Tp>, char>);

template <typename _Tp>
constexpr _Tp *__constexpr_memset(_Tp *dest, const _Tp *source, size_t __count) {
	if (is_constant_evaluated()) {
		while (__count-- > 0) { *(dest++) = *(source++); }
	} else {
		return (_Tp *)__builtin_memcpy(dest, source, __count * sizeof(_Tp));
	}
}

template <typename _Tp>
constexpr _Tp *__constexpr_memcpy(_Tp *dest, const _Tp *source, size_t __count) {
	if (is_constant_evaluated()) {
		while (__count-- > 0) { *(dest++) = *(source++); }
	} else {
		return (_Tp *)__builtin_memcpy(dest, source, __count * sizeof(_Tp));
	}
}

template <typename _Tp>
constexpr _Tp *__constexpr_memmove(_Tp *dest, const _Tp *source, size_t __count) {
	if (is_constant_evaluated()) {
		if (dest == source) {
			return dest; // No copy needed if src and dest are the same
		}

		// Check for overlap: if destination starts within the source range
		if (dest > source && dest < source + __count) {
			dest += __count;
			source += __count;

			while (__count-- > 0) { *(--dest) = *(--source); }
		} else {
			while (__count-- > 0) { *(dest++) = *(source++); }
		}

		return dest; // Return the original destination pointer
	} else {
		return (_Tp *)__builtin_memmove(dest, source, __count * sizeof(_Tp));
	}
}

template <typename _Tp, typename _Up>
constexpr int __constexpr_memcmp(const _Tp *__lhs, const _Up *__rhs, size_t __count) {
	static_assert(__is_trivially_lexicographically_comparable_v<_Tp, _Up>,
			"_Tp and _Up have to be trivially lexicographically comparable");

	if (is_constant_evaluated()) {
		while (__count != 0) {
			if (*__lhs < *__rhs) {
				return -1;
			}
			if (*__rhs < *__lhs) {
				return 1;
			}

			--__count;
			++__lhs;
			++__rhs;
		}
		return 0;
	} else {
		return __builtin_memcmp(__lhs, __rhs, __count * sizeof(_Tp));
	}
}

inline constexpr size_t __constexpr_strlen(const char *str) {
	if (is_constant_evaluated()) {
		if (str == nullptr) {
			return 0;
		}
		const char *end = str;
		while (*end != u'\0') { ++end; }
		return static_cast<size_t>(end - str);
	} else {
		return __builtin_strlen(str);
	}
}

inline constexpr size_t __constexpr_strlen(const char16_t *str) {
	if (str == nullptr) {
		return 0;
	}
	const char16_t *end = str;
	while (*end != u'\0') { ++end; }
	return static_cast<size_t>(end - str);
}

inline constexpr size_t __constexpr_strlen(const char32_t *str) {
	if (str == nullptr) {
		return 0;
	}
	const char32_t *end = str;
	while (*end != u'\0') { ++end; }
	return static_cast<size_t>(end - str);
}

template <typename CharType>
constexpr inline bool __constexpr_chareq(CharType c1, CharType c2) {
	return c1 == c2;
}

template <>
constexpr inline bool __constexpr_chareq<char>(char c1, char c2) {
	return static_cast<unsigned char>(c1) == static_cast<unsigned char>(c2);
}

template <typename CharType>
constexpr inline bool __constexpr_charlt(CharType c1, CharType c2) {
	return c1 < c2;
}

template <>
constexpr inline bool __constexpr_charlt<char>(char c1, char c2) {
	return static_cast<unsigned char>(c1) < static_cast<unsigned char>(c2);
}

template <typename CharType>
constexpr inline int __constexpr_strcompare(const CharType *s1, const CharType *s2, size_t count) {
	return __constexpr_memcmp(s1, s2, count);
}

template <typename CharType>
constexpr inline const CharType *__constexpr_strfind(const CharType *ptr, size_t count,
		const CharType &ch) {
	auto end = ptr + count;

	while (ptr != end) {
		if (*ptr++ == ch) {
			return ptr - 1;
		}
	}
	return nullptr;
}

template <>
constexpr inline const char *__constexpr_strfind<char>(const char *ptr, size_t count,
		const char &ch) {
	return (const char *)__builtin_memchr((void *)ptr, ch, count);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTEXPR_H_
