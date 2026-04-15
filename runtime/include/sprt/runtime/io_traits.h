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
#include <sprt/cxx/atomic>

namespace sprt {

enum class io_int_base {
	decimial,
	hex,
};

template <signed_or_unsigned_integer T>
struct io_int {
	T value;
	io_int_base base = io_int_base::decimial;
};


template <signed_or_unsigned_integer T>
inline io_int<T> io_hex(T value) {
	return io_int<T>{value, io_int_base::hex};
}

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
			io_traits<void>::template encode<CharType>(
					[&](StringViewBase<CharType> str) { ret += str.size(); }, value);
			return ret;
		}
	}

	template <io_character CharType, typename Type>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const Type &value) {
		io_traits<Type>::encode(cb, value);
	}

	template <typename Type>
	static void encode(const callback<void(StringViewUtf8)> &cb, const Type &value) {
		io_traits<Type>::encode(cb, value);
	}

	template <sprt::endian E, typename Type>
	static void encode(const callback<void(BytesViewTemplate<E>)> &cb, const Type &value) {
		io_traits<Type>::encode(cb, value);
	}
};

template <typename T>
struct io_traits<T &> {
	template <io_character CharType>
	static size_t length(const T &value) {
		return io_traits<T>::template length<CharType>(value);
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}

	template <sprt::endian E>
	static void encode(const callback<void(BytesViewTemplate<E>)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}
};

template <typename T>
struct io_traits<const T &> {
	template <io_character CharType>
	static size_t length(const T &value) {
		return io_traits<T>::template length<CharType>(value);
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}

	template <sprt::endian E>
	static void encode(const callback<void(BytesViewTemplate<E>)> &cb, const T &value) {
		io_traits<T>::encode(cb, value);
	}
};

template <typename T>
struct io_traits<atomic<T>> {
	template <io_character CharType>
	static size_t length(const atomic<T> &value) {
		return io_traits<T>::template length<CharType>(value.load());
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const atomic<T> &value) {
		io_traits<T>::encode(cb, value.load());
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const atomic<T> &value) {
		io_traits<T>::encode(cb, value.load());
	}

	template <sprt::endian E>
	static void encode(const callback<void(BytesViewTemplate<E>)> &cb, const atomic<T> &value) {
		io_traits<T>::encode(cb, value.load());
	}
};

template <enumeration E>
struct io_traits<E> {
	template <io_character CharType>
	static size_t length(const E &value) {
		return io_traits<sprt::underlying_type_t<E>>::template length<CharType>(
				sprt::to_underlying(value));
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const E &value) {
		io_traits<sprt::underlying_type_t<E>>::encode(cb, sprt::to_underlying(value));
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const E &value) {
		io_traits<sprt::underlying_type_t<E>>::encode(cb, sprt::to_underlying(value));
	}

	template <sprt::endian Endian>
	static void encode(const callback<void(BytesViewTemplate<Endian>)> &cb, const E &value) {
		io_traits<sprt::underlying_type_t<E>>::encode(cb, sprt::to_underlying(value));
	}
};

