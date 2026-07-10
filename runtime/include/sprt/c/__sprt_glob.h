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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_GLOB_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_GLOB_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>

// glob_t + the GLOB_* constants are defined per-platform to be ABI-compatible with
// the native <glob.h>; SPRuntimeCRegex.cpp static_asserts sizeof + the public field
// offsets against the system header (Android uses the musl-fallback layout).
#include <sprt/c/cross/__sprt_globtypes.h>

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(glob)(const char *__pattern, int __flags,
		int (*__errfunc)(const char *__epath, int __eerrno), __SPRT_ID(glob_t) * __pglob);
SPRT_API void __SPRT_ID(globfree)(__SPRT_ID(glob_t) * __pglob);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_GLOB_H_
