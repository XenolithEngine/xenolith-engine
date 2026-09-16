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

// Value-type provider switch. __SPRT_STL_LIBCXX_VALUETYPES is 1 only when the vendored
// libc++ pair/tuple/optional/variant/any can be used as sprt's single implementation
// (projected into namespace sprt so sprt::pair IS std::pair, etc.): a HOSTED build that is
// NOT compiling sprt itself. It is 0 — keep sprt's hand-written value-types — for the
// freestanding targets (where the hosted-style libc++ cannot compile: __STDC_HOSTED__==0)
// and for sprt's own translation units (the per-TU libc implementations that define
// __SPRT_BUILD, and the runtime library build marked SPRT_BUILD_RUNTIME), which do not
// carry the libc++ port on their include path.
#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL___STL_VALUE_PROVIDER_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL___STL_VALUE_PROVIDER_H_

#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD) && !defined(SPRT_BUILD_RUNTIME)
#define __SPRT_STL_LIBCXX_VALUETYPES 0
#else
#define __SPRT_STL_LIBCXX_VALUETYPES 0
#endif

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL___STL_VALUE_PROVIDER_H_
