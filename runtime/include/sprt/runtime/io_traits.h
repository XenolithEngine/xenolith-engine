/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_IO_TRAITS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_IO_TRAITS_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/string.h>
#include <sprt/runtime/status.h>
#include <sprt/cxx/cstring>
#include <sprt/cxx/typeinfo>
#include <sprt/cxx/thread>

namespace sprt {

template <typename T>
struct io_traits;

template <>
struct io_traits<void> {
	template <io_character CharType, typename Type>
	static size_t length(const Type &value) {
		if constexpr (requires(const Type &value) {
						  io_traits<Type>::template length<CharType>(value);
					  }) {
			return io_traits<Type>::template length<CharType>(value);
		} else {
			// use 'encode' if there is no quick length acquisition
			size_t ret = 0;
			io_traits<Type>::template encode<CharType>(
					[&](StringViewBase<CharType> str) { ret += str.size(); }, value);
			return ret;
		}
	}

	template <io_character CharType, typename Type>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const Type &value) {
		return io_traits<Type>::encode(cb, value);
	}
};

template <typename T>
struct io_traits<T &> {
	template <io_character CharType>
	static size_t length(const T &value) {
		return io_traits<T>::template length<CharType>(value);
	}

	template <io_character CharType, typename Type>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const T &value) {
		return io_traits<T>::encode(cb, value);
	}
};

template <typename T>
struct io_traits<const T &> {
	template <io_character CharType>
	static size_t length(const T &value) {
		return io_traits<T>::template length<CharType>(value);
	}

	template <io_character CharType, typename Type>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const T &value) {
		return io_traits<T>::encode(cb, value);
	}
};

template <unsigned_integer I>
struct io_traits<I> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 2;

	template <io_character CharType>
	static constexpr size_t length(const I &value) {
		return _itoa::_itoa_len(value);
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const I &value) {
		CharType buf[MAX_DIGITS];
		auto ret = _itoa::unsigned_to_decimal(buf, value, MAX_DIGITS);
		cb(StringViewBase<CharType>(buf + MAX_DIGITS - ret, ret));
	}
};

template <signed_integer I>
struct io_traits<I> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 2;

	template <io_character CharType>
	static constexpr size_t length(const I &value) {
		return _itoa::_itoa_len(value);
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const I &value) {
		CharType buf[MAX_DIGITS];

		size_t ret = 0;
		if (value < 0) {
			ret = _itoa::unsigned_to_decimal(buf, make_unsigned_t<I>(-value), MAX_DIGITS);
			buf[MAX_DIGITS - ret - 1] = '-';
			++ret;
		} else {
			ret = _itoa::unsigned_to_decimal(buf, make_unsigned_t<I>(value), MAX_DIGITS);
		}

		cb(StringViewBase<CharType>(buf + MAX_DIGITS - ret, ret));
	}
};

template <>
struct io_traits<double> {
	static constexpr size_t MAX_DIGITS = DOUBLE_MAX_DIGITS;

	template <io_character CharType>
	static constexpr size_t length(const double &value) {
		return _dtoa::dtoa_len(value);
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const double &value) {
		CharType buf[DOUBLE_MAX_DIGITS];
		auto ret = sprt::dtoa(value, buf, DOUBLE_MAX_DIGITS);
		cb(StringViewBase<CharType>(buf, ret));
	}
};

template <>
struct io_traits<float> {
	template <io_character CharType>
	static constexpr size_t length(const double &value) {
		return io_traits<double>::template length<CharType>(value);
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const double &value) {
		return io_traits<double>::encode(cb, value);
	}
};

template <>
struct io_traits<char> {
	template <io_character CharType>
	static constexpr size_t length(const char &value) {
		return 1;
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const char &value) {
		CharType out(value);
		cb(StringViewBase<CharType>(&out, 1));
	}
};

