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

// The case-mapping compile unit: the Unicode case properties and the public
// sprt::unicode case functions built on them.
//
// Until this unit existed there were seven implementations of these three
// functions - libunistring or ICU via dlopen on linux, NDK ICU plus a JNI
// fallback on android, CoreFoundation on darwin, LCMapStringEx on windows, calls
// into the JS host on wasm, and ASCII-only stubs on nuttx and embox - which
// disagreed with each other and with Unicode, and which made the result depend on
// what happened to be installed on the machine. This one is a pure function of a
// table compiled into the binary, so every target answers identically and no
// target needs a library at all.
//
// Everything is in one translation unit on purpose, for the same reason as the
// IDN unit next door: the generated table (data/) is parsed into a `constexpr`
// trie at compile time, which only works if the array and the reader are in the
// same TU. The engine therefore has no run-time initialization - no lazy statics,
// no allocation, no error path for the data.
//
// Scope: the simple 1:1 mappings only. The 1:N mappings (ss for sharp s, the
// Turkish and Lithuanian rules, final sigma) need the surrounding text and a
// locale, and arrive with the string overloads - which are still platform code.
// See docs/design/unicode-case-port-plan.adoc.

#include <sprt/runtime/unicode.h>
#include <sprt/runtime/stringview.h>

#include "private/SPRTUnicodeTrie.h"

#include "data/SPRuntimeUnicodeCaseData.cc"

#include "case_props.cc"

namespace sprt::unicode {

char32_t tolower(char32_t c) { return detail::toLowerSimple(c); }

char32_t toupper(char32_t c) { return detail::toUpperSimple(c); }

char32_t totitle(char32_t c) { return detail::toTitleSimple(c); }

} // namespace sprt::unicode
