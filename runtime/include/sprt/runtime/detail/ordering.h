/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ORDEING_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ORDEING_H_

/* Original source from LLVM libc++ */

#include <sprt/runtime/int.h>

#if __SPRT_USE_STL

#include <compare>

#else

namespace std {

// exposition only
enum class _OrdResult : signed char {
	__less = -1,
	__equiv = 0,
	__greater = 1
};

enum class _PartialOrdResult : signed char {
	__less = static_cast<signed char>(_OrdResult::__less),
	__equiv = static_cast<signed char>(_OrdResult::__equiv),
	__greater = static_cast<signed char>(_OrdResult::__greater),
	__unordered = -127,
};

class partial_ordering;
class weak_ordering;
class strong_ordering;

struct _CmpUnspecifiedParam {
	// If anything other than a literal 0 is provided, the behavior is undefined by the Standard.
	//
	// The alternative to the `__enable_if__` attribute would be to use the fact that a pointer
	// can be constructed from literal 0, but this conflicts with `-Wzero-as-null-pointer-constant`.
	template <class _Tp, class = sprt::enable_if_t<sprt::is_same_v<_Tp, int> > >
	consteval _CmpUnspecifiedParam(_Tp __zero) noexcept {
		(void)__zero;
	}
};

class partial_ordering {
	explicit constexpr partial_ordering(_PartialOrdResult __v) noexcept : __value_(__v) { }

public:
	// valid values
	static const partial_ordering less;
	static const partial_ordering equivalent;
	static const partial_ordering greater;
	static const partial_ordering unordered;

	// comparisons
	friend constexpr bool operator==(partial_ordering, partial_ordering) noexcept = default;

	friend constexpr bool operator==(partial_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == _PartialOrdResult::__equiv;
	}

	friend constexpr bool operator<(partial_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == _PartialOrdResult::__less;
	}

	friend constexpr bool operator<=(partial_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == _PartialOrdResult::__equiv
				|| __v.__value_ == _PartialOrdResult::__less;
	}

	friend constexpr bool operator>(partial_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == _PartialOrdResult::__greater;
	}

	friend constexpr bool operator>=(partial_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == _PartialOrdResult::__equiv
				|| __v.__value_ == _PartialOrdResult::__greater;
	}

	friend constexpr bool operator<(_CmpUnspecifiedParam, partial_ordering __v) noexcept {
		return __v.__value_ == _PartialOrdResult::__greater;
	}

	friend constexpr bool operator<=(_CmpUnspecifiedParam, partial_ordering __v) noexcept {
		return __v.__value_ == _PartialOrdResult::__equiv
				|| __v.__value_ == _PartialOrdResult::__greater;
	}

	friend constexpr bool operator>(_CmpUnspecifiedParam, partial_ordering __v) noexcept {
		return __v.__value_ == _PartialOrdResult::__less;
	}

	friend constexpr bool operator>=(_CmpUnspecifiedParam, partial_ordering __v) noexcept {
		return __v.__value_ == _PartialOrdResult::__equiv
				|| __v.__value_ == _PartialOrdResult::__less;
	}

	friend constexpr partial_ordering operator<=>(partial_ordering __v,
			_CmpUnspecifiedParam) noexcept {
		return __v;
	}

	friend constexpr partial_ordering operator<=>(_CmpUnspecifiedParam,
			partial_ordering __v) noexcept {
		return __v < 0 ? partial_ordering::greater : (__v > 0 ? partial_ordering::less : __v);
	}

private:
	_PartialOrdResult __value_;
};

inline constexpr partial_ordering partial_ordering::less(_PartialOrdResult::__less);
inline constexpr partial_ordering partial_ordering::equivalent(_PartialOrdResult::__equiv);
inline constexpr partial_ordering partial_ordering::greater(_PartialOrdResult::__greater);
inline constexpr partial_ordering partial_ordering::unordered(_PartialOrdResult::__unordered);

class weak_ordering {
	using _ValueT = signed char;

	explicit constexpr weak_ordering(_OrdResult __v) noexcept : __value_(_ValueT(__v)) { }

public:
	static const weak_ordering less;
	static const weak_ordering equivalent;
	static const weak_ordering greater;

	constexpr operator partial_ordering() const noexcept {
		return __value_ == 0 ? partial_ordering::equivalent
							 : (__value_ < 0 ? partial_ordering::less : partial_ordering::greater);
	}

	// comparisons
	friend constexpr bool operator==(weak_ordering, weak_ordering) noexcept = default;

	friend constexpr bool operator==(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == 0;
	}

	friend constexpr bool operator<(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ < 0;
	}

