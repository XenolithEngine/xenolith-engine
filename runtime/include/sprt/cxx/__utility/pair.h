/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_PAIR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_PAIR_H_

#include <sprt/c/bits/__sprt_def.h>

#if __SPRT_USE_STL
// libc++ present: project std::pair into namespace sprt (sprt::pair IS std::pair), the
// same single value-type switch as <sprt/cxx/tuple>. A hand-written sprt::pair would carry
// its own namespace-std tuple-protocol / make_pair / get that clash (ambiguous ADL) with
// libc++'s; projecting dissolves that and lets pair's piecewise ctor use std::tuple.
#include <utility>

namespace sprt {
using std::get;
using std::make_pair;
using std::pair;
using std::piecewise_construct;
using std::piecewise_construct_t;
} // namespace sprt

#else // freestanding / sprt's own build: hand-written pair

#include <sprt/cxx/__utility/swap.h>
#include <sprt/cxx/__type_traits/types.h>
#include <sprt/cxx/__type_traits/operations.h>
#include <sprt/cxx/__type_traits/queries.h>
#include <sprt/cxx/__type_traits/unwrap_ref.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__utility/integer_sequence.h>
#include <sprt/cxx/compare>

namespace sprt {

// Forward declaration only. pair's piecewise_construct constructors (below) are DEFINED
// in <sprt/cxx/tuple>, where the full tuple type and sprt::get are available — exactly
// as the standard defines pair's piecewise ctor in <tuple>.
template <typename... _UTp>
class tuple;

// [pairs.pair] piecewise construction tag (replaces the old sprt-only
// pair_emplace_construct_t extension).
struct piecewise_construct_t {
	explicit piecewise_construct_t() = default;
};
inline constexpr piecewise_construct_t piecewise_construct {};

template <typename Type, typename = void>
struct __is_replaceable : is_trivially_copyable<Type> { };

template <typename Type>
struct __is_replaceable<Type, enable_if_t<is_same<Type, typename Type::__replaceable>::value> >
: true_type { };

template <typename Type>
inline const bool __is_replaceable_v = __is_replaceable<Type>::value;

template <typename _T1, typename _T2>
struct check_pair_construction {
	template <int &...>
	static constexpr bool enable_implicit_default() {
		return __is_implicitly_default_constructible<_T1>::value
				&& __is_implicitly_default_constructible<_T2>::value;
	}

	template <int &...>
	static constexpr bool enable_default() {
		return is_default_constructible<_T1>::value && is_default_constructible<_T2>::value;
	}

	template <typename _U1, typename _U2>
	static constexpr bool is_pair_constructible() {
		return is_constructible<_T1, _U1>::value && is_constructible<_T2, _U2>::value;
	}

	template <typename _U1, typename _U2>
	static constexpr bool is_implicit() {
		return is_convertible_v<_U1, _T1> && is_convertible_v<_U2, _T2>;
	}
};

template <typename _T1, typename _T2>
struct pair {
	using first_type = _T1;
	using second_type = _T2;

	_T1 first;
	_T2 second;

	using trivially_relocatable =
			conditional_t<is_trivially_copyable<_T1>::value && is_trivially_copyable<_T2>::value,
					pair, void>;
	using replaceable =
			conditional_t<__is_replaceable_v<_T1> && __is_replaceable_v<_T2>, pair, void>;

	pair(pair const &) = default;
	pair(pair &&) = default;

	pair &operator=(pair &&) = default;
	pair &operator=(const pair &) = default;

	// [pairs.pair] default constructor: participates only when both elements are
	// default-constructible; explicit unless both are implicitly default-constructible.
	template <typename _U1 = _T1, typename _U2 = _T2,
			enable_if_t<check_pair_construction<_U1, _U2>::enable_default(), int> = 0>
	constexpr explicit(!check_pair_construction<_T1, _T2>::enable_implicit_default())
			pair() noexcept : first(), second() { }

	// [pairs.pair] piecewise construction. Declared here, DEFINED in <sprt/cxx/tuple>.
	template <typename... _Args1, typename... _Args2>
	constexpr pair(piecewise_construct_t, tuple<_Args1...> __first_args,
			tuple<_Args2...> __second_args);

private:
	template <typename... _Args1, sprt::size_t... _I1, typename... _Args2, sprt::size_t... _I2>
	constexpr pair(piecewise_construct_t, tuple<_Args1...> &__t1, tuple<_Args2...> &__t2,
			index_sequence<_I1...>, index_sequence<_I2...>);

public:
	template <typename _CheckArgsDep = check_pair_construction<_T1, _T2>,
			enable_if_t< _CheckArgsDep::template is_pair_constructible<_T1 const &, _T2 const &>(),
					int> = 0>
	constexpr explicit(!_CheckArgsDep::template is_implicit<_T1 const &, _T2 const &>())
			pair(_T1 const &__t1, _T2 const &__t2) noexcept
	: first(__t1), second(__t2) { }

