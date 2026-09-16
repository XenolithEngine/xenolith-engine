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

// Out-of-line TU for the vendored libc++ legacy ABI destructors ~mutex / ~condition_variable, which upstream ships as standalone TUs (mutex_destructor.cpp / condition_variable_destructor.cpp) so the key destructor symbol is emitted apart from the main mutex.cpp / condition_variable.cpp.

// Flag-off only: with SPRT_STD_THREADING_SPRT the overlay classes are header-inline
// (their destructors too), and these upstream TUs would not compile against them.
// Derive the mode from the single source of truth the overlay uses (default on), so the
// plain tests/libc build — which does not pass -DSPRT_STD_THREADING_SPRT — sees the same
// mode as the include_libc/cxx overlay headers instead of building these vendored TUs
// against sprt-backed classes. <__config> first so _LIBCPP_VERSION is defined before
// __sprt_config.h locks __SPRT_STD_EXTERNAL (project std-owned types onto libc++, not
// hand-define them).

#define _LIBCPP_BUILDING_LIBRARY

#include <__config>
#include <sprt/c/bits/__sprt_config.h>
#ifndef SPRT_STD_THREADING_SPRT
#include "libcxx/mutex_destructor.cpp"
#include "libcxx/condition_variable_destructor.cpp"
#endif
