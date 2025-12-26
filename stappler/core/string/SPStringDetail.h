/**
Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_CORE_STRING_SPSTRINGDETAIL_H_
#define CORE_CORE_STRING_SPSTRINGDETAIL_H_

#include "SPHalfFloat.h"
#include "SPRuntimeStringBuffer.h"
#include "SPUnicode.h"
#include "SPBytesReader.h"
#include "SPRuntimeString.h"
#include "SPRuntimePlatform.h"

namespace STAPPLER_VERSIONIZED stappler {

// Fast reader for char string
// Matching function based on templates
//
// Usage:
//   using StringView::Chars;
//   using StringView::Range;
//
//   reader.readUntil<Chars<' ', '\n', '\r', '\t'>>();
//   reader.readChars<Chars<'-', '+', '.', 'e'>, Range<'0', '9'>>();
//

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::platform {

SP_PUBLIC char32_t tolower(char32_t c);
SP_PUBLIC char32_t toupper(char32_t c);
SP_PUBLIC char32_t totitle(char32_t c);

template <typename Interface>
SP_PUBLIC auto tolower(StringView) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto toupper(StringView) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto totitle(StringView) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto tolower(WideStringView) -> typename Interface::WideStringType;

template <typename Interface>
SP_PUBLIC auto toupper(WideStringView) -> typename Interface::WideStringType;

template <typename Interface>
SP_PUBLIC auto totitle(WideStringView) -> typename Interface::WideStringType;

SP_PUBLIC int compare_u(StringView l, StringView r);
SP_PUBLIC int compare_u(WideStringView l, WideStringView r);

SP_PUBLIC int caseCompare_u(StringView l, StringView r);
SP_PUBLIC int caseCompare_u(WideStringView l, WideStringView r);

SP_PUBLIC inline StringView getOsLocale() { return sprt::platform::getOsLocale(); }

} // namespace stappler::platform

namespace STAPPLER_VERSIONIZED stappler::string::detail {

inline char32_t tolower(char32_t c) { return platform::tolower(c); }
inline char32_t toupper(char32_t c) { return platform::toupper(c); }
inline char32_t totitle(char32_t c) { return platform::totitle(c); }

// read number from string and offset pointers

template <typename L, typename R,
		typename CharType = typename std::enable_if<
				std::is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int compare_c(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename std::enable_if<
				std::is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int compare_u(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename std::enable_if<
				std::is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int caseCompare_c(const L &l, const R &r);

template <typename L, typename R,
		typename CharType = typename std::enable_if<
				std::is_same< typename L::value_type, typename R::value_type >::value,
				typename L::value_type>::type>
inline int caseCompare_u(const L &l, const R &r);

template <typename _CharT>
constexpr size_t length(const _CharT *__p) {
	if (!__p) {
		return 0;
	}

	return std::char_traits<_CharT>::length(__p);
}

// Limited length function
// This function can be used to safely obtain the length of a string when its potential maximum length is known
// For such a case, this function will not perform the comparison of infinity but will limit it to the maximum number of characters transmitted
template <typename _CharT>
constexpr size_t length(const _CharT *__p, size_t max) {
	if (!__p) {
		return 0;
	}

	if (max == maxOf<size_t>()) {
		return std::char_traits<_CharT>::length(__p);
	} else {
		size_t __i = 0;
		while (__i < max && __p[__i] != _CharT(0)) { ++__i; }
		return __i;
	}
}

template <>
constexpr size_t length(const uint8_t *__p, size_t max) {
	if (!__p) {
		return 0;
	}

	return max;
}

template <typename T, typename Char>
inline auto readNumber(const Char *ptr, size_t len, int base, uint8_t &offset) -> Result<T> {
	// prevent to read out of bounds, copy symbols to stack buffer
	char buf[32] = {0}; // int64_t/scientific double character length max
	size_t m = min(size_t(31), len);
	size_t i = 0;
	for (; i < m; i++) {
		auto c = ptr[i];
		if (c < 127) {
			buf[i] = c;
		} else {
			break;
		}
	}

	// read number from internal buffer
	char *ret = nullptr;
	auto val = StringToNumber<T>(buf, &ret, base);
	if (*ret == 0) {
		// while string was used
		offset = i;
	} else if (ret && ret != buf) {
		// part of string was used
		offset = ret - buf;
	} else {
		// fail to read number
		offset = 0;
		return Result<T>();
	}
	return Result<T>(val);
}

template <typename CharType>
inline int compare_c(const CharType *lPtr, size_t lSize, const CharType *rPtr, size_t rSize) {
	auto __len = std::min(lSize, rSize);
	auto ret = std::char_traits<CharType>::compare(lPtr, rPtr, __len);
	if (!ret) {
		if (lSize < rSize) {
			return -1;
		} else if (lSize == rSize) {
			return 0;
		} else {
			return 1;
		}
	}
	return ret;
}

template <typename L, typename R, typename CharType>
inline int compare_c(const L &l, const R &r) {
	return compare_c(l.data(), l.size(), r.data(), r.size());
}

template <typename L, typename R, typename CharType>
inline int compare_u(const L &l, const R &r) {
	return platform::compare_u(l, r);
}

template <typename CharType>
inline auto caseCompareChar_c(const std::locale &locale, const CharType &l, const CharType &r) {
	return std::toupper(l, locale) <=> std::toupper(r, locale);
}

// libc++ has no char16_t support, but has wchar_t support
#if _LIBCPP_HAS_WIDE_CHARACTERS

// Specialize comparation template
template <>
inline auto caseCompareChar_c<char16_t>(const std::locale &locale, const char16_t &l,
		const char16_t &r) {
	return std::toupper(wchar_t(l), locale) <=> std::toupper(wchar_t(r), locale);
}

#endif

template <typename L, typename R, typename CharType>
inline int caseCompare_c(const L &l, const R &r) {
	auto &locale = std::locale::classic();

	auto ret = std::lexicographical_compare_three_way(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), [&](const CharType &l, const CharType &r) -> std::strong_ordering {
		return caseCompareChar_c(locale, l, r);
	});

	if (std::is_eq(ret)) {
		return 0;
	} else if (std::is_lt(ret)) {
		return -1;
	} else {
		return 1;
	}
}

template <typename L, typename R, typename CharType>
inline int caseCompare_u(const L &l, const R &r) {
	return platform::caseCompare_u(l, r);
}


template <typename FunctionalStreamArg>
struct FunctionalStreamCharTraits { };

template <typename Char>
struct FunctionalStreamCharTraits<StringViewBase<Char>> {
	using CharType = Char;
};

template <>
struct FunctionalStreamCharTraits<StringViewUtf8> {
	using CharType = StringViewUtf8::CharType;
};

template <Endian E>
struct FunctionalStreamCharTraits<BytesViewTemplate<E>> {
	using CharType = BytesViewTemplate<E>::CharType;
};

template <typename FunctionalStream>
struct FunctionalStreamTraits { };

template <typename Arg>
struct FunctionalStreamTraits<Callback<void(Arg)>> {
	using ArgType = Arg;
	using CharType = typename FunctionalStreamCharTraits<ArgType>::CharType;
};

template <typename Arg>
struct FunctionalStreamTraits<std::function<void(Arg)>> {
	using ArgType = Arg;
	using CharType = typename FunctionalStreamCharTraits<ArgType>::CharType;
};

template <typename Arg>
struct FunctionalStreamTraits<memory::function<void(Arg)>> {
	using ArgType = Arg;
	using CharType = typename FunctionalStreamCharTraits<ArgType>::CharType;
};

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream,
		typename FunctionalStreamTraits<FunctionalStream>::ArgType str) {
	stream(str);
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream,
		const typename FunctionalStreamTraits<FunctionalStream>::CharType *str) {
	streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(str));
}

template <typename FunctionalStream, size_t N>
inline void streamWrite(const FunctionalStream &stream,
		const typename FunctionalStreamTraits<FunctionalStream>::CharType str[N]) {
	streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(str, N));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, double d) {
	std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType, sprt::DOUBLE_MAX_DIGITS>
			buf = {0};
	auto ret = sprt::dtoa(d, buf.data(), buf.size());
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(), ret));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, float f) {
	streamWrite(stream, double(f));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, int64_t i) {
	std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType,
			std::numeric_limits<int64_t>::digits10 + 2>
			buf = {0};
	auto ret = sprt::itoa(sprt::int64_t(i), buf.data(), buf.size());
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(
					buf.data() + buf.size() - ret, ret));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, uint64_t i) {
	std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType,
			std::numeric_limits<int64_t>::digits10 + 2>
			buf = {0};
	auto ret = sprt::itoa(sprt::uint64_t(i), buf.data(), buf.size());
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(
					buf.data() + buf.size() - ret, ret));
}

#if SP_HAVE_DEDICATED_SIZE_T
template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, size_t i) {
	std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType,
			std::numeric_limits<int64_t>::digits10 + 2>
			buf = {0};
	auto ret = string::detail::itoa(sprt::uint64_t(i), buf.data(), buf.size());
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(
					buf.data() + buf.size() - ret, ret));
}

#endif

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, int32_t i) {
	streamWrite(stream, int64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, uint32_t i) {
	streamWrite(stream, uint64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, int16_t i) {
	streamWrite(stream, int64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, uint16_t i) {
	streamWrite(stream, uint64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, int8_t i) {
	streamWrite(stream, int64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, uint8_t i) {
	streamWrite(stream, uint64_t(i));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, char32_t c) {
	if constexpr (sizeof(typename FunctionalStreamTraits<FunctionalStream>::CharType) == 1) {
		std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType, 6> buf = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf8EncodeBuf(buf.data(), buf.size(), c)));
	} else {
		std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType, 6> buf = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf16EncodeBuf(buf.data(), buf.size(), c)));
	}
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, char16_t c) {
	if constexpr (sizeof(typename FunctionalStreamTraits<FunctionalStream>::CharType) == 1) {
		std::array<typename FunctionalStreamTraits<FunctionalStream>::CharType, 4> buf = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf8EncodeBuf(buf.data(), buf.size(), c)));
	} else {
		streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(&c, 1));
	}
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, char c) {
	if constexpr (sizeof(typename FunctionalStreamTraits<FunctionalStream>::CharType) == 1) {
		streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(&c, 1));
	} else {
		char16_t ch = c;
		streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(&ch, 1));
	}
}

SP_PUBLIC void streamWrite(const Callback<void(WideStringView)> &stream, const StringView &c);
SP_PUBLIC void streamWrite(const std::function<void(WideStringView)> &stream, const StringView &c);
SP_PUBLIC void streamWrite(const memory::function<void(WideStringView)> &stream,
		const StringView &c);

inline void streamWrite(const Callback<void(StringViewUtf8)> &stream, const StringView &c) {
	stream(StringViewUtf8(c.data(), c.size()));
}
inline void streamWrite(const std::function<void(StringViewUtf8)> &stream, const StringView &c) {
	stream(StringViewUtf8(c.data(), c.size()));
}
inline void streamWrite(const memory::function<void(StringViewUtf8)> &stream, const StringView &c) {
	stream(StringViewUtf8(c.data(), c.size()));
}

SP_PUBLIC void streamWrite(const Callback<void(StringView)> &stream, const std::type_info &c);
SP_PUBLIC void streamWrite(const Callback<void(WideStringView)> &stream, const std::type_info &c);
SP_PUBLIC void streamWrite(const Callback<void(StringViewUtf8)> &stream, const std::type_info &c);
SP_PUBLIC void streamWrite(const std::function<void(StringView)> &stream, const std::type_info &c);
SP_PUBLIC void streamWrite(const std::function<void(WideStringView)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const std::function<void(StringViewUtf8)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const memory::function<void(StringView)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const memory::function<void(WideStringView)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const memory::function<void(StringViewUtf8)> &stream,
		const std::type_info &c);

inline void streamWrite(const Callback<void(BytesView)> &cb, const BytesView &val) { cb(val); }
inline void streamWrite(const Callback<void(BytesView)> &cb, const uint8_t &val) {
	cb(BytesView(&val, 1));
}

} // namespace stappler::string::detail

namespace STAPPLER_VERSIONIZED stappler {

struct StringComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return string::detail::compare_c(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

struct StringCaseComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return string::detail::caseCompare_c(BytesReader<CharT>(l, len),
				BytesReader<CharT>(r, len));
	}
};

struct StringUnicodeComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return string::detail::compare_u(BytesReader<CharT>(l, len), BytesReader<CharT>(r, len));
	}
};

struct StringUnicodeCaseComparator {
	template <typename CharT>
	static int compare(const CharT *l, const CharT *r, size_t len) {
		return string::detail::caseCompare_u(BytesReader<CharT>(l, len),
				BytesReader<CharT>(r, len));
	}
};

template <typename C>
inline std::basic_ostream<C> &operator<<(std::basic_ostream<C> &os, const StringViewBase<C> &str) {
	return os.write(str.data(), str.size());
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os,
		const sprt::StringView &str) {
	return os.write(str.data(), str.size());
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os,
		const StringViewUtf8 &str) {
	return os.write(str.data(), str.size());
}

#define SP_STRINGVIEW_COMPARE_OP(Type1, Type2) \
	template <typename C> inline bool operator == (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) == 0; } \
	template <typename C> inline bool operator != (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) != 0; } \
	template <typename C> inline bool operator > (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) > 0; } \
	template <typename C> inline bool operator >= (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) >= 0; } \
	template <typename C> inline bool operator < (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) < 0; } \
	template <typename C> inline bool operator <= (const Type1 &l, const Type2 &r) { return string::detail::compare_c(l, r) <= 0; }

#define SP_STRINGVIEW_COMPARE_OP_UTF8(Type1, Type2) \
	inline bool operator == (const Type1 &l, const Type2 &r) { return string::detail::compare_c /* not an error! It's faster */ (l, r) == 0; } \
	inline bool operator != (const Type1 &l, const Type2 &r) { return string::detail::compare_c /* not an error! It's faster */ (l, r) != 0; } \
	inline bool operator > (const Type1 &l, const Type2 &r) { return string::detail::compare_u(l, r) > 0; } \
	inline bool operator >= (const Type1 &l, const Type2 &r) { return string::detail::compare_u(l, r) >= 0; } \
	inline bool operator < (const Type1 &l, const Type2 &r) { return string::detail::compare_u(l, r) < 0; } \
	inline bool operator <= (const Type1 &l, const Type2 &r) { return string::detail::compare_u(l, r) <= 0; }

