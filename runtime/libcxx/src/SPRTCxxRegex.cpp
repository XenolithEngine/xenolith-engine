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

// Out-of-line translation unit for the vendored libc++ <regex> port. It compiles the
// verbatim upstream libcxx/regex.cpp (regex_error, __get_collation_name,
// __get_classname, the __match_any_but_newline<char/wchar_t>::__exec specializations)
// in the sprt environment: <regex> resolves to the port entry shim; the vendored
// source is pulled by relative path so it is compiled here, once, rather than being
// scanned as a standalone unit.
//
// This TU does NOT define __SPRT_BUILD: regex is hosted-style libc++ code that pulls
// the full STL layer (<string>, <locale>, <system_error>); the libcxx module compiles
// it with hosted include flags (see libcxx.mk), not the freestanding runtime flags.

#define _LIBCPP_BUILDING_LIBRARY

_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")

#include "libcxx/regex.cpp"
