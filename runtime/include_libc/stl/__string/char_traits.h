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

// std::char_traits and the associated fpos/streamoff vocabulary, re-exported from
// sprt. The implementation lives in sprt (sprt::char_traits, the default trait of
// sprt::__basic_string); this wrapper only lifts it into namespace std so the standard
// name resolves for STL consumers. Included by <string>.

#ifndef RUNTIME_INCLUDE_LIBC_STL___STRING_CHAR_TRAITS_H_
#define RUNTIME_INCLUDE_LIBC_STL___STRING_CHAR_TRAITS_H_

#include <sprt/cxx/__string/char_traits.h>

namespace std {

using sprt::streamoff;
using sprt::fpos;
using sprt::char_traits;

} // namespace std

#endif // RUNTIME_INCLUDE_LIBC_STL___STRING_CHAR_TRAITS_H_
