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
#include <sprt/runtime/io_traits.h>

namespace sprt {

template <typename T, typename StringType>
inline const callback<void(StringType)> &operator<<(const callback<void(StringType)> &cb,
		const T &val) {
	static_assert(sprt::is_same_v<StringType, StringViewBase<char>>
					|| sprt::is_same_v<StringType, StringViewBase<char16_t>>
					|| sprt::is_same_v<StringType, StringViewBase<char32_t>>
					|| sprt::is_same_v<StringType, StringViewUtf8>
					|| sprt::is_same_v<StringType, BytesView>,
			"Functional stream argument should be one of StringView, WideStringView, "
			"StringViewBase<char32_t>, StringViewUtf8, BytesView");

	static_assert(requires(const callback<void(StringType)> &cb,
						  const T &val) { io_traits<T>::encode(cb, val); },
			"sprt::io_traits<T> is not defined correctly; Be sure that `encode` function "
			"defined with correct callback and argument type");

	io_traits<T>::encode(cb, val);
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
	template <typename... Args>
	static constexpr size_t calculateSize(Args &&...args) {
		return (1 + ... + io_traits<void>::template length<CharType>(args));
	}

	template <typename String, typename... Args>
	static auto toString(Args &&...args) -> String {
		static_assert(sprt::is_same_v<typename String::value_type, CharType>,
				"Invalid string type");
		auto bufferSize = calculateSize(sprt::forward<Args>(args)...);
		String ret;
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
	static auto allocateStringView(memory::pool_t *pool, Args &&...args) -> StringView {
		auto bufferSize = calculateSize(sprt::forward<Args>(args)...) + 1;
		auto buf = (char *)memory::pool::palloc(pool, bufferSize);
		bufferSize = toStringBuf(buf, bufferSize, sprt::forward<Args>(args)...);
		return StringView(buf, bufferSize);
	}

	template <typename... Args>
	static constexpr void merge(const callback<void(StringView)> &cb, Args &&...args) {
		auto bufferSize = calculateSize(sprt::forward<Args>(args)...);

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

template <__SPRT_ID(FILE) * (*StreamGetter)() >
struct __c_ostream_wrapper {
	void operator()(StringView str) const {
		__sprt_fwrite(str.data(), 1, str.size(), StreamGetter()); //
	}
};

static auto cout = makeCallback(__c_ostream_wrapper<&__sprt_stdout_impl>{});
static auto cerr = makeCallback(__c_ostream_wrapper<&__sprt_stderr_impl>{});

template <typename CharType, typename Allocator>
struct __basic_streambuf : public __basic_string<CharType, Allocator> {
	using __basic_string<CharType, Allocator>::__basic_string;

	void operator()(StringViewBase<CharType> str) { this->append(str.data(), str.size()); }
};

template <typename CharType, typename Allocator>
struct __basic_stringstream
: public callback_storage<void, sizeof(__basic_streambuf<CharType, Allocator>),
		  alignof(__basic_streambuf<CharType, Allocator>), StringViewBase<CharType>> {

	using string_type = __basic_string<CharType, Allocator>;
	using buffer_type = __basic_streambuf<CharType, Allocator>;
	using storage_type = callback_storage<void, sizeof(__basic_streambuf<CharType, Allocator>),
			alignof(__basic_streambuf<CharType, Allocator>), StringViewBase<CharType>>;

	__basic_stringstream() : storage_type(buffer_type()) { }
	~__basic_stringstream() { }

	const string_type &str() const { return this->template getFunction<buffer_type>(); }

	bool empty() const { return this->template getFunction<buffer_type>().empty(); }

	void clear() { this->template getFunction<buffer_type>().clear(); }

	void write(const char *str, size_t len) { (*this)(StringView(str, len)); }

	void reserve(size_t s) { this->template getFunction<buffer_type>().reserve(s); }

	StringViewBase<CharType> weak() const {
		return StringViewBase<CharType>(this->template getFunction<buffer_type>());
	}
};

using __pool_stringstream = __basic_stringstream<char, detail::AllocatorPool<char>>;
using __pool_u16stringstream = __basic_stringstream<char16_t, detail::AllocatorPool<char16_t>>;
using __pool_u32stringstream = __basic_stringstream<char32_t, detail::AllocatorPool<char32_t>>;

using __malloc_stringstream = __basic_stringstream<char, detail::AllocatorMalloc<char>>;
using __malloc_u16stringstream = __basic_stringstream<char16_t, detail::AllocatorMalloc<char16_t>>;
using __malloc_u32stringstream = __basic_stringstream<char32_t, detail::AllocatorMalloc<char32_t>>;

template <typename CharType>
using __pool_basic_stringstream = __basic_stringstream<CharType, detail::AllocatorPool<char>>;

template <typename CharType>
using __malloc_basic_stringstream = __basic_stringstream<CharType, detail::AllocatorMalloc<char>>;

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_STREAM_H_
