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

// Minimal common_reference / basic_common_reference ([meta.trans.other]). The full
// four-bullet algorithm (ternary common reference, the basic_common_reference hook,
// COMMON-REF) is NOT implemented; this approximates the two-type case with
// common_type. It exists so the C++20 iterator/range machinery and the libc++
// test-support headers (which name std::common_reference_t / specialize
// std::basic_common_reference) compile against the sprt STL.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_COMMON_REFERENCE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_COMMON_REFERENCE_H_

#include <sprt/cxx/__type_traits/queries.h> // common_type / common_type_t

namespace sprt {
inline namespace __cxx_type_traits {

// User customization point; the primary template is intentionally empty.
template <typename _Tp, typename _Up, template <typename> class _TQual,
		template <typename> class _UQual>
struct basic_common_reference { };

template <typename...>
struct common_reference;

template <typename... _Tp>
using common_reference_t = typename common_reference<_Tp...>::type;

template <>
struct common_reference<> { };

template <typename _Tp>
struct common_reference<_Tp> {
	using type = _Tp;
};

// Two-type case: approximated by common_type (decays references). Sufficient for
// the freestanding STL; not the standard's reference-preserving result.
template <typename _Tp, typename _Up>
requires requires { typename common_type_t<_Tp, _Up>; }
struct common_reference<_Tp, _Up> {
	using type = common_type_t<_Tp, _Up>;
};

template <typename _Tp, typename _Up, typename... _Rest>
requires requires { typename common_reference_t<_Tp, _Up>; }
struct common_reference<_Tp, _Up, _Rest...>
: common_reference<common_reference_t<_Tp, _Up>, _Rest...> { };

} // namespace __cxx_type_traits
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_COMMON_REFERENCE_H_