template <>
struct io_traits<char16_t> {
	template <io_character CharType>
	static constexpr size_t length(const char16_t &value) {
		if constexpr (sizeof(CharType) == 4) {
			return 1;
		} else if constexpr (sizeof(CharType) == 2) {
			return 1;
		} else if constexpr (sizeof(CharType) == 1) {
			return unicode::utf8EncodeLength(value);
		} else {
			static_assert(false, "unknown output char type");
		}
		return 1;
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const char16_t &value) {
		if constexpr (sizeof(CharType) == 4) {
			CharType out(value);
			cb(StringViewBase<CharType>(&out, 1));
		} else if constexpr (sizeof(CharType) == 2) {
			CharType out(value);
			cb(StringViewBase<CharType>(&out, 1));
		} else if constexpr (sizeof(CharType) == 1) {
			auto bufSize = unicode::utf8EncodeLength(value) + 1;
			auto buf = __sprt_typed_malloca(char, bufSize);
			auto ret = unicode::utf8EncodeBuf(buf, bufSize, value);
			cb(StringViewBase<CharType>(buf, ret));
			__sprt_freea(buf);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <>
struct io_traits<char32_t> {
	template <io_character CharType>
	static constexpr size_t length(const char32_t &value) {
		if constexpr (sizeof(CharType) == 4) {
			return 1;
		} else if constexpr (sizeof(CharType) == 2) {
			return unicode::utf16EncodeLength(value);
		} else if constexpr (sizeof(CharType) == 1) {
			return unicode::utf8EncodeLength(value);
		} else {
			static_assert(false, "unknown output char type");
		}
		return 1;
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const char32_t &value) {
		if constexpr (sizeof(CharType) == 4) {
			CharType out(value);
			cb(StringViewBase<CharType>(&out, 1));
		} else if constexpr (sizeof(CharType) == 2) {
			auto bufSize = unicode::utf16EncodeLength(value) + 1;
			auto buf = __sprt_typed_malloca(char16_t, bufSize);
			auto ret = unicode::utf16EncodeBuf(buf, bufSize, value);
			cb(StringViewBase<CharType>(buf, ret));
			__sprt_freea(buf);
		} else if constexpr (sizeof(CharType) == 1) {
			auto bufSize = unicode::utf8EncodeLength(value) + 1;
			auto buf = __sprt_typed_malloca(char, bufSize);
			auto ret = unicode::utf8EncodeBuf(buf, bufSize, value);
			cb(StringViewBase<CharType>(buf, ret));
			__sprt_freea(buf);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <>
struct io_traits<StringView> {
	template <io_character CharType>
	static constexpr size_t length(const StringView &value) {
		return value.size();
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const StringView &value) {
		if constexpr (sizeof(CharType) == 2 || sizeof(CharType) == 4) {
			auto buf = __sprt_typed_malloca(CharType, value.size() + 1);
			auto target = buf;
			auto source = value.data();
			auto end = source + value.size();
			while (source != end) { *target++ = *source++; }
			*target = 0;
			cb(StringViewBase<CharType>(buf, value.size()));
			__sprt_freea(buf);
		} else if constexpr (sizeof(CharType) == 1) {
			cb(value);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <>
struct io_traits<WideStringView> {
	template <io_character CharType>
	static constexpr size_t length(const WideStringView &value) {
		if constexpr (sizeof(CharType) == 2 || sizeof(CharType) == 4) {
			return value.size();
		} else {
			return unicode::getUtf8Length(value);
		}
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const WideStringView &value) {
		if constexpr (sizeof(CharType) == 4) {
			auto buf = __sprt_typed_malloca(CharType, value.size() + 1);
			auto target = buf;
			auto source = value.data();
			auto end = source + value.size();
			while (source != end) { *target++ = *source++; }
			*target = 0;
			cb(StringViewBase<CharType>(buf, value.size()));
			__sprt_freea(buf);
		} else if constexpr (sizeof(CharType) == 2) {
			cb(value);
		} else if constexpr (sizeof(CharType) == 1) {
			unicode::toUtf8(cb, value);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <>
struct io_traits<StringViewBase<char32_t>> {
	template <io_character CharType>
	static constexpr size_t length(const StringViewBase<char32_t> &value) {
		if constexpr (sizeof(CharType) == 4) {
			return value.size();
		} else if constexpr (sizeof(CharType) == 2) {
			return unicode::getUtf16Length(value);
		} else {
			return unicode::getUtf8Length(value);
		}
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const StringViewBase<char32_t> &value) {
		if constexpr (sizeof(CharType) == 4) {
			cb(value);
		} else if constexpr (sizeof(CharType) == 2) {
			unicode::toUtf16(cb, value);
		} else if constexpr (sizeof(CharType) == 1) {
			unicode::toUtf8(cb, value);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <>
struct io_traits<StringViewUtf8> {
	template <io_character CharType>
	static constexpr size_t length(const StringViewUtf8 &value) {
		if constexpr (sizeof(CharType) == 4) {
			return unicode::getUtf32Length(value);
		} else if constexpr (sizeof(CharType) == 2) {
			return unicode::getUtf16Length(value);
		} else if constexpr (sizeof(CharType) == 1) {
			return value.size();
		} else {
			static_assert(false, "unknown output char type");
		}
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const StringViewUtf8 &value) {
		if constexpr (sizeof(CharType) == 4) {
			unicode::toUtf32(cb, value);
		} else if constexpr (sizeof(CharType) == 2) {
			unicode::toUtf16(cb, value);
		} else if constexpr (sizeof(CharType) == 1) {
			cb(value);
		} else {
			static_assert(false, "unknown output char type");
		}
	}
};

template <io_character C>
struct io_traits<C *> {
	template <io_character CharType>
	static constexpr size_t length(C *value) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb, C *value) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value));
	}
};

template <io_character C>
struct io_traits<const C *> {
	template <io_character CharType>
	static constexpr size_t length(const C *value) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const C *value) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value));
	}
};

template <io_character C, size_t N>
struct io_traits<const C (&)[N]> {
	template <io_character CharType>
	static constexpr size_t length(const C (&value)[N]) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value, N));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const C (&value)[N]) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value, N));
	}
};

template <io_character C, size_t N>
struct io_traits<C[N]> {
	template <io_character CharType>
	static constexpr size_t length(const C value[N]) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value, N));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const C value[N]) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value, N));
	}
};