template <unsigned_integer I>
struct io_traits<I> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 2;

	template <io_character CharType>
	static constexpr size_t length(const I &value) {
		return _itoa::_itoa_len(value);
	}

	template <io_character CharType, typename View>
	static constexpr void __encode(const callback<void(View)> &cb, const I &value) {
		CharType buf[MAX_DIGITS + 1];
		auto ret = _itoa::unsigned_to_decimal(buf, value, MAX_DIGITS);
		cb(StringViewBase<CharType>(buf + MAX_DIGITS - ret, ret));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const I &value) {
		__encode<CharType>(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const I &value) {
		__encode<char>(cb, value);
	}

	template <sprt::endian E>
	static void encode(const callback<void(BytesViewTemplate<E>)> &cb, const I &value) {
		if constexpr (E == sprt::endian::native) {
			cb(BytesViewTemplate<E>(reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
		} else {
			I v = byteswap(value);
			cb(BytesViewTemplate<E>(reinterpret_cast<const uint8_t *>(&v), sizeof(v)));
		}
	}
};

template <unsigned_integer I>
struct io_traits<io_int<I>> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 4;

	template <io_character CharType>
	static constexpr size_t length(const io_int<I> &value) {
		switch (value.base) {
		case io_int_base::decimial: return _itoa::_itoa_len(value.value); break;
		case io_int_base::hex: return _itoa::_itoa_hex_len(value.value) + 2; break;
		}
	}

	template <io_character CharType, typename View>
	static constexpr void __encode(const callback<void(View)> &cb, const io_int<I> &value) {
		CharType buf[MAX_DIGITS + 1];
		size_t ret = 0;
		switch (value.base) {
		case io_int_base::decimial:
			ret = _itoa::unsigned_to_decimal(buf, value.value, MAX_DIGITS);
			break;
		case io_int_base::hex:
			ret = _itoa::unsigned_to_hex(buf, value.value, MAX_DIGITS, false);
			buf[MAX_DIGITS - ret++ - 1] = 'x';
			buf[MAX_DIGITS - ret++ - 1] = '0';
			break;
		}

		cb(StringViewBase<CharType>(buf + MAX_DIGITS - ret, ret));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const io_int<I> &value) {
		__encode<CharType>(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const io_int<I> &value) {
		__encode<char>(cb, value);
	}
};

template <signed_integer I>
struct io_traits<I> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 2;

	template <io_character CharType>
	static constexpr size_t length(const I &value) {
		return _itoa::_itoa_len(value);
	}

	template <io_character CharType, typename View>
	static constexpr void __encode(const callback<void(View)> &cb, const I &value) {
		CharType buf[MAX_DIGITS + 1];

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

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const I &value) {
		__encode<CharType>(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const I &value) {
		__encode<char>(cb, value);
	}
};

template <signed_integer I>
struct io_traits<io_int<I>> {
	static constexpr size_t MAX_DIGITS = Digits10<I> + 4;

	template <io_character CharType>
	static constexpr size_t length(const io_int<I> &value) {
		switch (value.base) {
		case io_int_base::decimial: return _itoa::_itoa_len(value.value); break;
		case io_int_base::hex: return _itoa::_itoa_hex_len(value.value) + 2; break;
		}
	}

	template <io_character CharType, typename View>
	static constexpr void __encode(const callback<void(View)> &cb, const io_int<I> &value) {
		CharType buf[MAX_DIGITS + 1];
		size_t ret = 0;
		switch (value.base) {
		case io_int_base::decimial:
			if (value.value < 0) {
				ret = _itoa::unsigned_to_decimal(buf, make_unsigned_t<I>(-value.value), MAX_DIGITS);
				buf[MAX_DIGITS - ret++ - 1] = '-';
			} else {
				ret = _itoa::unsigned_to_decimal(buf, make_unsigned_t<I>(value.value), MAX_DIGITS);
			}
			break;
		case io_int_base::hex:
			if (value.value < 0) {
				ret = _itoa::unsigned_to_hex(buf, make_unsigned_t<I>(-value.value), MAX_DIGITS,
						false);
				buf[MAX_DIGITS - ret++ - 1] = 'x';
				buf[MAX_DIGITS - ret++ - 1] = '0';
				buf[MAX_DIGITS - ret++ - 1] = '-';
			} else {
				ret = _itoa::unsigned_to_hex(buf, make_unsigned_t<I>(value.value), MAX_DIGITS,
						false);
				buf[MAX_DIGITS - ret++ - 1] = 'x';
				buf[MAX_DIGITS - ret++ - 1] = '0';
			}
			break;
		}

		cb(StringViewBase<CharType>(buf + MAX_DIGITS - ret, ret));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const io_int<I> &value) {
		__encode<CharType>(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const io_int<I> &value) {
		__encode<char>(cb, value);
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

	static void encode(const callback<void(StringViewUtf8)> &cb, const double &value) {
		char buf[DOUBLE_MAX_DIGITS];
		auto ret = sprt::dtoa(value, buf, DOUBLE_MAX_DIGITS);
		cb(StringViewUtf8(buf, ret));
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
			const float &value) {
		io_traits<double>::encode(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const float &value) {
		io_traits<double>::encode(cb, value);
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

	static void encode(const callback<void(StringViewUtf8)> &cb, const char &value) {
		cb(StringViewUtf8(&value, 1));
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

	static void encode(const callback<void(StringViewUtf8)> &cb, const char16_t &value) {
		auto bufSize = unicode::utf8EncodeLength(value) + 1;
		auto buf = __sprt_typed_malloca(char, bufSize);
		auto ret = unicode::utf8EncodeBuf(buf, bufSize, value);
		cb(StringViewUtf8(buf, ret));
		__sprt_freea(buf);
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
	static void encode(const callback<void(StringViewUtf8)> &cb, const char32_t &value) {
		auto bufSize = unicode::utf8EncodeLength(value) + 1;
		auto buf = __sprt_typed_malloca(char, bufSize);
		auto ret = unicode::utf8EncodeBuf(buf, bufSize, value);
		cb(StringViewUtf8(buf, ret));
		__sprt_freea(buf);
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

	static void encode(const callback<void(StringViewUtf8)> &cb, const StringView &value) {
		cb(value);
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

	static void encode(const callback<void(StringViewUtf8)> &cb, const WideStringView &value) {
		unicode::toUtf8(cb, value);
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

	static void encode(const callback<void(StringViewUtf8)> &cb,
			const StringViewBase<char32_t> &value) {
		unicode::toUtf8(cb, value);
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb,
			const StringViewUtf8 &value) {
		cb(value);
	}
};

template <sprt::endian OrgignalEndian>
struct io_traits<BytesViewTemplate<OrgignalEndian>> {
	template <sprt::endian TargetEndian>
	static constexpr void encode(const callback<void(BytesViewTemplate<TargetEndian>)> &cb,
			const BytesViewTemplate<OrgignalEndian> &value) {
		cb(value);
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb, C *value) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value));
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb, const C *value) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value));
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb, const C (&value)[N]) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value, N));
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb, const C value[N]) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value, N));
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb, const C value[N]) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value, N));
	}
};

