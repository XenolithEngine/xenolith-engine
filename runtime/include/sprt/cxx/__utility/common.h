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

#ifndef RUNTIME_INCLUDE_CXX___UTILITY_COMMON_H_
#define RUNTIME_INCLUDE_CXX___UTILITY_COMMON_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/cxx/__type_traits/modifications.h>
#include <sprt/cxx/__type_traits/types.h>
#include <sprt/cxx/__type_traits/queries.h>
#include <sprt/cxx/__type_traits/operations.h>

namespace sprt {

template <typename Type>
Type &&__declval(int);

template <typename Type>
Type __declval(long);

template <typename Type>
decltype(sprt::__declval<Type>(0)) declval() noexcept {
	static_assert(!__is_same(Type, Type),
                "std::declval can only be used in an unevaluated context. "
                "It's likely that your current usage is trying to extract a value from the function.");
}

/*
	forward
*/

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &__t) noexcept {
	return static_cast<Type &&>(__t);
}

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &&__t) noexcept {
	static_assert(!is_lvalue_reference<Type>::value, "cannot forward an rvalue as an lvalue");
	return static_cast<Type &&>(__t);
}

/*
	move
*/

// Use sprt::move instead of std::move to enable additional diagnostics
template <typename Type,
		enable_if_t<
				// Attempt to move pointer is most likely an reference count error
				// Use move_unsafe if it's not an error (like, in template context)
				!is_pointer_v<remove_reference_t<Type>>, bool> = true>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}


template <typename _Tp>
using __move_if_noexcept_result_t = conditional_t<!is_nothrow_move_constructible<_Tp>::value
				&& is_copy_constructible<_Tp>::value,
		const _Tp &, _Tp &&>;

template <typename _Tp>
inline __move_if_noexcept_result_t<_Tp> move_if_noexcept(_Tp &__x) noexcept {
	return sprt::move(__x);
}


// Behaves like std::move
template <typename Type>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move_unsafe(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}

template <typename _T1, typename _T2 = _T1>
inline constexpr _T1 exchange(_T1 &__obj, _T2 &&__new_value) noexcept(
		is_nothrow_move_constructible<_T1>::value && is_nothrow_assignable<_T1 &, _T2>::value) {
	_T1 __old_value = sprt::move(__obj);
	__obj = sprt::forward<_T2>(__new_value);
	return __old_value;
}

template <typename Type>
constexpr inline Type *addressof(Type &__x) noexcept {
	return __builtin_addressof(__x);
}

template <typename _Tp>
[[nodiscard]]
constexpr const _Tp &as_const(_Tp &__t) noexcept {
	return __t;
}

template <typename _Tp>
void as_const(const _Tp &&) = delete;

template <typename _Tp>
[[nodiscard]]
constexpr underlying_type_t<_Tp> to_underlying(_Tp __val) noexcept {
	return static_cast<underlying_type_t<_Tp>>(__val);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_CXX___UTILITY_COMMON_H_
