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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_MODIFICATIONS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_MODIFICATIONS_H_

#include <sprt/cxx/__type_traits/constants.h>

namespace sprt {

template <typename Type>
struct remove_const {
	using type = __remove_const(Type);
};

template <typename Type>
using remove_const_t = __remove_const(Type);

template <typename Type>
struct remove_volatile {
	using type = __remove_volatile(Type);
};

template <typename Type>
using remove_volatile_t = __remove_volatile(Type);

template <typename Type>
struct remove_cv {
	using type = __remove_cv(Type);
};

template <typename Type>
using remove_cv_t = __remove_cv(Type);


template <typename Type>
struct add_const {
	using type = const Type;
};

template <typename Type>
using add_const_t = typename add_const<Type>::type;

template <typename Type>
struct add_cv {
	using type = const volatile Type;
};

template <typename Type>
using add_cv_t = typename add_cv<Type>::type;

template <typename Type>
struct add_volatile {
	using type = volatile Type;
};

template <typename Type>
using add_volatile_t = typename add_volatile<Type>::type;


template <typename Type>
struct make_signed {
	using type = __make_signed(Type);
};

template <typename Type>
using make_signed_t = __make_signed(Type);


template <typename Type>
struct make_unsigned {
	using type = __make_unsigned(Type);
};

template <typename Type>
using make_unsigned_t = __make_unsigned(Type);


template <typename Type>
struct remove_reference {
	using type = __remove_reference_t(Type);
};

template <typename Type>
using remove_reference_t = __remove_reference_t(Type);

template <typename Type>
struct add_lvalue_reference {
	using type = __add_lvalue_reference(Type);
};

template <typename Type>
using add_lvalue_reference_t = __add_lvalue_reference(Type);


template <typename Type>
struct add_rvalue_reference {
	using type = __add_rvalue_reference(Type);
};

template <typename Type>
using add_rvalue_reference_t = __add_rvalue_reference(Type);


template <typename Type>
struct remove_pointer {
	using type = __remove_pointer(Type);
};

template <typename Type>
using remove_pointer_t = typename remove_pointer<Type>::type;

template <typename Type>
struct add_pointer {
	using type = __add_pointer(Type);
};

template <typename Type>
using add_pointer_t = __add_pointer(Type);


template <typename Type>
struct remove_extent {
	using type = __remove_extent(Type);
};

template <typename Type>
using remove_extent_t = __remove_extent(Type);

template <typename Type>
struct remove_all_extents {
	using type = __remove_all_extents(Type);
};

template <typename Type>
using remove_all_extents_t = __remove_all_extents(Type);

template <typename Type>
struct remove_cvref {
	using type = __remove_cvref(Type);
};

template <typename Type>
using remove_cvref_t = __remove_cvref(Type);

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_MODIFICATIONS_H_
