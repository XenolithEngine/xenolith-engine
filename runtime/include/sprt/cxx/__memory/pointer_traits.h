/**
 Copyright (c) 2026 Xenolith Team Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___MEMORY_POINTER_TRAITS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___MEMORY_POINTER_TRAITS_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__type_traits/queries.h>
#include <sprt/cxx/__type_traits/types.h>

namespace sprt {

template <typename _Default, typename _Void, template <typename...> typename _Op, typename... _Args>
struct __detector {
	using type = _Default;
};

template <typename _Default, template <typename...> typename _Op, typename... _Args>
struct __detector<_Default, void_t<_Op<_Args...> >, _Op, _Args...> {
	using type = _Op<_Args...>;
};

template <typename _Default, template <typename...> typename _Op, typename... _Args>
using __detected_or_t = typename __detector<_Default, void, _Op, _Args...>::type;

template <typename _Ptr>
struct __pointer_traits_element_type_impl { };

template <template <typename, typename...> typename _Sp, typename _Tp, typename... _Args>
struct __pointer_traits_element_type_impl<_Sp<_Tp, _Args...> > {
	using type = _Tp;
};

template <typename _Ptr, typename = void>
struct __pointer_traits_element_type : __pointer_traits_element_type_impl<_Ptr> { };

template <typename _Ptr>
struct __pointer_traits_element_type<_Ptr, void_t<typename _Ptr::element_type> > {
	using type = typename _Ptr::element_type;
};

template <typename _Tp, typename _Up>
struct __pointer_traits_rebind_impl {
	static_assert(false,
			"Cannot rebind pointer; did you forget to add a rebind member to your pointer?");
};

template <template <typename, typename...> typename _Sp, typename _Tp, typename... _Args,
		typename _Up>
struct __pointer_traits_rebind_impl<_Sp<_Tp, _Args...>, _Up> {
	using type = _Sp<_Up, _Args...>;
};

template <typename _Tp, typename _Up, typename = void>
struct __pointer_traits_rebind : __pointer_traits_rebind_impl<_Tp, _Up> { };

template <typename _Tp, typename _Up>
struct __pointer_traits_rebind<_Tp, _Up, void_t<typename _Tp::template rebind<_Up> > > {
	using type = typename _Tp::template rebind<_Up>;
};

template <typename _Tp>
using __difference_type_member = typename _Tp::difference_type;

template <typename _Ptr, typename = void>
struct __pointer_traits_impl { };

template <typename _Ptr>
struct __pointer_traits_impl<_Ptr, void_t<typename __pointer_traits_element_type<_Ptr>::type> > {
	typedef _Ptr pointer;
	typedef typename __pointer_traits_element_type<pointer>::type element_type;
	using difference_type = __detected_or_t<ptrdiff_t, __difference_type_member, pointer>;

	template <typename _Up>
	using rebind = typename __pointer_traits_rebind<pointer, _Up>::type;

public:
	constexpr static pointer pointer_to(
			conditional_t<is_void<element_type>::value, __nat, element_type> &__r) {
		return pointer::pointer_to(__r);
	}
};

template <typename _Ptr>
struct pointer_traits : __pointer_traits_impl<_Ptr> { };

template <typename _Tp>
struct pointer_traits<_Tp *> {
	using pointer = _Tp *;
	using element_type = _Tp;
	using difference_type = ptrdiff_t;

	template <typename _Up>
	using rebind = _Up *;

public:
	constexpr static pointer pointer_to(
			sprt::conditional_t<sprt::is_void<element_type>::value, __nat, element_type>
					&__r) noexcept {
		return sprt::addressof(__r);
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___MEMORY_POINTER_TRAITS_H_
