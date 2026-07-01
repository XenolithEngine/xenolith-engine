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

// Internal configuration for the include_libc/stl headers. It is included FIRST by
// every stl header (before any <sprt/cxx/...>), so that any application that uses
// the STL layer always sees the FULL std implementations rather than the minimal
// sprt shells. Do not include this header directly. It must only set configuration
// macros and must not pull in any sprt header itself.

#ifndef RUNTIME_INCLUDE_LIBC_STL___SPRT_STL_CONFIG_H_
#define RUNTIME_INCLUDE_LIBC_STL___SPRT_STL_CONFIG_H_

// Make std::source_location the full type (see sprt/cxx/source_location): the sprt
// header leaves std::source_location as the builtin shell unless this is defined.
#ifndef __SPRT_STD_SOURCE_LOCATION
#define __SPRT_STD_SOURCE_LOCATION 1
#endif

#endif // RUNTIME_INCLUDE_LIBC_STL___SPRT_STL_CONFIG_H_