	friend constexpr bool operator<=(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ <= 0;
	}

	friend constexpr bool operator>(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ > 0;
	}

	friend constexpr bool operator>=(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ >= 0;
	}

	friend constexpr bool operator<(_CmpUnspecifiedParam, weak_ordering __v) noexcept {
		return 0 < __v.__value_;
	}

	friend constexpr bool operator<=(_CmpUnspecifiedParam, weak_ordering __v) noexcept {
		return 0 <= __v.__value_;
	}

	friend constexpr bool operator>(_CmpUnspecifiedParam, weak_ordering __v) noexcept {
		return 0 > __v.__value_;
	}

	friend constexpr bool operator>=(_CmpUnspecifiedParam, weak_ordering __v) noexcept {
		return 0 >= __v.__value_;
	}

	friend constexpr weak_ordering operator<=>(weak_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v;
	}

	friend constexpr weak_ordering operator<=>(_CmpUnspecifiedParam, weak_ordering __v) noexcept {
		return __v < 0 ? weak_ordering::greater : (__v > 0 ? weak_ordering::less : __v);
	}

private:
	_ValueT __value_;
};

inline constexpr weak_ordering weak_ordering::less(_OrdResult::__less);
inline constexpr weak_ordering weak_ordering::equivalent(_OrdResult::__equiv);
inline constexpr weak_ordering weak_ordering::greater(_OrdResult::__greater);

class strong_ordering {
	using _ValueT = signed char;

	explicit constexpr strong_ordering(_OrdResult __v) noexcept : __value_(_ValueT(__v)) { }

public:
	static const strong_ordering less;
	static const strong_ordering equal;
	static const strong_ordering equivalent;
	static const strong_ordering greater;

	// conversions
	constexpr operator partial_ordering() const noexcept {
		return __value_ == 0 ? partial_ordering::equivalent
							 : (__value_ < 0 ? partial_ordering::less : partial_ordering::greater);
	}

	constexpr operator weak_ordering() const noexcept {
		return __value_ == 0 ? weak_ordering::equivalent
							 : (__value_ < 0 ? weak_ordering::less : weak_ordering::greater);
	}

	// comparisons
	friend constexpr bool operator==(strong_ordering, strong_ordering) noexcept = default;

	friend constexpr bool operator==(strong_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ == 0;
	}

	friend constexpr bool operator<(strong_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ < 0;
	}

	friend constexpr bool operator<=(strong_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ <= 0;
	}

	friend constexpr bool operator>(strong_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ > 0;
	}

	friend constexpr bool operator>=(strong_ordering __v, _CmpUnspecifiedParam) noexcept {
		return __v.__value_ >= 0;
	}

	friend constexpr bool operator<(_CmpUnspecifiedParam, strong_ordering __v) noexcept {
		return 0 < __v.__value_;
	}

	friend constexpr bool operator<=(_CmpUnspecifiedParam, strong_ordering __v) noexcept {
		return 0 <= __v.__value_;
	}

	friend constexpr bool operator>(_CmpUnspecifiedParam, strong_ordering __v) noexcept {
		return 0 > __v.__value_;
	}

	friend constexpr bool operator>=(_CmpUnspecifiedParam, strong_ordering __v) noexcept {
		return 0 >= __v.__value_;
	}

	friend constexpr strong_ordering operator<=>(strong_ordering __v,
			_CmpUnspecifiedParam) noexcept {
		return __v;
	}

	friend constexpr strong_ordering operator<=>(_CmpUnspecifiedParam,
			strong_ordering __v) noexcept {
		return __v < 0 ? strong_ordering::greater : (__v > 0 ? strong_ordering::less : __v);
	}

private:
	_ValueT __value_;
};

inline constexpr strong_ordering strong_ordering::less(_OrdResult::__less);
inline constexpr strong_ordering strong_ordering::equal(_OrdResult::__equiv);
inline constexpr strong_ordering strong_ordering::equivalent(_OrdResult::__equiv);
inline constexpr strong_ordering strong_ordering::greater(_OrdResult::__greater);

/// [cmp.categories.pre]/1
/// The types partial_ordering, weak_ordering, and strong_ordering are
/// collectively termed the comparison category types.
template <class _Tp>
concept __comparison_category = sprt::is_same_v<_Tp, partial_ordering>
		|| sprt::is_same_v<_Tp, weak_ordering> || sprt::is_same_v<_Tp, strong_ordering>;

} // namespace std

#endif

namespace sprt {

using std::partial_ordering;
using std::weak_ordering;
using std::strong_ordering;

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_ORDEING_H_
