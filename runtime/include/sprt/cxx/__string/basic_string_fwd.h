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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_STRING_BASIC_STRING_FWD_H_
#define RUNTIME_INCLUDE_SPRT_CXX_STRING_BASIC_STRING_FWD_H_

// Single canonical forward declaration of sprt::__basic_string together with its
// default template arguments. A default template-argument may be specified in only
// one declaration per translation unit, so every header that needs to name
// __basic_string ahead of its full definition (e.g. <bitset>, <string>) includes this
// instead of redeclaring the template (which would clash on the defaults).

#include <sprt/cxx/__string/char_traits.h> // char_traits (Traits default)
#include <sprt/cxx/detail/linear_memory.h> // detail::mem_sso_test (UseSoo default)

namespace sprt {

template <typename CharType, typename Allocator, typename Traits = char_traits<CharType>,
		bool UseSoo = detail::mem_sso_test<CharType>::value>
class __basic_string;

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_STRING_BASIC_STRING_FWD_H_