SP_STRINGVIEW_COMPARE_OP(StringViewBase<C>, StringViewBase<C>)

SP_STRINGVIEW_COMPARE_OP(memory::StandartInterface::BasicStringType<C>, StringViewBase<C>)
SP_STRINGVIEW_COMPARE_OP(StringViewBase<C>, memory::StandartInterface::BasicStringType<C>)

SP_STRINGVIEW_COMPARE_OP(memory::PoolInterface::BasicStringType<C>, StringViewBase<C>)
SP_STRINGVIEW_COMPARE_OP(StringViewBase<C>, memory::PoolInterface::BasicStringType<C>)

SP_STRINGVIEW_COMPARE_OP_UTF8(StringViewUtf8, StringViewUtf8)

SP_STRINGVIEW_COMPARE_OP_UTF8(memory::StandartInterface::BasicStringType<char>, StringViewUtf8)
SP_STRINGVIEW_COMPARE_OP_UTF8(StringViewUtf8, memory::StandartInterface::BasicStringType<char>)

SP_STRINGVIEW_COMPARE_OP_UTF8(memory::PoolInterface::BasicStringType<char>, StringViewUtf8)
SP_STRINGVIEW_COMPARE_OP_UTF8(StringViewUtf8, memory::PoolInterface::BasicStringType<char>)

