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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_PROMOTE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_PROMOTE_H_

#include <sprt/runtime/detail/type_categories.h>
#include <sprt/runtime/detail/type_queries.h>

namespace sprt::detail {

float __promote_impl(float);
double __promote_impl(char);
double __promote_impl(int);
double __promote_impl(unsigned);
double __promote_impl(long);
double __promote_impl(unsigned long);
double __promote_impl(long long);
double __promote_impl(unsigned long long);

#if defined(__SIZEOF_INT128__)
double __promote_impl(__int128_t);
double __promote_impl(__uint128_t);
#endif

double __promote_impl(double);
long double __promote_impl(long double);

template <typename... _Args>
using promote_t = decltype((enable_if_t<(is_arithmetic<_Args>::value && ...)>)0,
		(sprt::detail::__promote_impl(_Args()) + ...));

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_PROMOTE_H_
