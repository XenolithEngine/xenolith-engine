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

// C++20 iterator-associated types + iterator concepts ([iterator.concepts]) and a
// minimal <ranges> foundation ([range.access] CPOs + [range.refinements] concepts).
// This is the pragmatic subset the libc++ test-support headers (test_iterators.h)
// need in order to compile against the sprt STL: correct enough for the positive
// iterator-concept assertions, with the range concepts present-but-approximate (the
// suite has no namespace-scope static_asserts over range concepts). It is NOT a full
// std::ranges implementation.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_CONCEPTS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_CONCEPTS_H_

#include <sprt/cxx/concepts>
#include <sprt/cxx/__iterator/iterator_ops.h>
#include <sprt/cxx/__iterator/iterator_tags.h>
#include <sprt/cxx/__type_traits/modifications.h> // remove_cvref_t / remove_reference_t / make_signed_t
#include <sprt/cxx/__type_traits/queries.h>

namespace sprt {

// ---- referenceable helper ---------------------------------------------------
template <typename _Tp>
using __with_reference = _Tp &;
template <typename _Tp>
concept __can_reference = requires { typename __with_reference<_Tp>; };
template <typename _Tp>
concept __dereferenceable = requires(_Tp &__t) {
	{ *__t } -> __can_reference;
};

// ---- associated types -------------------------------------------------------
// iter_value_t / iter_difference_t resolve from a member typedef (the C++17/20
// test iterators and every sprt container iterator expose value_type /
// difference_type directly) or, for raw pointers, from the pointee. They must NOT
// fall through to iterator_traits<_Ip> for an arbitrary _Ip: sprt's iterator_traits
// primary is undefined, so that would hard-error instead of being SFINAE-friendly
// (iter_value_t<int> must simply be a substitution failure).
template <typename _Ip>
struct __iter_value { };
template <typename _Ip>
requires requires { typename _Ip::value_type; }
struct __iter_value<_Ip> {
	using type = typename _Ip::value_type;
};
template <typename _Tp>
struct __iter_value<_Tp *> {
	using type = remove_cv_t<_Tp>;
};
template <typename _Ip>
using iter_value_t = typename __iter_value<remove_cvref_t<_Ip>>::type;

template <typename _Ip>
struct __iter_diff { };
template <typename _Ip>
requires requires { typename _Ip::difference_type; }
struct __iter_diff<_Ip> {
	using type = typename _Ip::difference_type;
};
template <typename _Tp>
struct __iter_diff<_Tp *> {
	using type = sprt::ptrdiff_t;
};
template <typename _Ip>
using iter_difference_t = typename __iter_diff<remove_cvref_t<_Ip>>::type;

template <typename _Ip>
requires __dereferenceable<_Ip &>
using iter_reference_t = decltype(*sprt::declval<_Ip &>());

// ---- ranges::iter_move / iter_swap (minimal CPOs) ---------------------------
namespace ranges {

struct __iter_move_fn {
	template <typename _Ip>
	constexpr decltype(auto) operator()(_Ip &&__i) const {
		using _Ref = decltype(*sprt::forward<_Ip>(__i));
		if constexpr (sprt::is_lvalue_reference_v<_Ref>) {
			return static_cast<sprt::remove_reference_t<_Ref> &&>(*sprt::forward<_Ip>(__i));
		} else {
			return *sprt::forward<_Ip>(__i);
		}
	}
};
inline constexpr __iter_move_fn iter_move{};

struct __iter_swap_fn {
	template <typename _I1, typename _I2>
	constexpr void operator()(_I1 &&__a, _I2 &&__b) const {
		sprt::swap(*__a, *__b);
	}
};
inline constexpr __iter_swap_fn iter_swap{};

} // namespace ranges

template <typename _Ip>
requires __dereferenceable<_Ip &>
using iter_rvalue_reference_t = decltype(sprt::ranges::iter_move(sprt::declval<_Ip &>()));

// ---- ITER_CONCEPT ([iterator.concepts.general]) -----------------------------
template <typename _Ip>
struct __iter_concept_impl {
	using type = typename iterator_traits<_Ip>::iterator_category;
};
template <typename _Ip>
requires requires { typename _Ip::iterator_concept; }
struct __iter_concept_impl<_Ip> {
	using type = typename _Ip::iterator_concept;
};
template <typename _Tp>
struct __iter_concept_impl<_Tp *> {
	using type = contiguous_iterator_tag; // raw pointers are contiguous ([iterator.traits])
};
template <typename _Ip>
using __iter_concept_t = typename __iter_concept_impl<remove_cvref_t<_Ip>>::type;

// ---- readable / writable ----------------------------------------------------
template <typename _In>
concept indirectly_readable = requires(const remove_cvref_t<_In> __i) {
	typename iter_value_t<_In>;
	typename iter_reference_t<remove_cvref_t<_In>>;
	{ *__i } -> __can_reference;
};

template <typename _Out, typename _Tp>
concept indirectly_writable = requires(_Out &&__o, _Tp &&__t) {
	*__o = sprt::forward<_Tp>(__t);
	*sprt::forward<_Out>(__o) = sprt::forward<_Tp>(__t);
};

// ---- incrementable hierarchy ------------------------------------------------
template <typename _Ip>
concept weakly_incrementable = movable<_Ip> && requires(_Ip __i) {
	typename iter_difference_t<_Ip>;
	{ ++__i } -> same_as<_Ip &>;
	__i++;
};

template <typename _Ip>
concept incrementable = regular<_Ip> && weakly_incrementable<_Ip> && requires(_Ip __i) {
	{ __i++ } -> same_as<_Ip>;
};

template <typename _Ip>
concept input_or_output_iterator = __dereferenceable<_Ip> && weakly_incrementable<_Ip>;

template <typename _Sp, typename _Ip>
concept sentinel_for = semiregular<_Sp> && input_or_output_iterator<_Ip>
		&& __weakly_equality_comparable_with<_Sp, _Ip>;

template <typename _Sp, typename _Ip>
concept sized_sentinel_for = sentinel_for<_Sp, _Ip> && requires(const _Ip &__i, const _Sp &__s) {
	{ __s - __i } -> same_as<iter_difference_t<_Ip>>;
	{ __i - __s } -> same_as<iter_difference_t<_Ip>>;
};

// ---- iterator concepts ------------------------------------------------------
template <typename _Ip>
concept input_iterator = input_or_output_iterator<_Ip> && indirectly_readable<_Ip> && requires {
	typename __iter_concept_t<_Ip>;
} && derived_from<__iter_concept_t<_Ip>, input_iterator_tag>;

template <typename _Ip, typename _Tp>
concept output_iterator = input_or_output_iterator<_Ip> && indirectly_writable<_Ip, _Tp>
		&& requires(_Ip __i, _Tp &&__t) { *__i++ = sprt::forward<_Tp>(__t); };

template <typename _Ip>
concept forward_iterator =
		input_iterator<_Ip> && derived_from<__iter_concept_t<_Ip>, forward_iterator_tag>
		&& incrementable<_Ip> && sentinel_for<_Ip, _Ip>;

template <typename _Ip>
concept bidirectional_iterator = forward_iterator<_Ip>
		&& derived_from<__iter_concept_t<_Ip>, bidirectional_iterator_tag> && requires(_Ip __i) {
			   { --__i } -> same_as<_Ip &>;
			   { __i-- } -> same_as<_Ip>;
		   };

template <typename _Ip>
concept random_access_iterator = bidirectional_iterator<_Ip>
		&& derived_from<__iter_concept_t<_Ip>, random_access_iterator_tag> && totally_ordered<_Ip>
		&& sized_sentinel_for<_Ip, _Ip>
		&& requires(_Ip __i, const _Ip __j, const iter_difference_t<_Ip> __n) {
			   { __i += __n } -> same_as<_Ip &>;
			   { __j + __n } -> same_as<_Ip>;
			   { __n + __j } -> same_as<_Ip>;
			   { __i -= __n } -> same_as<_Ip &>;
			   { __j - __n } -> same_as<_Ip>;
			   { __j[__n] } -> __can_reference;
		   };

template <typename _Ip>
concept contiguous_iterator =
		random_access_iterator<_Ip> && derived_from<__iter_concept_t<_Ip>, contiguous_iterator_tag>
		&& is_lvalue_reference_v<iter_reference_t<_Ip>>;

// ---- minimal <ranges> foundation -------------------------------------------
namespace ranges {

struct __begin_fn {
	template <typename _Tp, size_t _Np>
	constexpr _Tp *operator()(_Tp (&__a)[_Np]) const noexcept {
		return __a;
	}
	template <typename _Tp>
	constexpr auto operator()(_Tp &&__t) const -> decltype(__t.begin()) {
		return __t.begin();
	}
};
inline constexpr __begin_fn begin{};

struct __end_fn {
	template <typename _Tp, size_t _Np>
	constexpr _Tp *operator()(_Tp (&__a)[_Np]) const noexcept {
		return __a + _Np;
	}
	template <typename _Tp>
	constexpr auto operator()(_Tp &&__t) const -> decltype(__t.end()) {
		return __t.end();
	}
};
inline constexpr __end_fn end{};

template <typename _Tp>
using iterator_t = decltype(sprt::ranges::begin(sprt::declval<_Tp &>()));
template <typename _Tp>
using sentinel_t = decltype(sprt::ranges::end(sprt::declval<_Tp &>()));

template <typename _Tp>
concept range = requires(_Tp &__t) {
	sprt::ranges::begin(__t);
	sprt::ranges::end(__t);
};

template <typename _Tp>
using range_value_t = iter_value_t<iterator_t<_Tp>>;
template <typename _Tp>
using range_reference_t = iter_reference_t<iterator_t<_Tp>>;
template <typename _Tp>
using range_difference_t = iter_difference_t<iterator_t<_Tp>>;

template <typename _Tp>
concept input_range = range<_Tp> && input_iterator<iterator_t<_Tp>>;
template <typename _Tp>
concept forward_range = input_range<_Tp> && forward_iterator<iterator_t<_Tp>>;
template <typename _Tp>
concept bidirectional_range = forward_range<_Tp> && bidirectional_iterator<iterator_t<_Tp>>;
template <typename _Tp>
concept random_access_range = bidirectional_range<_Tp> && random_access_iterator<iterator_t<_Tp>>;
template <typename _Tp>
concept common_range = range<_Tp> && same_as<iterator_t<_Tp>, sentinel_t<_Tp>>;

struct view_base { };

// enable_view ([range.view]): true for types deriving from view_base (the
// view_interface branch is dropped with the rest of the adaptor machinery).
// Without this opt-in gate every movable container would satisfy `view`.
template <typename _Tp>
inline constexpr bool enable_view = derived_from<_Tp, view_base>;

template <typename _Tp>
concept view = range<_Tp> && movable<_Tp> && enable_view<_Tp>;
template <typename _Tp>
concept viewable_range = range<remove_cvref_t<_Tp>>;
template <typename _Tp>
concept borrowed_range = range<_Tp>;

// dangling ([range.dangling])
struct dangling {
	constexpr dangling() noexcept = default;
	template <typename... _Args>
	constexpr dangling(_Args &&...) noexcept { }
};

} // namespace ranges

namespace views {
// Minimal all_t: enough to name in deduction guides (never instantiated by the
// classic-algorithm tests). A real views::all would return ref_view/owning_view.
template <typename _Rng>
using all_t = sprt::remove_cvref_t<_Rng>;
} // namespace views

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_CONCEPTS_H_