template <typename C>
inline std::weak_ordering operator<=>(const StringViewBase<C> &l, const StringViewBase<C> &r) {
	auto c = string::detail::compare_c(l, r);
	if (c == 0) {
		return std::weak_ordering::equivalent;
	} else if (c < 0) {
		return std::weak_ordering::less;
	} else {
		return std::weak_ordering::greater;
	}
}

template <typename C>
inline std::weak_ordering operator<=>(const StringViewUtf8 &l, const StringViewUtf8 &r) {
	auto c = string::detail::compare_c(l, r);
	if (c == 0) {
		return std::weak_ordering::equivalent;
	} else if (c < 0) {
		return std::weak_ordering::less;
	} else {
		return std::weak_ordering::greater;
	}
}

template <typename C>
inline bool operator==(const StringViewBase<C> &l, const C *r) {
	return string::detail::compare_c(l, StringViewBase<C>(r)) == 0;
}
template <typename C>
inline bool operator!=(const StringViewBase<C> &l, const C *r) {
	return string::detail::compare_c(l, StringViewBase<C>(r)) != 0;
}

template <typename C>
inline bool operator==(const C *l, const StringViewBase<C> &r) {
	return string::detail::compare_c(StringViewBase<C>(l), r) == 0;
}
template <typename C>
inline bool operator!=(const C *l, const StringViewBase<C> &r) {
	return string::detail::compare_c(StringViewBase<C>(l), r) != 0;
}

inline bool operator==(const StringViewUtf8 &l, const char *r) {
	return string::detail::compare_c(l, StringViewUtf8(r)) == 0;
}
inline bool operator!=(const StringViewUtf8 &l, const char *r) {
	return string::detail::compare_c(l, StringViewUtf8(r)) != 0;
}

inline bool operator==(const char *l, const StringViewUtf8 &r) {
	return string::detail::compare_c(StringViewUtf8(l), r) == 0;
}
inline bool operator!=(const char *l, const StringViewUtf8 &r) {
	return string::detail::compare_c(StringViewUtf8(l), r) != 0;
}