template <io_character C, size_t N>
struct io_traits<sprt::array<C, N>> {
	template <io_character CharType>
	static constexpr size_t length(const sprt::array<C, N> value) {
		return io_traits<StringViewBase<C>>::template length<CharType>(StringViewBase<C>(value, N));
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const sprt::array<C, N> value) {
		io_traits<StringViewBase<C>>::encode(cb, StringViewBase<C>(value, N));
	}

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb,
			const sprt::array<C, N> value) {
		io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(value, N));
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

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb,
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

template <typename T>
struct io_traits<const T *> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const T *value) {
		cb << "(" << typeid(const T *) << ")" << sprt::io_hex(reinterpret_cast<uintptr_t>(value));
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const T *value) {
		cb << "(" << typeid(const T *) << ")" << sprt::io_hex(reinterpret_cast<uintptr_t>(value));
	}
};

template <typename T>
struct io_traits<T *> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, T *value) {
		cb << "(" << typeid(T *) << ")" << sprt::io_hex(reinterpret_cast<uintptr_t>(value));
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, T *value) {
		cb << "(" << typeid(T *) << ")" << sprt::io_hex(reinterpret_cast<uintptr_t>(value));
	}
};

template <>
struct io_traits<Status> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb, const Status &value) {
		status::getStatusDescription(value,
				[&](StringView str) { io_traits<StringView>::encode(cb, str); });
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const Status &value) {
		status::getStatusDescription(value, [&](StringView str) {
			io_traits<StringViewUtf8>::encode(cb, StringViewUtf8(str));
		});
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
		io_traits<thread::id::__type>::template encode<CharType>(cb, value.__native);
	}

	static constexpr void encode(const callback<void(StringViewUtf8)> &cb,
			const thread::id &value) {
		io_traits<thread::id::__type>::encode(cb, value.__native);
	}
};

} // namespace sprt

#endif