template <io_character C, size_t N>
struct io_traits<const C[N]> {
	template <io_character CharType>
	static constexpr size_t length(const C value[N]) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value, N));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const C value[N]) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value, N));
	}
};

template <io_character StringCharType, typename Allocator>
struct io_traits<__basic_string<StringCharType, Allocator>> {
	template <io_character CharType>
	static constexpr size_t length(const __basic_string<StringCharType, Allocator> &str) {
		return io_traits<StringViewBase<StringCharType>>::template length<CharType>(
				StringViewBase<StringCharType>(str));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const __basic_string<StringCharType, Allocator> &str) {
		io_traits<StringViewBase<StringCharType>>::encode(cb, StringViewBase<StringCharType>(str));
	}
};

template <>
struct io_traits<type_info> {
	template <io_character CharType>
	static constexpr size_t length(const type_info &value) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
		int status = 0;
		auto name = abi::__cxa_demangle(value.name(), nullptr, nullptr, &status);
		if (status == 0) {
			auto ret = strlen(name);
			::__sprt_free(name);
			return ret;
		} else {
			return strlen(value.name());
		}
#else
		return strlen(value.name());
#endif
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const type_info &value) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
		int status = 0;
		auto name = abi::__cxa_demangle(value.name(), nullptr, nullptr, &status);
		if (status == 0) {
			io_traits<StringView>::encode(cb, StringView(name));
			::__sprt_free(name);
		} else {
			io_traits<StringView>::encode(cb, StringView(value.name()));
		}
#else
		io_traits<StringView>::encode(cb, StringView(value.name()));
#endif
	}
};

template <>
struct io_traits<Status> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const Status &value) {
		status::getStatusDescription(value,
				[&](StringView str) { io_traits<StringView>::encode(cb, str); });
	}
};

template <>
struct io_traits<thread::id> {
	template <io_character CharType>
	static constexpr size_t length(const thread::id &value) {
		return io_traits<thread::id::__type>::template length<CharType>(value.__native);
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const thread::id &value) {
		return io_traits<thread::id::__type>::template encode<CharType>(cb, value.__native);
	}
};

} // namespace sprt

#endif