#undef SP_STRINGVIEW_COMPARE_OP
#undef SP_STRINGVIEW_COMPARE_OP_UTF8

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::equals(const CharType *d, size_t l) const {
	return (l == len && Comparator::compare(ptr, d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::equals(const CharType *d) const {
	// Use limited-length for safety
	auto l = string::detail::length(d, len + 1);
	if (l != len) {
		return false;
	}
	return equals<Comparator>(d, len);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::prefix(const CharType *d, size_t l) const {
	return (l <= len && Comparator::compare(ptr, d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::starts_with(const CharType *d) const {
	// Use limited-length for safety
	auto l = string::detail::length(d, len + 1);
	if (l > len) {
		return false;
	}
	return prefix<Comparator>(d, l);
}

template <typename CharT>
template <size_t Count, typename Comparator>
inline bool BytesReader<CharT>::starts_with(const CharType d[Count]) const {
	if (Count > len) {
		return false;
	}
	return prefix<Comparator>(d, Count);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType *d, size_t l) const {
	return (l <= len && Comparator::compare(ptr + (len - l), d, l) == 0);
}

template <typename CharT>
template <typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType *d) const {
	auto l = string::detail::length(d, len + 1);
	if (l > len) {
		return false;
	}
	return ends_with<Comparator>(d, l);
}

template <typename CharT>
template <size_t Count, typename Comparator>
inline bool BytesReader<CharT>::ends_with(const CharType d[Count]) const {
	if (Count > len) {
		return false;
	}
	return ends_with<Comparator>(d, Count);
}

template <typename _CharType>
template <typename Interface, typename... Args>
auto StringViewBase<_CharType>::merge(Args &&...args) ->
		typename Interface::template BasicStringType<CharType> {
	using StringType = typename Interface::template BasicStringType<CharType>;

	StringType ret;
	ret.reserve(_size(forward<Args>(args)...));
	_merge(ret, forward<Args>(args)...);
	return ret;
}

template <typename _CharType>
template <typename Interface, _CharType C, typename... Args>
auto StringViewBase<_CharType>::merge(Args &&...args) ->
		typename Interface::template BasicStringType<CharType> {
	using StringType = typename Interface::template BasicStringType<CharType>;

	StringType ret;
	ret.reserve(_size(forward<Args>(args)...) + sizeof...(Args));
	_mergeWithSep<StringType, C, true>(ret, forward<Args>(args)...);
	return ret;
}


template <typename _CharType>
template <typename T>
inline size_t StringViewBase<_CharType>::__size(const T &t) {
	return t.size();
}

template <typename _CharType>
inline size_t StringViewBase<_CharType>::__size(const CharType *c) {
	return std::char_traits<_CharType>::length(c);
}

template <typename _CharType>
template <typename T, typename... Args>
inline size_t StringViewBase<_CharType>::_size(T &&t) {
	return __size(t);
}

template <typename _CharType>
template <typename T, typename... Args>
inline size_t StringViewBase<_CharType>::_size(T &&t, Args &&...args) {
	return __size(t) + _size(forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, typename T>
inline void StringViewBase<_CharType>::__merge(Buf &buf, T &&t) {
	if (!t.empty()) {
		buf.append(t.data(), t.size());
	}
}

template <typename _CharType>
template <typename Buf>
inline void StringViewBase<_CharType>::__merge(Buf &buf, const CharType *c) {
	if (c) {
		buf.append(c);
	}
}

template <typename _CharType>
template <typename Buf, typename T, typename... Args>
inline void StringViewBase<_CharType>::_merge(Buf &buf, T &&t, Args &&...args) {
	__merge(buf, std::forward<T>(t));
	_merge(buf, forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, typename T>
inline void StringViewBase<_CharType>::_merge(Buf &buf, T &&t) {
	__merge(buf, std::forward<T>(t));
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T>
inline void StringViewBase<_CharType>::__mergeWithSep(Buf &buf, T &&t) {
	Self tmp(t);
	tmp.trimChars<typename Self::template Chars<C>>();
	if (!tmp.empty()) {
		if constexpr (Front) {
			tmp = Self(t);
			tmp.backwardSkipChars<typename Self::template Chars<C>>();

			buf.append(tmp.data(), tmp.size());
		} else {
			tmp = Self(t);
			tmp.trimChars<typename Self::template Chars<C>>();

			if (!buf.empty()) {
				buf.push_back(C);
			}

			buf.append(tmp.data(), tmp.size());
		}
	}
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T, typename... Args>
inline void StringViewBase<_CharType>::_mergeWithSep(Buf &buf, T &&t, Args &&...args) {
	__mergeWithSep<Buf, C, Front>(buf, std::forward<T>(t));
	_mergeWithSep<Buf, C, false>(buf, std::forward<Args>(args)...);
}

template <typename _CharType>
template <typename Buf, _CharType C, bool Front, typename T>
inline void StringViewBase<_CharType>::_mergeWithSep(Buf &buf, T &&t) {
	__mergeWithSep<Buf, C, Front>(buf, std::forward<T>(t));
}


template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const CharType *ptr, size_t len)
: BytesReader<_CharType>(ptr, string::detail::length(ptr, len)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const CharType *ptr, size_t pos,
		size_t len)
: BytesReader<_CharType>(ptr + pos, string::detail::length(ptr + pos, len)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const Self &ptr, size_t pos, size_t len)
: BytesReader<_CharType>(ptr.data() + pos, min(len, ptr.size() - pos)) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const Self &ptr, size_t len)
: BytesReader<_CharType>(ptr.data(), min(len, ptr.size())) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(
		const sprt::StringViewBase<CharType> &str)
: StringViewBase(str.data(), str.size()) { }

template <typename _CharType>
inline constexpr StringViewBase<_CharType>::StringViewBase(const sprt::StringBuffer<CharType> &str)
: StringViewBase(str.data(), str.size()) { }


template <typename _CharType>
StringViewBase<_CharType>::StringViewBase(const PoolString &str)
: StringViewBase(str.data(), str.size()) { }

template <typename _CharType>
StringViewBase<_CharType>::StringViewBase(const StdString &str)
: StringViewBase(str.data(), str.size()) { }

template <typename _CharType>
template <size_t Size>
inline constexpr StringViewBase<_CharType>::StringViewBase(const std::array<CharType, Size> &str)
: StringViewBase(str.data(), str.size()) { }

template <typename _CharType>
constexpr auto StringViewBase<_CharType>::operator=(const sprt::StringViewBase<CharType> &str)
		-> Self & {
	this->set(str);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator=(const PoolString &str) -> Self & {
	this->set(str);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator=(const StdString &str) -> Self & {
	this->set(str);
	return *this;
}

template <typename _CharType>
inline auto StringViewBase<_CharType>::operator=(const CharType *ptr) -> Self & {
	this->set(StringViewBase<_CharType>(ptr));
	return *this;
}

template <typename _CharType>
template <size_t Size>
inline auto StringViewBase<_CharType>::operator=(const CharType c[Size]) -> Self & {
	this->set(StringViewBase<_CharType>(c, Size));
	return *this;
}

template <typename _CharType>
template <size_t Size>
inline constexpr auto StringViewBase<_CharType>::operator=(const std::array<CharType, Size> &str)
		-> Self & {
	this->set(str);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const PoolString &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const StdString &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const Self &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const sprt::StringViewBase<CharType> &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::set(const CharType *p, size_t l) -> Self & {
	this->ptr = p;
	this->len = l;
	return *this;
}

template <typename _CharType>
template <size_t Size>
constexpr auto StringViewBase<_CharType>::set(const std::array<CharType, Size> &str) -> Self & {
	this->ptr = str.data();
	this->len = str.size();
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::pdup(memory::pool_t *p) const -> Self {
	if (!p) {
		p = memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf = (_CharType *)memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::ptolower_c(memory::pool_t *p) const -> Self {
	if (!p) {
		p = memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf = (_CharType *)memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = std::tolower(buf[i], std::locale()); }
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::ptoupper_c(memory::pool_t *p) const -> Self {
	if (!p) {
		p = memory::pool::acquire();
	}
	if (this->size() > 0) {
		auto buf = (_CharType *)memory::pool::palloc(p, (this->size() + 1) * sizeof(_CharType));
		memcpy(buf, this->data(), this->size() * sizeof(_CharType));
		for (size_t i = 0; i < this->size(); ++i) { buf[i] = std::toupper(buf[i], std::locale()); }
		buf[this->size()] = 0;
		return Self(buf, this->size());
	}
	return Self();
}

template <typename _CharType>
template <typename Interface>
auto StringViewBase<_CharType>::str() const ->
		typename Interface::template BasicStringType<CharType> {
	if (this->ptr && this->len > 0) {
		return typename Interface::template BasicStringType<CharType>(this->ptr, this->len);
	} else {
		return typename Interface::template BasicStringType<CharType>();
	}
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator++() -> Self & {
	if (!this->empty()) {
		this->ptr++;
		this->len--;
	}
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator++(int) -> Self {
	auto tmp = *this;
	if (!this->empty()) {
		this->ptr++;
		this->len--;
	}
	return tmp;
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator+=(size_t l) -> Self & {
	this->offset(l);
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const CharType &c) const -> bool {
	return this->len > 0 && *this->ptr == c;
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const CharType *c) const -> bool {
	return this->prefix(c, TraitsType::length(c));
}

template <typename _CharType>
auto StringViewBase<_CharType>::is(const Self &c) const -> bool {
	return this->prefix(c.data(), c.size());
}

template <typename _CharType>
template <_CharType C>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && *this->ptr == C;
}

template <typename _CharType>
template <CharGroupId G>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && MatchCharGroup<G>::match(*this->ptr);
}

template <typename _CharType>
template <typename M>
auto StringViewBase<_CharType>::is() const -> bool {
	return this->len > 0 && M::match(*this->ptr);
}

template <typename _CharType>
auto StringViewBase<_CharType>::begin() const -> Self {
	return Self(this->ptr, this->len);
}

template <typename _CharType>
auto StringViewBase<_CharType>::end() const -> Self {
	return Self(this->ptr + this->len, 0);
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator-(const Self &other) const -> Self {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		return Self(this->ptr, this->ptr - other.ptr);
	}
	return Self();
}

template <typename _CharType>
auto StringViewBase<_CharType>::operator-=(const Self &other) const -> Self & {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		this->len = this->ptr - other.ptr;
		return *this;
	}
	return *this;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readFloat() -> Result<float> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<float>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readDouble() -> Result<double> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<double>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
auto StringViewBase<_CharType>::readInteger(int base) -> Result<int64_t> {
	Self tmp = *this;
	tmp.skipChars<typename Self::template CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<int64_t>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::skipChars() -> void {
	size_t offset = 0;
	while (this->len > offset && match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = std::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::skipUntil() -> void {
	size_t offset = 0;
	while (this->len > offset && !match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = std::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardSkipChars() -> void {
	while (this->len > 0 && match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardSkipUntil() -> void {
	while (this->len > 0 && !match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <typename _CharType>
auto StringViewBase<_CharType>::skipString(const Self &str) -> bool {
	if (!this->ptr) {
		return false;
	}
	if (this->prefix(str.data(), str.size())) {
		auto s = std::min(str.size(), this->len);
		this->ptr += s;
		this->len -= s;
		return true;
	}
	return false;
}

template <typename _CharType>
auto StringViewBase<_CharType>::skipUntilString(const Self &str, bool stopBeforeString) -> bool {
	if (!this->ptr) {
		return false;
	}

	while (this->len > 0 && !this->prefix(str.data(), str.size())) {
		this->ptr += 1;
		this->len -= 1;
	}
	if (this->len > 0 && *this->ptr != 0 && !stopBeforeString) {
		skipString(str);
	}

	return this->len > 0 && *this->ptr != 0;
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename _CharType>
auto StringViewBase<_CharType>::readUntilString(const Self &str) -> Self {
	auto tmp = *this;
	skipUntilString(str);
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename _CharType>
template <typename Separator, typename Callback>
auto StringViewBase<_CharType>::split(const Callback &cb) const -> void {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::trimChars() -> void {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::trimUntil() -> void {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <typename _CharType>
template <typename... Args>
auto StringViewBase<_CharType>::match(CharType c) -> bool {
	return chars::Compose<CharType, Args...>::match(c);
}


inline StringViewUtf8::StringViewUtf8() : BytesReader(nullptr, 0) { }

inline StringViewUtf8::StringViewUtf8(const char *ptr, size_t len)
: BytesReader(ptr, string::detail::length(ptr, len)) { }

inline StringViewUtf8::StringViewUtf8(const char *ptr, size_t pos, size_t len)
: BytesReader(ptr + pos, string::detail::length(ptr + pos, len)) { }

inline StringViewUtf8::StringViewUtf8(const StringViewUtf8 &ptr, size_t len)
: StringViewUtf8(ptr, 0, len) { }

inline StringViewUtf8::StringViewUtf8(const StringViewUtf8 &ptr, size_t pos, size_t len)
: BytesReader(ptr.data() + pos, min(len, ptr.size() - pos)) { }

inline StringViewUtf8::StringViewUtf8(const PoolString &str)
: StringViewUtf8(str.data(), str.size()) { }

inline StringViewUtf8::StringViewUtf8(const StdString &str)
: StringViewUtf8(str.data(), str.size()) { }

inline StringViewUtf8::StringViewUtf8(const StringViewBase<char> &str)
: StringViewUtf8(str.data(), str.size()) { }

inline StringViewUtf8::StringViewUtf8(const sprt::StringView &str)
: StringViewUtf8(str.data(), str.size()) { }

inline auto StringViewUtf8::operator=(const PoolString &str) -> Self & {
	this->set(str);
	return *this;
}
inline auto StringViewUtf8::operator=(const StdString &str) -> Self & {
	this->set(str);
	return *this;
}
inline auto StringViewUtf8::operator=(const Self &str) -> Self & {
	this->set(str);
	return *this;
}
inline auto StringViewUtf8::operator=(const StringViewBase<char> &str) -> Self & {
	this->set(str);
	return *this;
}
inline auto StringViewUtf8::operator=(const sprt::StringView &str) -> Self & {
	this->set(str);
	return *this;
}

inline auto StringViewUtf8::set(const PoolString &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}
inline auto StringViewUtf8::set(const StdString &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}
inline auto StringViewUtf8::set(const Self &str) -> Self & {
	this->set(str.data(), str.size());
	return *this;
}
inline auto StringViewUtf8::set(const char *p, size_t l) -> Self & {
	ptr = p;
	len = l;
	return *this;
}
inline auto StringViewUtf8::set(const StringViewBase<char> &str) -> Self & {
	return set(str.data(), str.size());
}
inline auto StringViewUtf8::set(const sprt::StringView &str) -> Self & {
	return set(str.data(), str.size());
}

inline bool StringViewUtf8::is(const char &c) const { return len > 0 && *ptr == c; }
inline bool StringViewUtf8::is(const char16_t &c) const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == char32_t(c);
}
inline bool StringViewUtf8::is(const char32_t &c) const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == c;
}
inline bool StringViewUtf8::is(const char *c) const {
	return prefix(c, std::char_traits<char>::length(c));
}
inline bool StringViewUtf8::is(const Self &c) const { return prefix(c.data(), c.size()); }

template <char32_t C>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& sprt::unicode::utf8Decode32(ptr, len) == C;
}

template <CharGroupId G>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& chars::CharGroup<MatchCharType, G>::match(sprt::unicode::utf8Decode32(ptr, len));
}

template <typename M>
inline bool StringViewUtf8::is() const {
	return len > 0 && len >= sprt::unicode::utf8_length_data[uint8_t(*ptr)]
			&& M::match(sprt::unicode::utf8Decode32(ptr, len));
}

inline char32_t StringViewUtf8::getChar() const {
	if (!empty()) {
		uint8_t off = 0;
		auto ret = sprt::unicode::utf8Decode32(this->ptr, this->len, off);
		if (off > len) {
			// invalid codepoint in view
			return 0;
		}
		return ret;
	} else {
		return 0;
	}
}

inline char32_t StringViewUtf8::readChar() {
	if (!empty()) {
		uint8_t off = 0;
		auto ret = sprt::unicode::utf8Decode32(this->ptr, this->len, off);
		if (off > len) {
			// invalid codepoint in view
			offset(len);
			return 0;
		}
		offset(off);
		return ret;
	} else {
		return 0;
	}
}

inline auto StringViewUtf8::letter() const -> Self {
	if (this->len > 0) {
		return Self(this->ptr,
				std::min(this->len, size_t(sprt::unicode::utf8_length_data[uint8_t(*ptr)])));
	}
	return Self();
}
template <typename Interface>
inline auto StringViewUtf8::str() const -> typename Interface::StringType {
	if (this->ptr && this->len > 0) {
		return typename Interface::StringType(this->ptr, this->len);
	}
	return typename Interface::StringType();
}

// extend offset functions with unicode support
inline void StringViewUtf8::offset(size_t l) {
	while (l > 0 && len > 0) {
		++(*this);
		--l;
	}
}
inline auto StringViewUtf8::operator++() -> Self & {
	if (len > 0) {
		auto l = std::min(size_t(sprt::unicode::utf8_length_data[uint8_t(*ptr)]), len);
		ptr += l;
		len -= l;
	}
	return *this;
}
inline auto StringViewUtf8::operator++(int) -> Self {
	auto tmp = *this;
	++(*this);
	return tmp;
}
inline auto StringViewUtf8::operator+=(size_t l) -> Self & {
	offset(l);
	return *this;
}

inline bool StringViewUtf8::isSpace() const {
	auto tmp = *this;
	tmp.skipChars<WhiteSpace>();
	return tmp.empty();
}

inline auto StringViewUtf8::begin() const -> Self { return Self(this->ptr, this->len); }
inline auto StringViewUtf8::end() const -> Self { return Self(this->ptr + this->len, 0); }
inline auto StringViewUtf8::operator-(const Self &other) const -> Self {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		return Self(this->ptr, this->ptr - other.ptr);
	}
	return Self();
}
inline auto StringViewUtf8::operator-=(const Self &other) -> Self & {
	if (this->ptr > other.ptr && size_t(this->ptr - other.ptr) < this->len) {
		this->len = this->ptr - other.ptr;
		return *this;
	}
	return *this;
}
inline auto StringViewUtf8::operator*() const -> MatchCharType {
	return sprt::unicode::utf8Decode32(ptr, len);
}

template <typename Callback>
inline void StringViewUtf8::foreach (const Callback &cb) const {
	static_assert(std::is_invocable_v<Callback, MatchCharType>);
	auto p = ptr;
	const auto e = ptr + len;
	while (p < e) {
		const uint8_t mask = sprt::unicode::utf8_length_mask[uint8_t(*p)];
		const uint8_t len = sprt::unicode::utf8_length_data[uint8_t(*p)];
		uint32_t ret = *p++ & mask;
		for (uint8_t c = 1; c < len; ++c) {
			const auto ch = *p++;
			if ((ch & 0xc0) != 0x80) {
				ret = 0;
				break;
			}
			ret <<= 6;
			ret |= (ch & 0x3f);
		}
		cb(MatchCharType(ret));
	}
}

inline size_t StringViewUtf8::code_size() const {
	size_t ret = 0;
	auto p = ptr;
	const auto e = ptr + len;
	while (p < e) {
		++ret;
		p += sprt::unicode::utf8_length_data[uint8_t(*p)];
	}
	return ret;
}

inline StringViewUtf8::operator StringViewBase<char>() const {
	return StringViewBase<char>(ptr, len);
}

inline Result<float> StringViewUtf8::readFloat() {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<float>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}
inline Result<double> StringViewUtf8::readDouble() {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<double>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}
inline Result<int64_t> StringViewUtf8::readInteger(int base) {
	Self tmp = *this;
	tmp.skipChars<CharGroup<CharGroupId::WhiteSpace>>();
	uint8_t offset = 0;
	auto ret = string::detail::readNumber<int64_t>(tmp.ptr, tmp.len, 0, offset);
	this->ptr += offset;
	this->len -= offset;
	return ret;
}

template <typename... Args>
inline void StringViewUtf8::skipChars() {
	uint8_t clen = 0;
	size_t offset = 0;
	while (len > offset
			&& match<Args...>(sprt::unicode::utf8Decode32(ptr + offset, len - offset, clen))
			&& clen > 0) {
		offset += clen;
	}
	auto off = std::min(offset, len);
	len -= off;
	ptr += off;
}

template <typename... Args>
inline void StringViewUtf8::skipUntil() {
	uint8_t clen = 0;
	size_t offset = 0;
	while (len > offset
			&& !match<Args...>(sprt::unicode::utf8Decode32(ptr + offset, len - offset, clen))
			&& clen > 0) {
		offset += clen;
	}
	auto off = std::min(offset, len);
	len -= off;
	ptr += off;
}

template <typename... Args>
inline void StringViewUtf8::backwardSkipChars() {
	uint8_t clen = 0;
	while (this->len > 0 && rv_match_utf8<Args...>(this->ptr, this->len, clen)) {
		if (clen > 0) {
			this->len -= std::min(size_t(clen), this->len);
		} else {
			return;
		}
	}
}

template <typename... Args>
inline void StringViewUtf8::backwardSkipUntil() {
	uint8_t clen = 0;
	while (this->len > 0 && !rv_match_utf8<Args...>(this->ptr, this->len, clen)) {
		if (clen > 0) {
			this->len -= std::min(size_t(clen), this->len);
			;
		} else {
			return;
		}
	}
}

inline bool StringViewUtf8::skipString(const Self &str) {
	if (!ptr) {
		return false;
	}
	if (this->prefix(str.data(), str.size())) {
		auto s = std::min(len, str.size());
		ptr += s;
		len -= s;
		return true;
	}
	return false;
}
inline bool StringViewUtf8::skipUntilString(const Self &str, bool stopBeforeString) {
	if (!ptr) {
		return false;
	}

	while (this->len > 0 && !this->prefix(str.data(), str.size())) {
		this->ptr += 1;
		this->len -= 1;
	}
	if (this->len > 0 && *this->ptr != 0 && !stopBeforeString) {
		skipString(str);
	}

	return this->len > 0 && *this->ptr != 0;
}

template <typename... Args>
inline auto StringViewUtf8::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <typename... Args>
inline auto StringViewUtf8::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

inline auto StringViewUtf8::readUntilString(const Self &str) -> Self {
	auto tmp = *this;
	skipUntilString(str);
	return Self(tmp.data(), tmp.size() - this->size());
}

template <typename Separator, typename Callback>
inline void StringViewUtf8::split(const Callback &cb) const {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <typename... Args>
inline void StringViewUtf8::trimChars() {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <typename... Args>
inline void StringViewUtf8::trimUntil() {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <typename... Args>
inline bool StringViewUtf8::rv_match_utf8(const CharType *ptr, size_t len, uint8_t &offset) {
	size_t nchars = 0;
	while (len > 0) {
		if (!sprt::unicode::isUtf8Surrogate(ptr[len - 1])) {
			++nchars;
			return match<Args...>(sprt::unicode::utf8Decode32(ptr + len - 1, nchars, offset));
		} else {
			--len;
			++nchars;
		}
	}
	offset = 0;
	return false;
}

template <typename... Args>
inline bool StringViewUtf8::match(MatchCharType c) {
	return chars::Compose<MatchCharType, Args...>::match(c);
}

template <Endian Endianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(const uint8_t *p, size_t l)
: BytesReader(p, l) { }

template <Endian Endianess>
BytesViewTemplate<Endianess>::BytesViewTemplate(const PoolBytes &vec)
: BytesReader(vec.data(), vec.size()) { }

template <Endian Endianess>
BytesViewTemplate<Endianess>::BytesViewTemplate(const StdBytes &vec)
: BytesReader(vec.data(), vec.size()) { }

template <Endian Endianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(StringView str)
: BytesReader((const uint8_t *)str.data(), str.size()) { }

template <Endian Endianess>
template <size_t Size>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const std::array<uint8_t, Size> &arr)
: BytesReader(arr.data(), Size) { }

template <Endian Endianess>
template <Endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> &data)
: BytesReader(data.data(), data.size()) { }

template <Endian Endianess>
template <Endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> ptr, size_t len)
: BytesReader(ptr.data(), min(len, ptr.size())) { }

template <Endian Endianess>
template <Endian OtherEndianess>
inline constexpr BytesViewTemplate<Endianess>::BytesViewTemplate(
		const BytesViewTemplate<OtherEndianess> ptr, size_t pos, size_t len)
: BytesReader(ptr.data() + pos, min(len, ptr.size() - pos)) { }


template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator=(const PoolBytes &b) -> Self & {
	return set(b);
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator=(const StdBytes &b) -> Self & {
	return set(b);
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator=(const Self &b) -> Self & {
	return set(b);
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::set(const PoolBytes &vec) -> Self & {
	ptr = vec.data();
	len = vec.size();
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::set(const StdBytes &vec) -> Self & {
	ptr = vec.data();
	len = vec.size();
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::set(const Self &vec) -> Self & {
	ptr = vec.data();
	len = vec.size();
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::set(const uint8_t *p, size_t l) -> Self & {
	ptr = p;
	len = l;
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator++() -> Self & {
	if (len > 0) {
		ptr++;
		len--;
	}
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator++(int) -> Self & {
	if (len > 0) {
		ptr++;
		len--;
	}
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator+=(size_t l) -> Self & {
	if (len > 0) {
		offset(l);
	}
	return *this;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator==(const Self &other) const -> bool {
	return len == other.len
			&& (ptr == other.ptr || memcmp(ptr, other.ptr, len * sizeof(CharType)) == 0);
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::operator!=(const Self &other) const -> bool {
	return !(*this == other);
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::pdup(memory::pool_t *p) const -> Self {
	if (!p) {
		p = memory::pool::acquire();
	}
	auto buf = (uint8_t *)memory::pool::palloc(p, this->size() * sizeof(uint8_t));
	memcpy(buf, this->data(), this->size() * sizeof(uint8_t));
	return Self(buf, this->size());
}

template <Endian Endianess>
template <typename Interface>
auto BytesViewTemplate<Endianess>::bytes() const -> typename Interface::BytesType {
	return typename Interface::BytesType(data(), data() + size());
}

template <Endian Endianess>
template <typename T>
auto BytesViewTemplate<Endianess>::convert(const uint8_t *data) -> T {
	T res;
	memcpy(&res, data, sizeof(T));
	return Converter<T>::Swap(res);
};

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::skipChars() -> void {
	size_t offset = 0;
	while (this->len > offset && match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = std::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::skipUntil() -> void {
	size_t offset = 0;
	while (this->len > offset && !match<Args...>(this->ptr[offset])) { ++offset; }
	auto off = std::min(offset, this->len);
	this->len -= off;
	this->ptr += off;
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardSkipChars() -> void {
	while (this->len > 0 && match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardSkipUntil() -> void {
	while (this->len > 0 && !match<Args...>(this->ptr[this->len - 1])) { --this->len; }
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::readChars() -> Self {
	auto tmp = *this;
	skipChars<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::readUntil() -> Self {
	auto tmp = *this;
	skipUntil<Args...>();
	return Self(tmp.data(), tmp.size() - this->size());
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardReadChars() -> Self {
	auto tmp = *this;
	backwardSkipChars<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::backwardReadUntil() -> Self {
	auto tmp = *this;
	backwardSkipUntil<Args...>();
	return Self(this->data() + this->size(), tmp.size() - this->size());
}

template <Endian Endianess>
template <typename Separator, typename Callback>
auto BytesViewTemplate<Endianess>::split(const Callback &cb) const -> void {
	Self str(*this);
	while (!str.empty()) {
		str.skipChars<Separator>();
		auto tmp = str.readUntil<Separator>();
		if (!tmp.empty()) {
			cb(tmp);
		}
	}
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::trimChars() -> void {
	this->skipChars<Args...>();
	if (!this->empty()) {
		this->backwardSkipChars<Args...>();
	}
}

template <Endian Endianess>
template <uint8_t... Args>
auto BytesViewTemplate<Endianess>::trimUntil() -> void {
	this->skipUntil<Args...>();
	if (!this->empty()) {
		this->backwardSkipUntil<Args...>();
	}
}

template <Endian Endianess>
template <uint8_t Arg, uint8_t... Args>
auto BytesViewTemplate<Endianess>::match(CharType c) -> bool {
	if (c == Arg) {
		return true;
	}
	if constexpr (sizeof...(Args) > 0) {
		return match<Args...>(c);
	}
	return false;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned64() -> uint64_t {
	uint64_t ret = 0;
	if (len >= 8) {
		ret = convert<uint64_t>(ptr);
		ptr += 8;
		len -= 8;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned32() -> uint32_t {
	uint32_t ret = 0;
	if (len >= 4) {
		ret = convert<uint32_t>(ptr);
		ptr += 4;
		len -= 4;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned24() -> uint32_t {
	uint32_t ret = 0;
	if (len >= 3) {
		ret = (*ptr << 16) + (*(ptr + 1) << 8) + *(ptr + 2);
		ptr += 3;
		len -= 3;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned16() -> uint16_t {
	uint16_t ret = 0;
	if (len >= 2) {
		ret = convert<uint16_t>(ptr);
		ptr += 2;
		len -= 2;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readUnsigned() -> uint8_t {
	uint8_t ret = 0;
	if (len > 0) {
		ret = *ptr;
		++ptr;
		--len;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat64() -> double {
	double ret = 0;
	if (len >= 8) {
		ret = convert<double>(ptr);
		ptr += 8;
		len -= 8;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat32() -> float {
	float ret = 0;
	if (len >= 4) {
		ret = convert<float>(ptr);
		ptr += 4;
		len -= 4;
	}
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readFloat16() -> float {
	return halffloat::decode(readUnsigned16());
}

// read null-terminated string
template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readString() -> StringView {
	size_t offset = 0;
	while (len - offset && ptr[offset]) { offset++; }
	StringView ret((const char *)ptr, offset);
	ptr += offset;
	len -= offset;
	if (len && *ptr == 0) {
		++ptr;
		--len;
	}
	return ret;
}

// read fixed-size string
template <Endian Endianess>
auto BytesViewTemplate<Endianess>::readString(size_t s) -> StringView {
	if (len < s) {
		s = len;
	}
	StringView ret((const char *)ptr, s);
	ptr += s;
	len -= s;
	return ret;
}

template <Endian Endianess>
auto BytesViewTemplate<Endianess>::toStringView() const -> StringView {
	return StringView((const char *)ptr, len);
}

template <Endian Endianess>
template <Endian Target>
auto BytesViewTemplate<Endianess>::readBytes(size_t s) -> BytesViewTemplate<Target> {
	if (len < s) {
		s = len;
	}
	BytesViewTemplate<Target> ret(ptr, s);
	ptr += s;
	len -= s;
	return ret;
}

template <Endian Endianess>
template <typename T>
auto BytesViewTemplate<Endianess>::readSpan(size_t s) -> SpanView<T> {
	if (len < s * sizeof(T)) {
		s = len / sizeof(T);
	}

	SpanView<T> ret(reinterpret_cast<const T *>(ptr), s);
	ptr += s * sizeof(T);
	len -= s * sizeof(T);
	return ret;
}

template <typename Compare>
inline int compareDataRanges(const uint8_t *l, size_t __lsize, const uint8_t *r, size_t __rsize,
		const Compare &cmp) {
	return std::lexicographical_compare(l, l + __lsize, r, r + __rsize, cmp);
}

template <Endian Endianess>
inline bool operator==(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) == r;
}

template <Endian Endianess>
inline bool operator==(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) == r;
}

template <Endian Endianess, size_t Size>
inline bool operator==(const std::array<uint8_t, Size> &l, const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) == r;
}

template <Endian Endianess>
inline bool operator==(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return l == BytesViewTemplate<Endianess>(r);
}

template <Endian Endianess>
inline bool operator==(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return l == BytesViewTemplate<Endianess>(r);
}

template <Endian Endianess, size_t Size>
inline bool operator==(const BytesViewTemplate<Endianess> &l, const std::array<uint8_t, Size> &r) {
	return l == BytesViewTemplate<Endianess>(r);
}


template <Endian Endianess>
inline bool operator!=(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) != r;
}

template <Endian Endianess>
inline bool operator!=(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) != r;
}

template <Endian Endianess, size_t Size>
inline bool operator!=(const std::array<uint8_t, Size> &l, const BytesViewTemplate<Endianess> &r) {
	return BytesViewTemplate<Endianess>(l) != r;
}

template <Endian Endianess>
inline bool operator!=(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return l != BytesViewTemplate<Endianess>(r);
}

template <Endian Endianess>
inline bool operator!=(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return l != BytesViewTemplate<Endianess>(r);
}

template <Endian Endianess, size_t Size>
inline bool operator!=(const BytesViewTemplate<Endianess> &l, const std::array<uint8_t, Size> &r) {
	return l != BytesViewTemplate<Endianess>(r);
}


template <Endian Endianess>
inline bool operator<(const BytesViewTemplate<Endianess> &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less<uint8_t>());
}

template <Endian Endianess>
inline bool operator<(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less<uint8_t>());
}

template <Endian Endianess>
inline bool operator<(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less<uint8_t>());
}

template <Endian Endianess>
inline bool operator<(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less<uint8_t>());
}

template <Endian Endianess>
inline bool operator<(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less<uint8_t>());
}


template <Endian Endianess>
inline bool operator<=(const BytesViewTemplate<Endianess> &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator<=(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator<=(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator<=(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator<=(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::less_equal<uint8_t>());
}


template <Endian Endianess>
inline bool operator>(const BytesViewTemplate<Endianess> &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater<uint8_t>());
}

template <Endian Endianess>
inline bool operator>(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater<uint8_t>());
}

template <Endian Endianess>
inline bool operator>(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater<uint8_t>());
}

template <Endian Endianess>
inline bool operator>(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater<uint8_t>());
}

template <Endian Endianess>
inline bool operator>(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater<uint8_t>());
}


template <Endian Endianess>
inline bool operator>=(const BytesViewTemplate<Endianess> &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator>=(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator>=(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator>=(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater_equal<uint8_t>());
}

template <Endian Endianess>
inline bool operator>=(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return std::lexicographical_compare(l.data(), l.data() + l.size(), r.data(),
			r.data() + r.size(), std::greater_equal<uint8_t>());
}

template <typename _Tp>
inline bool operator<(const SpanView<_Tp> &__x, const SpanView<_Tp> &__y) {
	return std::lexicographical_compare(__x.begin(), __x.end(), __y.begin(), __y.end());
}

/// Based on operator<
template <typename _Tp>
inline bool operator>(const SpanView<_Tp> &__x, const SpanView<_Tp> &__y) {
	return __y < __x;
}

/// Based on operator<
template <typename _Tp>
inline bool operator<=(const SpanView<_Tp> &__x, const SpanView<_Tp> &__y) {
	return !(__y < __x);
}

/// Based on operator<
template <typename _Tp>
inline bool operator>=(const SpanView<_Tp> &__x, const SpanView<_Tp> &__y) {
	return !(__x < __y);
}

template <typename Type>
auto makeSpanView(const std::vector<Type> &vec) -> SpanView<Type> {
	return SpanView<Type>(vec);
}

template <typename Type>
auto makeSpanView(const memory::vector<Type> &vec) -> SpanView<Type> {
	return SpanView<Type>(vec);
}

template <typename Type, size_t Size>
auto makeSpanView(const std::array<Type, Size> &vec) -> SpanView<Type> {
	return SpanView<Type>(vec);
}

template <typename Type>
auto makeSpanView(const Type *ptr, size_t size) -> SpanView<Type> {
	return SpanView<Type>(ptr, size);
}

template <typename Type, size_t Size>
auto makeSpanView(const Type (&array)[Size]) -> SpanView<Type> {
	return SpanView<Type>(array, Size);
}

} // namespace STAPPLER_VERSIONIZED stappler

namespace std {

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::StringView> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::StringView &value) const noexcept {
		return hash<string_view>()(string_view(value.data(), value.size()));
	}
};

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::StringViewUtf8> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::StringViewUtf8 &value) const noexcept {
		return hash<string_view>()(string_view(value.data(), value.size()));
	}
};

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::WideStringView> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::WideStringView &value) const noexcept {
		return hash<u16string_view>()(u16string_view(value.data(), value.size()));
	}
};

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
		STAPPLER_VERSIONIZED_NAMESPACE::Endian::Little>> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
			STAPPLER_VERSIONIZED_NAMESPACE::Endian::Little> &value) const noexcept {
		if constexpr (sizeof(size_t) == 4) {
			return stappler::hash::hash32((const char *)value.data(), value.size());
		} else {
			return stappler::hash::hash64((const char *)value.data(), value.size());
		}
	}
};

template <>
struct hash< STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
		STAPPLER_VERSIONIZED_NAMESPACE::Endian::Big>> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
			STAPPLER_VERSIONIZED_NAMESPACE::Endian::Big> &value) const noexcept {
		if constexpr (sizeof(size_t) == 4) {
			return stappler::hash::hash32((const char *)value.data(), value.size());
		} else {
			return stappler::hash::hash64((const char *)value.data(), value.size());
		}
	}
};

template <>
struct hash< STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
		STAPPLER_VERSIONIZED_NAMESPACE::Endian::Mixed>> {
	hash() { }

	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate<
			STAPPLER_VERSIONIZED_NAMESPACE::Endian::Mixed> &value) const noexcept {
		if constexpr (sizeof(size_t) == 4) {
			return stappler::hash::hash32((const char *)value.data(), value.size());
		} else {
			return stappler::hash::hash64((const char *)value.data(), value.size());
		}
	}
};

template <typename Value>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::SpanView<Value>> {
	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::SpanView<Value> &value) {
		return value.hash();
	}
};

} // namespace std

#endif /* CORE_CORE_STRING_SPSTRINGDETAIL_H_ */
