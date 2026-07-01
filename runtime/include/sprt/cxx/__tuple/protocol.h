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

// The tuple protocol primaries (std::tuple_size / std::tuple_element). They MUST live
// in namespace std (they are the language-recognized customization points used by
// structured bindings and by user specializations), even though the tuple/pair/array
// types they describe live in namespace sprt. Shared by <sprt/cxx/tuple>,
// <sprt/cxx/__utility/pair.h> and <sprt/cxx/array> (and re-exported by the stl headers)
// so there is exactly one set of primaries.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TUPLE_PROTOCOL_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TUPLE_PROTOCOL_H_

#include <sprt/cxx/type_traits>
#include <sprt/cxx/cstddef>

namespace std {

template <typename _Tp>
struct tuple_size;

template <typename _Tp>
inline constexpr sprt::size_t tuple_size_v = tuple_size<_Tp>::value;

template <sprt::size_t _Ip, typename _Tp>
struct tuple_element;

template <sprt::size_t _Ip, typename _Tp>
using tuple_element_t = typename tuple_element<_Ip, _Tp>::type;

// cv-qualified tuple-like types forward to the unqualified specialization. The
// __enable_if_has_tuple_size guard (LWG 2313) makes these exist only when
// tuple_size<_Tp>::value is well-formed, so tuple_size<const NonTuple> is an
// incomplete type (SFINAE-friendly) rather than a hard error.
template <typename _Tp, sprt::size_t = tuple_size<_Tp>::value>
using __enable_if_has_tuple_size = _Tp;

template <typename _Tp>
struct tuple_size<const __enable_if_has_tuple_size<_Tp>>
: sprt::integral_constant<sprt::size_t, tuple_size<_Tp>::value> { };
template <typename _Tp>
struct tuple_size<volatile __enable_if_has_tuple_size<_Tp>>
: sprt::integral_constant<sprt::size_t, tuple_size<_Tp>::value> { };
template <typename _Tp>
struct tuple_size<const volatile __enable_if_has_tuple_size<_Tp>>
: sprt::integral_constant<sprt::size_t, tuple_size<_Tp>::value> { };

template <sprt::size_t _Ip, typename _Tp>
struct tuple_element<_Ip, const _Tp> {
	using type = const tuple_element_t<_Ip, _Tp>;
};
template <sprt::size_t _Ip, typename _Tp>
struct tuple_element<_Ip, volatile _Tp> {
	using type = volatile tuple_element_t<_Ip, _Tp>;
};
template <sprt::size_t _Ip, typename _Tp>
struct tuple_element<_Ip, const volatile _Tp> {
	using type = const volatile tuple_element_t<_Ip, _Tp>;
};

} // namespace std

#endif // RUNTIME_INCLUDE_SPRT_CXX___TUPLE_PROTOCOL_H_
