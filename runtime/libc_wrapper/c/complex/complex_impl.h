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

// Minimal stand-in for musl's src/internal/complex_impl.h. SPRuntimeCComplex.cpp
// borrows a few musl complex sources on Android (Bionic gates clog/cpow and the
// long double variants to API 26 while we target 24); each of those sources does
// #include "complex_impl.h" for the CMPLX* constructors plus the real-math
// declarations. musl's real header is unusable here — it drags in the
// musl-internal libm.h — so pull <complex.h>/<math.h> for the function
// declarations and reproduce just the CMPLX* constructors.
//
// musl writes CMPLX with a union compound literal, which is not valid C++; build
// the value with __builtin_complex instead. __real__ matches musl's semantics of
// taking the real part when an argument is itself complex (catanl.c relies on
// that), while being a no-op on a real argument. Bionic only defines CMPLX under
// C11, so this is also what makes it exist in this C++ TU.

#ifndef RUNTIME_LIBC_WRAPPER_C_COMPLEX_COMPLEX_IMPL_H_
#define RUNTIME_LIBC_WRAPPER_C_COMPLEX_COMPLEX_IMPL_H_

#include <complex.h>
#include <math.h>

#undef __CMPLX
#undef CMPLX
#undef CMPLXF
#undef CMPLXL

#define __CMPLX(x, y, t) __builtin_complex((t)__real__(x), (t)__real__(y))
#define CMPLX(x, y) __CMPLX(x, y, double)
#define CMPLXF(x, y) __CMPLX(x, y, float)
#define CMPLXL(x, y) __CMPLX(x, y, long double)

#endif // RUNTIME_LIBC_WRAPPER_C_COMPLEX_COMPLEX_IMPL_H_
