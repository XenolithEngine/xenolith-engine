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

// clang-format off
static constexpr uint8_t s_constexprTolowerTable[] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
	48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,
	96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

static constexpr uint8_t s_constexprToUpperTable[] = {
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
	48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
	80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
	96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
	80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};
// clang-format on

constexpr inline char __constexpr_tolower_c(char c) {
	return static_cast<char>(s_constexprTolowerTable[static_cast<unsigned char>(c)]);
}

constexpr inline char __constexpr_toupper_c(char c) {
	return static_cast<char>(s_constexprToUpperTable[static_cast<unsigned char>(c)]);
}

constexpr inline char16_t __constexpr_tolower_c(char16_t c) {
	return (c & ~0xFF) | static_cast<char16_t>(s_constexprTolowerTable[c & 0xFF]);
}

constexpr inline char16_t __constexpr_toupper_c(char16_t c) {
	return (c & ~0xFF) | static_cast<char16_t>(s_constexprToUpperTable[c & 0xFF]);
}

constexpr inline char32_t __constexpr_tolower_c(char32_t c) {
	return (c & ~0xFF) | static_cast<char32_t>(s_constexprTolowerTable[c & 0xFF]);
}

constexpr inline char32_t __constexpr_toupper_c(char32_t c) {
	return (c & ~0xFF) | static_cast<char32_t>(s_constexprToUpperTable[c & 0xFF]);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTEXPR_H_
