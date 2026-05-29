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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_REFERENCE_WRAPPER
#define RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_REFERENCE_WRAPPER

#include "sprt/cxx/__utility/common.h"
#include <sprt/cxx/__type_traits/constants.h>
#include <sprt/cxx/__functional/invoke.h>

namespace sprt {

template <typename _Tp>
struct __has_result_type {
private:
	template <typename _Up>
	static false_type __test(...);
	template <typename _Up>
	static true_type __test(typename _Up::result_type * = 0);

public:
	static const bool value = decltype(__test<_Tp>(0))::value;
};


template <typename _Tp>
class reference_wrapper {
public:
	// types
	typedef _Tp type;

private:
	type *__f_;

	static void __fun(_Tp &) noexcept;
	static void __fun(_Tp &&) = delete;

public:
	template <typename _Up, typename = void_t<decltype(__fun(sprt::declval<_Up>()))>,
			enable_if_t<!is_same<remove_cvref_t<_Up>, reference_wrapper>::value, int> = 0>
	constexpr reference_wrapper(_Up &&__u) noexcept(noexcept(__fun(sprt::declval<_Up>()))) {
		type &__f = static_cast<_Up &&>(__u);
		__f_ = sprt::addressof(__f);
	}

	// access
	constexpr operator type &() const noexcept { return *__f_; }
	constexpr type &get() const noexcept { return *__f_; }

	// invoke
	template <typename... _ArgTypes>
	constexpr invoke_result_t<type &, _ArgTypes...> operator()(_ArgTypes &&...__args) const
			// Since is_nothrow_invocable requires C++17 LWG3764 is not backported
			// to earlier versions.
			noexcept(is_nothrow_invocable_v<_Tp &, _ArgTypes...>) {
		return sprt::__invoke(get(), sprt::forward<_ArgTypes>(__args)...);
	}
};

template <typename _Tp>
reference_wrapper(_Tp &) -> reference_wrapper<_Tp>;

template <typename _Tp>
inline constexpr reference_wrapper<_Tp> ref(_Tp &__t) noexcept {
	return reference_wrapper<_Tp>(__t);
}

template <typename _Tp>
inline constexpr reference_wrapper<_Tp> ref(reference_wrapper<_Tp> __t) noexcept {
	return __t;
}

template <typename _Tp>
inline constexpr reference_wrapper<const _Tp> cref(const _Tp &__t) noexcept {
	return reference_wrapper<const _Tp>(__t);
}

template <typename _Tp>
inline constexpr reference_wrapper<const _Tp> cref(reference_wrapper<_Tp> __t) noexcept {
	return __t;
}

template <typename _Tp>
void ref(const _Tp &&) = delete;

template <typename _Tp>
void cref(const _Tp &&) = delete;


} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_REFERENCE_WRAPPER
