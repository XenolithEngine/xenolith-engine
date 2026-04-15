/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_COMMON_STRING_SPMETASTRING_H_
#define STAPPLER_COMMON_STRING_SPMETASTRING_H_

#include "SPCore.h"

namespace STAPPLER_VERSIONIZED stappler::metastring {

template <char... Chars>
struct metastring {
	template <typename Interface>
	static constexpr auto string() -> typename Interface::StringType {
		return {Chars...};
	}

	template <typename String>
	static constexpr auto string() -> String {
		return {String::value_type(Chars)...};
	}

	static constexpr sprt::array<char, sizeof...(Chars)> array() { return {{Chars...}}; }

	template <typename Interface>
	auto to_string() -> typename Interface::StringType const {
		return {Chars...};
	}

	constexpr sprt::array<char, sizeof...(Chars)> to_array() const { return {{Chars...}}; }

	operator sprt::__malloc_string() const { return {Chars...}; }
	operator sprt::__pool_string() const { return {Chars...}; }

	constexpr size_t size() const { return sizeof...(Chars); }
};

template <char... Lhs, char... Rhs>
constexpr inline auto merge(const metastring<Lhs...> &, const metastring<Rhs...> &) {
	return metastring<Lhs..., Rhs...>();
}

template <typename... Args, char... Lhs, char... Rhs>
constexpr inline auto merge(const metastring<Lhs...> &lhs, const metastring<Rhs...> &rhs,
		Args &&...args) {
	return merge(lhs, merge(rhs, args...));
}

template <char... Lhs>
constexpr inline auto merge(const metastring<Lhs...> &lhs) {
	return lhs;
}

constexpr inline auto merge() { return metastring<>(); }

constexpr int num_digits(size_t x) { return x < 10 ? 1 : 1 + num_digits(x / 10); }

template <int size, size_t x, char... args>
struct numeric_builder {
	using type = typename numeric_builder<size - 1, x / 10, '0' + (x % 10), args...>::type;
};

template <size_t x, char... args>
struct numeric_builder<2, x, args...> {
	using type = metastring<'0' + x / 10, '0' + (x % 10), args...>;
};

template <size_t x, char... args>
struct numeric_builder<1, x, args...> {
	using type = metastring<'0' + x, args...>;
};

template <size_t x>
using numeric = typename numeric_builder<num_digits(x), x>::type;

} // namespace stappler::metastring


namespace STAPPLER_VERSIONIZED stappler {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

template <typename CharType, CharType... Chars>
auto operator""_meta() {
	return metastring::metastring<Chars...>();
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

} // namespace STAPPLER_VERSIONIZED stappler

namespace sprt {

template <char... Chars>
struct io_traits<STAPPLER_VERSIONIZED_NAMESPACE::metastring::metastring<Chars...>> {
	template <io_character CharType>
	static constexpr size_t length(
			const STAPPLER_VERSIONIZED_NAMESPACE::metastring::metastring<Chars...> &value) {
		return value.size();
	}

	template <io_character CharType>
	static constexpr void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const STAPPLER_VERSIONIZED_NAMESPACE::metastring::metastring<Chars...> &value) {
		auto arr = value.to_array();
		cb(arr);
	}
};

} // namespace sprt

#endif /* STAPPLER_CORE_STRING_SPMETASTRING_H_ */
