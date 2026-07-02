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

#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_

// SPRT's own <complex.h>: the C99 complex math surface. Each function comes in
// three precisions (bare = double _Complex, f = float, l = long double). The
// forwarders are plain extern-C functions taking/returning _Complex, used from
// both C and C++ (clang accepts _Complex in C++); there is no C++ namespace
// section because the `complex` macro and std::complex would collide.
//   - hosted: SPRT_UMBRELLA_FUNC is a static-inline forwarding to __sprt_*_impl
//     (the libc wrapper -> native libm).
//   - freestanding: a bare prototype resolved to the musl-provided public name.

#include <sprt/c/__sprt_complex.h>

// ISO C complex macros. `complex`/`imaginary` are keywords-via-macro in C only
// (in C++ they would clash with std::complex). _Complex_I/I and the C11 CMPLX
// family use clang's __builtin_complex so they are valid constant expressions.
#ifndef __cplusplus
#ifndef complex
#define complex _Complex
#endif
#endif

#ifndef _Complex_I
#define _Complex_I (__builtin_complex(0.0f, 1.0f))
#endif
#ifndef I
#define I _Complex_I
#endif

#ifndef CMPLX
#define CMPLX(x, y) __builtin_complex((double)(x), (double)(y))
#define CMPLXF(x, y) __builtin_complex((float)(x), (float)(y))
#define CMPLXL(x, y) __builtin_complex((long double)(x), (long double)(y))
#endif

__SPRT_BEGIN_DECL

#define SPRT_FUNC_BEGIN SPRT_UMBRELLA_FUNC
#define SPRT_FUNC_END SPRT_UMBRELLA_END
#define SPRT_FUNC_BODY SPRT_UMBRELLA_REQUIRED

#include <sprt/wrappers/libc/complex_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_
