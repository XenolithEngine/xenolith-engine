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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMESTREAM_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMESTREAM_H_

#include "SPRuntimeCallback.h"
#include "SPRuntimeUnicode.h"

namespace sprt {

namespace detail {

template <typename FunctionalStreamArg>
struct FunctionalStreamCharTraits { };

template <typename Char>
struct FunctionalStreamCharTraits<StringViewBase<Char>> {
	using CharType = Char;
};

template <>
struct FunctionalStreamCharTraits<BytesView> {
	using CharType = uint8_t;
};

template <typename FunctionalStream>
struct FunctionalStreamTraits { };

template <typename Arg>
struct FunctionalStreamTraits<callback<void(Arg)>> {
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
	typename FunctionalStreamTraits<FunctionalStream>::CharType buf[DOUBLE_MAX_DIGITS];
	auto ret = sprt::dtoa(d, buf, DOUBLE_MAX_DIGITS);
	streamWrite(stream, typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf, ret));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, float f) {
	streamWrite(stream, double(f));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, int64_t i) {
	typename FunctionalStreamTraits<FunctionalStream>::CharType buf[INT_MAX_DIGITS];
	auto ret = sprt::itoa(sprt::int64_t(i), buf, INT_MAX_DIGITS);
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf + INT_MAX_DIGITS - ret,
					ret));
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, uint64_t i) {
	typename FunctionalStreamTraits<FunctionalStream>::CharType buf[INT_MAX_DIGITS];
	auto ret = sprt::itoa(sprt::uint64_t(i), buf, INT_MAX_DIGITS);
	streamWrite(stream,
			typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf + INT_MAX_DIGITS - ret,
					ret));
}

#if SP_HAVE_DEDICATED_SIZE_T
template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, size_t i) {
	typename FunctionalStreamTraits<FunctionalStream>::CharType buf[INT_MAX_DIGITS];
	auto ret = sprt::itoa(sprt::uint64_t(i), buf, INT_MAX_DIGITS);
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
	static constexpr size_t BufSize = 6;
	if constexpr (sizeof(typename FunctionalStreamTraits<FunctionalStream>::CharType) == 1) {
		typename FunctionalStreamTraits<FunctionalStream>::CharType buf[BufSize] = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf8EncodeBuf(buf, BufSize, c)));
	} else {
		typename FunctionalStreamTraits<FunctionalStream>::CharType buf[BufSize] = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf16EncodeBuf(buf, BufSize, c)));
	}
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, char16_t c) {
	if constexpr (sizeof(typename FunctionalStreamTraits<FunctionalStream>::CharType) == 1) {
		static constexpr size_t BufSize = 4;
		typename FunctionalStreamTraits<FunctionalStream>::CharType buf[BufSize] = {0};
		streamWrite(stream,
				typename FunctionalStreamTraits<FunctionalStream>::ArgType(buf.data(),
						sprt::unicode::utf8EncodeBuf(buf, BufSize, c)));
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

inline void streamWrite(const callback<void(BytesView)> &cb, const BytesView &val) { cb(val); }
inline void streamWrite(const callback<void(BytesView)> &cb, const uint8_t &val) {
	cb(BytesView(&val, 1));
}

} // namespace detail

template <typename T, typename ReturnType, typename... ArgumentTypes>
const callback<ReturnType(ArgumentTypes...)> &operator<<(
		const callback<ReturnType(ArgumentTypes...)> &cb, const T &val) {
	static_assert(sizeof...(ArgumentTypes) == 1,
			"Functional stream should accept only one argument");

	detail::streamWrite(cb, val);
	return cb;
}

template <typename CharType, typename Type>
void __processArgs(const callback<void(StringViewBase<CharType>)> &cb, Type &&arg) {
	cb << arg;
}

template <typename CharType, typename Type, typename... Args>
void __processArgs(const callback<void(StringViewBase<CharType>)> &cb, Type &&arg, Args &&...args) {
	__processArgs(cb, forward<Type>(arg));
	__processArgs(cb, forward<Args>(args)...);
}

} // namespace sprt

#endif
