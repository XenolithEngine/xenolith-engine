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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_UNWRAP_REF_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_UNWRAP_REF_H_

#include <sprt/cxx/__functional/reference_wrapper.h>

namespace sprt {

template <typename _Tp>
struct unwrap_reference {
	using type = _Tp;
};

template <typename _Tp>
struct unwrap_reference<reference_wrapper<_Tp> > {
	using type = _Tp &;
};

template <typename _Tp>
using unwrap_ref_decay_t = typename unwrap_reference<decay_t<_Tp> >::type;

template <typename _Tp>
using unwrap_reference_t = typename unwrap_reference<_Tp>::type;

template <typename _Tp>
struct unwrap_ref_decay : unwrap_reference<decay_t<_Tp> > { };

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_UNWRAP_REF_H_
