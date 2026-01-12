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

#ifndef STAPPLER_CORE_STRING_SPSTRINGVIEW_H_
#define STAPPLER_CORE_STRING_SPSTRINGVIEW_H_

// Umbrella header for StringView implementation

#include "SPMemInterface.h"
#include <sprt/runtime/stringview.h>

namespace STAPPLER_VERSIONIZED stappler {

namespace chars = sprt::chars;

using sprt::BytesReader;
using sprt::StringViewBase;
using sprt::StringView;
using sprt::StringViewUtf8;
using sprt::WideStringView;
using sprt::BytesViewTemplate;
using sprt::BytesView;
using sprt::BytesViewNetwork;
using sprt::BytesViewHost;
using sprt::SpanView;

using sprt::StringComparator;
using sprt::StringCaseComparator;
using sprt::StringUnicodeComparator;
using sprt::StringUnicodeCaseComparator;

using sprt::CharGroupId;

using CallbackStream = Callback<void(StringView)>;

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::string::detail {

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

template <sprt::endian E>
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
struct FunctionalStreamTraits<sprt::memory::function<void(Arg)>> {
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
SP_PUBLIC void streamWrite(const sprt::memory::function<void(WideStringView)> &stream,
		const StringView &c);

inline void streamWrite(const Callback<void(StringViewUtf8)> &stream, const StringView &c) {
	stream(StringViewUtf8(c.data(), c.size()));
}
inline void streamWrite(const std::function<void(StringViewUtf8)> &stream, const StringView &c) {
	stream(StringViewUtf8(c.data(), c.size()));
}
inline void streamWrite(const sprt::memory::function<void(StringViewUtf8)> &stream,
		const StringView &c) {
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
SP_PUBLIC void streamWrite(const sprt::memory::function<void(StringView)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const sprt::memory::function<void(WideStringView)> &stream,
		const std::type_info &c);
SP_PUBLIC void streamWrite(const sprt::memory::function<void(StringViewUtf8)> &stream,
		const std::type_info &c);

inline void streamWrite(const Callback<void(BytesView)> &cb, const BytesView &val) { cb(val); }
inline void streamWrite(const Callback<void(BytesView)> &cb, const uint8_t &val) {
	cb(BytesView(&val, 1));
}

} // namespace stappler::string::detail

namespace sprt {

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

} // namespace sprt

namespace STAPPLER_VERSIONIZED stappler {

template <typename CharType>
inline auto operator<=>(const memory::StandartInterface::BasicStringType<CharType> &l,
		const StringViewBase<CharType> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const memory::PoolInterface::BasicStringType<CharType> &l,
		const StringViewBase<CharType> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const StringViewBase<CharType> &l,
		const memory::StandartInterface::BasicStringType<CharType> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_c(l, r));
}

template <typename CharType>
inline auto operator<=>(const StringViewBase<CharType> &l,
		const memory::PoolInterface::BasicStringType<CharType> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_c(l, r));
}

inline auto operator<=>(const memory::StandartInterface::BasicStringType<char> &l,
		const StringViewUtf8 &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_u(l, r));
}

inline auto operator<=>(const memory::PoolInterface::BasicStringType<char> &l,
		const StringViewUtf8 &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_u(l, r));
}

inline auto operator<=>(const StringViewUtf8 &l,
		const memory::StandartInterface::BasicStringType<char> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_u(l, r));
}

inline auto operator<=>(const StringViewUtf8 &l,
		const memory::PoolInterface::BasicStringType<char> &r) {
	return sprt::__convertIntToTwc(sprt::detail::compare_u(l, r));
}

template <sprt::endian Endianess>
inline auto operator<=>(const BytesViewTemplate<Endianess> &l,
		const memory::PoolInterface::BytesType &r) {
	return sprt::__compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <sprt::endian Endianess>
inline auto operator<=>(const memory::PoolInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return sprt::__compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <sprt::endian Endianess>
inline auto operator<=>(const BytesViewTemplate<Endianess> &l,
		const memory::StandartInterface::BytesType &r) {
	return sprt::__compareDataRanges(l.data(), l.size(), r.data(), r.size());
}

template <sprt::endian Endianess>
inline auto operator<=>(const memory::StandartInterface::BytesType &l,
		const BytesViewTemplate<Endianess> &r) {
	return sprt::__compareDataRanges(l.data(), l.size(), r.data(), r.size());
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
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate< sprt::endian::little>> {
	hash() { }

	constexpr size_t operator()(
			const STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate< sprt::endian::little> &value)
			const noexcept {
		if constexpr (sizeof(size_t) == 4) {
			return sprt::hash32((const char *)value.data(), value.size());
		} else {
			return sprt::hash64((const char *)value.data(), value.size());
		}
	}
};

template <>
struct hash< STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate< sprt::endian::big>> {
	hash() { }

	constexpr size_t operator()(
			const STAPPLER_VERSIONIZED_NAMESPACE::BytesViewTemplate< sprt::endian::big> &value)
			const noexcept {
		if constexpr (sizeof(size_t) == 4) {
			return sprt::hash32((const char *)value.data(), value.size());
		} else {
			return sprt::hash64((const char *)value.data(), value.size());
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

namespace STAPPLER_VERSIONIZED stappler::memory {

template <typename T>
inline auto makeCallback(T &&t) ->
		typename std::enable_if<!std::is_function<T>::value && !std::is_bind_expression<T>::value,
				typename sprt::callback_traits<decltype(&T::operator())>::type>::type {
	using Type = typename sprt::callback_traits<decltype(&T::operator())>::type;

	return Type(std::forward<T>(t));
}

template <typename Sig>
inline auto makeCallback(const std::function<Sig> &fn) {
	return callback<Sig>(fn);
}

template <typename Sig>
inline auto makeCallback(const memory::function<Sig> &fn) {
	return callback<Sig>(fn);
}

template <typename Char>
inline auto makeCallback(std::basic_ostream<Char> &stream) {
	return makeCallback([&](StringViewBase<Char> str) { stream << str; });
}

} // namespace stappler::memory

#endif /* STAPPLER_CORE_STRING_SPSTRINGVIEW_H_ */