	template < typename _U1, typename _U2,
			enable_if_t<
					check_pair_construction<_T1, _T2>::template is_pair_constructible<_U1, _U2>(),
					int> = 0 >
	constexpr explicit(!check_pair_construction<_T1, _T2>::template is_implicit<_U1, _U2>())
			pair(_U1 &&__u1, _U2 &&__u2) noexcept
	: first(sprt::forward<_U1>(__u1)), second(sprt::forward<_U2>(__u2)) { }

	template < typename _U1, typename _U2,
			enable_if_t<check_pair_construction<_T1,
								_T2>::template is_pair_constructible< _U1 const &, _U2 const &>(),
					int> = 0>
	constexpr explicit(
			!check_pair_construction<_T1, _T2>::template is_implicit<_U1 const &, _U2 const &>())
			pair(pair<_U1, _U2> const &__p) noexcept
	: first(__p.first), second(__p.second) { }

	template <typename _U1, typename _U2,
			enable_if_t<
					check_pair_construction<_T1, _T2>::template is_pair_constructible<_U1, _U2>(),
					int> = 0>
	constexpr explicit(!check_pair_construction<_T1, _T2>::template is_implicit<_U1, _U2>())
			pair(pair<_U1, _U2> &&__p) noexcept
	: first(sprt::forward<_U1>(__p.first)), second(sprt::forward<_U2>(__p.second)) { }

	template < typename _U1, typename _U2,
			enable_if_t<is_assignable<first_type &, _U1 const &>::value
							&& is_assignable<second_type &, _U2 const &>::value,
					int> = 0>
	constexpr pair &operator=(pair<_U1, _U2> const &__p) {
		first = __p.first;
		second = __p.second;
		return *this;
	}

	template <typename _U1, typename _U2,
			enable_if_t<is_assignable<first_type &, _U1>::value
							&& is_assignable<second_type &, _U2>::value,
					int> = 0>
	constexpr pair &operator=(pair<_U1, _U2> &&__p) {
		first = sprt::forward<_U1>(__p.first);
		second = sprt::forward<_U2>(__p.second);
		return *this;
	}

	constexpr void swap(pair &__p) noexcept {
		using sprt::swap;
		swap(first, __p.first);
		swap(second, __p.second);
	}
};

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator==(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return __x.first == __y.first && __x.second == __y.second;
}

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator!=(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return !(__x == __y);
}

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator<(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return __x.first < __y.first || (!(__y.first < __x.first) && __x.second < __y.second);
}

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator>(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return __y < __x;
}

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator>=(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return !(__x < __y);
}

template <typename _T1, typename _T2, typename _U1, typename _U2>
inline constexpr bool operator<=(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) noexcept {
	return !(__y < __x);
}

// C++20 [pairs.spec]: three-way comparison. The legacy relational operators above
// stay (a non-rewritten operator< is always preferred over the rewritten <=>
// candidate, so there is no ambiguity); this adds the missing `pair <=> pair`.
template <typename _T1, typename _T2, typename _U1, typename _U2>
constexpr std::common_comparison_category_t<__synth_three_way_result_t<_T1, _U1>,
		__synth_three_way_result_t<_T2, _U2>>
operator<=>(const pair<_T1, _T2> &__x, const pair<_U1, _U2> &__y) {
	if (auto __c = sprt::__synth_three_way(__x.first, __y.first); __c != 0) {
		return __c;
	}
	return sprt::__synth_three_way(__x.second, __y.second);
}

// [pairs.spec] non-member swap.
template <typename _T1, typename _T2>
constexpr void swap(pair<_T1, _T2> &__x, pair<_T1, _T2> &__y) noexcept(noexcept(__x.swap(__y))) {
	__x.swap(__y);
}

template <typename _T1, typename _T2>
inline constexpr pair<unwrap_ref_decay_t<_T1>, unwrap_ref_decay_t<_T2> > make_pair(_T1 &&__t1,
		_T2 &&__t2) {
	return pair<unwrap_ref_decay_t<_T1>, unwrap_ref_decay_t<_T2> >(sprt::forward<_T1>(__t1),
			sprt::forward<_T2>(__t2));
}

template <class T1, class T2>
pair(T1, T2) -> pair<T1, T2>;

} // namespace sprt

#endif // __SPRT_STL_LIBCXX_VALUETYPES: libc++ projection vs hand-written pair

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_PAIR_H_
