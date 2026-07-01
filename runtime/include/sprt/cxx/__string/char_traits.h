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

// sprt::char_traits — the default character-traits type used by sprt::__basic_string
// and re-exported unchanged as std::char_traits by the <string> STL wrapper. It carries
// no allocator policy, so it is a plain library type. Comparison and search delegate to
// the __constexpr_* primitives, which order `char` as unsigned char and multi-byte
// element types by element value (endianness-independent) per [char.traits]. The
// pos_type/off_type/state_type typedefs exist for conformance; this freestanding layer
// has no streams or codecvt, so state_type is a minimal placeholder rather than mbstate_t.
// The primary template is left undefined ([char.traits]/1); only the standard
// specializations are provided.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___STRING_CHAR_TRAITS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___STRING_CHAR_TRAITS_H_

#include <sprt/cxx/cstddef>
#include <sprt/cxx/cstdint>
#include <sprt/cxx/compare>
#include <sprt/cxx/detail/constexpr.h>
#include <sprt/c/bits/__sprt_wint_t.h>

namespace sprt {

using streamoff = long long;

// Minimal multibyte-conversion state. The standard names char_traits::state_type
// mbstate_t; this freestanding layer has no codecvt that would consume it.
struct __sprt_mbstate {
	unsigned long __wch = 0;
	unsigned char __count = 0;
};

template <typename _State>
class fpos {
	streamoff __off_ = 0;
	_State __st_ {};

public:
	constexpr fpos() noexcept = default;
	constexpr fpos(streamoff __off) noexcept : __off_(__off) { }

	constexpr operator streamoff() const noexcept { return __off_; }
	constexpr _State state() const noexcept { return __st_; }
	constexpr void state(_State __s) noexcept { __st_ = __s; }

	constexpr fpos &operator+=(streamoff __o) noexcept {
		__off_ += __o;
		return *this;
	}
	constexpr fpos &operator-=(streamoff __o) noexcept {
		__off_ -= __o;
		return *this;
	}
	constexpr fpos operator+(streamoff __o) const noexcept {
		fpos __t(*this);
		__t += __o;
		return __t;
	}
	constexpr fpos operator-(streamoff __o) const noexcept {
		fpos __t(*this);
		__t -= __o;
		return __t;
	}
	friend constexpr streamoff operator-(const fpos &__a, const fpos &__b) noexcept {
		return __a.__off_ - __b.__off_;
	}
	friend constexpr bool operator==(const fpos &__a, const fpos &__b) noexcept {
		return __a.__off_ == __b.__off_;
	}
	friend constexpr bool operator!=(const fpos &__a, const fpos &__b) noexcept {
		return __a.__off_ != __b.__off_;
	}
};

// Shared algorithmic base. _IntT is the int_type; _Eof the eof() value. compare()/find()
// delegate to the __constexpr_* primitives, which already implement the standard's
// unsigned/element-wise ordering; char_traits<char> only overrides the scalar lt/eq and
// the int_type conversions, which the standard specifies in terms of unsigned char.
template <typename _CharT, typename _IntT, _IntT _Eof>
struct __char_traits_base {
	using char_type = _CharT;
	using int_type = _IntT;
	using off_type = streamoff;
	using state_type = __sprt_mbstate;
	using pos_type = fpos<state_type>;
	using comparison_category = strong_ordering;

	static constexpr void assign(char_type &__c1, const char_type &__c2) noexcept { __c1 = __c2; }
	static constexpr bool eq(char_type __a, char_type __b) noexcept { return __a == __b; }
	static constexpr bool lt(char_type __a, char_type __b) noexcept { return __a < __b; }

	static constexpr int compare(const char_type *__s1, const char_type *__s2, size_t __n) {
		return __constexpr_strcompare(__s1, __s2, __n);
	}
	static constexpr size_t length(const char_type *__s) {
		size_t __i = 0;
		while (!eq(__s[__i], char_type())) {
			++__i;
		}
		return __i;
	}
	static constexpr const char_type *find(
			const char_type *__s, size_t __n, const char_type &__a) {
		return __constexpr_strfind(__s, __n, __a);
	}
	static constexpr char_type *move(char_type *__s1, const char_type *__s2, size_t __n) {
		if (__n == 0 || __s1 == __s2) {
			return __s1;
		}
		if (__s1 < __s2) {
			for (size_t __i = 0; __i < __n; ++__i) {
				__s1[__i] = __s2[__i];
			}
		} else {
			for (size_t __i = __n; __i != 0; --__i) {
				__s1[__i - 1] = __s2[__i - 1];
			}
		}
		return __s1;
	}
	static constexpr char_type *copy(char_type *__s1, const char_type *__s2, size_t __n) {
		for (size_t __i = 0; __i < __n; ++__i) {
			__s1[__i] = __s2[__i];
		}
		return __s1;
	}
	static constexpr char_type *assign(char_type *__s, size_t __n, char_type __a) {
		for (size_t __i = 0; __i < __n; ++__i) {
			__s[__i] = __a;
		}
		return __s;
	}

	static constexpr int_type to_int_type(char_type __c) noexcept { return int_type(__c); }
	static constexpr char_type to_char_type(int_type __i) noexcept { return char_type(__i); }
	static constexpr bool eq_int_type(int_type __a, int_type __b) noexcept { return __a == __b; }
	static constexpr int_type eof() noexcept { return _Eof; }
	static constexpr int_type not_eof(int_type __i) noexcept {
		return eq_int_type(__i, eof()) ? int_type(0) : __i;
	}
};

// Primary left undefined ([char.traits]/1): only the standard specializations exist.
template <typename _CharT>
struct char_traits;

template <>
struct char_traits<char> : __char_traits_base<char, int, -1> {
	// char comparisons and int conversions are performed as unsigned char. compare()
	// is inherited: it delegates to __constexpr_strcompare, which is already unsigned
	// for single-byte types.
	static constexpr bool lt(char_type __a, char_type __b) noexcept {
		return static_cast<unsigned char>(__a) < static_cast<unsigned char>(__b);
	}
	static constexpr int_type to_int_type(char_type __c) noexcept {
		return static_cast<int_type>(static_cast<unsigned char>(__c));
	}
	static constexpr char_type to_char_type(int_type __i) noexcept {
		return static_cast<char_type>(static_cast<unsigned char>(__i));
	}
};

template <>
struct char_traits<wchar_t>
: __char_traits_base<wchar_t, __sprt_wint_t, static_cast<__sprt_wint_t>(-1)> { };

// char8_t is a permanent type in this C++20 runtime (the keyword is always available),
// so this specialization is unconditional, like char16_t/char32_t.
template <>
struct char_traits<char8_t>
: __char_traits_base<char8_t, unsigned int, static_cast<unsigned int>(-1)> { };

template <>
struct char_traits<char16_t>
: __char_traits_base<char16_t, uint_least16_t, static_cast<uint_least16_t>(-1)> { };

template <>
struct char_traits<char32_t>
: __char_traits_base<char32_t, uint_least32_t, static_cast<uint_least32_t>(-1)> { };

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___STRING_CHAR_TRAITS_H_
