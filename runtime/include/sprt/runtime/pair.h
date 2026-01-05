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

#include <sprt/runtime/init.h>
#include <sprt/runtime/detail/constructable.h>

namespace sprt {

template <class _T1, class _T2>
struct __check_pair_construction {
	template <int &...>
	static constexpr bool __enable_implicit_default() {
		return __is_implicitly_default_constructible<_T1>::value
				&& __is_implicitly_default_constructible<_T2>::value;
	}

	template <int &...>
	static constexpr bool __enable_default() {
		return is_default_constructible<_T1>::value && is_default_constructible<_T2>::value;
	}

	template <class _U1, class _U2>
	static constexpr bool __is_pair_constructible() {
		return is_constructible<_T1, _U1>::value && is_constructible<_T2, _U2>::value;
	}

	template <class _U1, class _U2>
	static constexpr bool __is_implicit() {
		return is_convertible<_U1, _T1>::value && is_convertible<_U2, _T2>::value;
	}
};

template <class _T1, class _T2>
struct pair {
	using first_type = _T1;
	using second_type = _T2;

	_T1 first;
	_T2 second;

	using __trivially_relocatable =
			conditional_t<is_trivially_copyable<_T1>::value && is_trivially_copyable<_T2>::value,
					pair, void>;
	using __replaceable =
			conditional_t<__is_replaceable_v<_T1> && __is_replaceable_v<_T2>, pair, void>;

	pair(pair const &) = default;
	pair(pair &&) = default;

	template <class _CheckArgsDep = __check_pair_construction<_T1, _T2>,
			enable_if_t<_CheckArgsDep::__enable_default(), int> = 0>
	explicit(!_CheckArgsDep::__enable_implicit_default()) constexpr pair()
			noexcept(is_nothrow_default_constructible<first_type>::value
					&& is_nothrow_default_constructible<second_type>::value)
	: first(), second() { }

	template <class _CheckArgsDep = __check_pair_construction<_T1, _T2>,
			enable_if_t<
					_CheckArgsDep::template __is_pair_constructible<_T1 const &, _T2 const &>(),
					int> = 0>
	constexpr explicit(!_CheckArgsDep::template __is_implicit<_T1 const &, _T2 const &>())
			pair(_T1 const &__t1, _T2 const &__t2)
					noexcept(is_nothrow_copy_constructible<first_type>::value
							&& is_nothrow_copy_constructible<second_type>::value)
	: first(__t1), second(__t2) { }

	template < class _U1, class _U2,
			enable_if_t<__check_pair_construction<_T1, _T2>::template __is_pair_constructible<_U1,
								_U2>(),
					int> = 0 >
	constexpr explicit(!__check_pair_construction<_T1, _T2>::template __is_implicit<_U1, _U2>())
			pair(_U1 &&__u1, _U2 &&__u2) noexcept(is_nothrow_constructible<first_type, _U1>::value
					&& is_nothrow_constructible<second_type, _U2>::value)
	: first(sprt::forward<_U1>(__u1)), second(sprt::forward<_U2>(__u2)) { }

	template < class _U1, class _U2,
			enable_if_t<__check_pair_construction<_T1,
								_T2>::template __is_pair_constructible< _U1 const &, _U2 const &>(),
					int> = 0>
	constexpr explicit(!__check_pair_construction<_T1, _T2>::template __is_implicit<_U1 const &,
			_U2 const &>()) pair(pair<_U1, _U2> const &__p)
			noexcept(is_nothrow_constructible<first_type, _U1 const &>::value
					&& is_nothrow_constructible<second_type, _U2 const &>::value)
	: first(__p.first), second(__p.second) { }

	template <class _U1, class _U2,
			enable_if_t<__check_pair_construction<_T1, _T2>::template __is_pair_constructible<_U1,
								_U2>(),
					int> = 0>
	constexpr explicit(!__check_pair_construction<_T1, _T2>::template __is_implicit<_U1, _U2>())
			pair(pair<_U1, _U2> &&__p) noexcept(is_nothrow_constructible<first_type, _U1 &&>::value
					&& is_nothrow_constructible<second_type, _U2 &&>::value)
	: first(sprt::forward<_U1>(__p.first)), second(sprt::forward<_U2>(__p.second)) { }

	constexpr pair &operator=(conditional_t<is_copy_assignable<first_type>::value
					&& is_copy_assignable<second_type>::value,
			pair, __nat> const &__p) noexcept(is_nothrow_copy_assignable<first_type>::value
			&& is_nothrow_copy_assignable<second_type>::value) {
		first = __p.first;
		second = __p.second;
		return *this;
	}

	constexpr pair &operator=(conditional_t<is_move_assignable<first_type>::value
					&& is_move_assignable<second_type>::value,
			pair, __nat> &&__p) noexcept(is_nothrow_move_assignable<first_type>::value
			&& is_nothrow_move_assignable<second_type>::value) {
		first = sprt::forward<first_type>(__p.first);
		second = sprt::forward<second_type>(__p.second);
		return *this;
	}

	template < class _U1, class _U2,
			enable_if_t<is_assignable<first_type &, _U1 const &>::value
							&& is_assignable<second_type &, _U2 const &>::value,
					int> = 0>
	constexpr pair &operator=(pair<_U1, _U2> const &__p) {
		first = __p.first;
		second = __p.second;
		return *this;
	}

	template <class _U1, class _U2,
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

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_PAIR_H_
