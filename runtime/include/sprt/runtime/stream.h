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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_STREAM_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_STREAM_H_

#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#if __SPRT_USE_STL
#include <typeinfo>
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
#include <cxxabi.h>
#endif
#endif

namespace sprt::detail {

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

inline void streamWrite(const callback<void(StringView)> &cb, const Status &st) {
	status::getStatusDescription(st, cb);
}

inline void streamWrite(const callback<void(BytesView)> &cb, const BytesView &val) { cb(val); }
inline void streamWrite(const callback<void(BytesView)> &cb, const uint8_t &val) {
	cb(BytesView(&val, 1));
}

#if __SPRT_USE_STL

template <typename Stream>
static void printDemangled(const Stream &stream, const std::type_info &t) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
	int status = 0;
	auto name = abi::__cxa_demangle(t.name(), nullptr, nullptr, &status);
	if (status == 0) {
		streamWrite(stream, name);
		::free(name);
	} else {
		streamWrite(stream, t.name());
	}
#else
	streamWrite(stream, t.name());
#endif
}

template <typename FunctionalStream>
inline void streamWrite(const FunctionalStream &stream, const std::type_info &c) {
	printDemangled(stream, c);
}

#endif

} // namespace sprt::detail

namespace sprt {

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

template <typename Container, typename ValueType>
concept TypedContainer =
		is_same_v<remove_cv_t<remove_reference_t<decltype(*(Container().data()))>>, ValueType>;

template <typename CharType>
struct StreamTraits {
	static constexpr inline size_t getSizeFor(const CharType *value) {
		return __constexpr_strlen(value);
	}

	template <size_t N>
	static constexpr inline size_t getSizeFor(const CharType (&str)[N]) {
		return N;
	}

	static constexpr inline size_t getSizeFor(const CharType &value) { return 1; }

	static constexpr inline size_t getSizeFor(const int64_t &value) {
		return sprt::itoa(sprt::int64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const uint64_t &value) {
		return sprt::itoa(sprt::uint64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const int32_t &value) {
		return sprt::itoa(sprt::int64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const uint32_t &value) {
		return sprt::itoa(sprt::uint64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const int16_t &value) {
		return sprt::itoa(sprt::int64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const uint16_t &value) {
		return sprt::itoa(sprt::uint64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const int8_t &value) {
		return sprt::itoa(sprt::int64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const uint8_t &value) {
		return sprt::itoa(sprt::uint64_t(value), (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const double &value) {
		return sprt::dtoa(value, (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const float &value) {
		return sprt::dtoa(value, (CharType *)nullptr, 0);
	}
	static constexpr inline size_t getSizeFor(const Status &value) {
		size_t ret = 0;
		status::getStatusDescription(value, [&](StringView str) { ret = str.size(); });
		return ret;
	}

	template <typename Container>
	requires TypedContainer<Container, CharType>
	static constexpr inline size_t getSizeFor(const Container &c) {
		return c.size();
	}

	template <typename CustomType>
	static constexpr inline size_t getSizeFor(const CustomType &c) {
		size_t s = 0;
		auto fn = [&](StringView str) { s += str.size(); };
		callback<void(StringView)>(fn) << c;
		return s;
	}

	template <typename... Args>
	static constexpr size_t calculateSize(Args &&...args) {
		return (1 + ... + getSizeFor(args));
	}

	template <typename... Args>
	static memory::basic_string<CharType, memory::detail::DynamicAllocator<CharType>> toString(
			Args &&...args) {
		auto bufferSize = calculateSize(forward<Args>(args)...);
		memory::basic_string<CharType, memory::detail::DynamicAllocator<CharType>> ret;
		ret.resize(bufferSize - 1);

		auto target = ret.data();
		auto targetSize = bufferSize;
		__processArgs<CharType>([&](StringViewBase<CharType> str) {
			target = strappend(target, &targetSize, str.data(), str.size());
		}, forward<Args>(args)...);

		ret.resize(target - ret.data());
		return ret;
	}

	template <typename... Args>
	static size_t toStringBuf(CharType *buf, size_t bufferSize, Args &&...args) {
		auto target = buf;
		auto targetSize = bufferSize;
		__processArgs<CharType>([&](StringViewBase<CharType> str) {
			target = strappend(target, &targetSize, str.data(), str.size());
		}, forward<Args>(args)...);
		return target - buf;
	}

	template <typename... Args>
	static constexpr void merge(const callback<void(StringView)> &cb, Args &&...args) {
		auto bufferSize = calculateSize(forward<Args>(args)...);

		auto buf = __sprt_typed_malloca(CharType, bufferSize);

		auto target = buf;
		auto targetSize = bufferSize;
		__processArgs<CharType>([&](StringView str) {
			target = strappend(target, &targetSize, str.data(), str.size());
		}, forward<Args>(args)...);

		cb(StringView(buf, target - buf));

		__sprt_freea(buf);
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STREAM_H_
