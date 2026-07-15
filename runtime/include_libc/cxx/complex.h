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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_CXX_COMPLEX_H_
#define CORE_RUNTIME_INCLUDE_LIBC_CXX_COMPLEX_H_

/*
	Overlay <complex.h> for the libc++ port (include_libc/cxx precedes libcxx/include).

	libc++'s own <complex.h> maps to <complex> in C++ and, per the C++ standard,
	drops the C complex surface (creal/cabs/CMPLX/...). A C++ TU that includes
	<complex.h> to reach those C functions — which glibc permits and existing code
	relies on — would lose them under libc++.

	This overlay restores glibc-like behaviour for C++: it pulls in both
	std::complex (libc++) and sprt's C <complex.h> functions, and re-exposes the C
	construction macros that sprt/wrappers/libc/complex.h guards out under
	__cplusplus — EXCEPT `complex` itself, whose `#define complex _Complex` would
	shadow std::complex.

	C compilations never reach this file: include_libc/cxx is a C++-only search
	path, so C code resolves <complex.h> to include_libc/complex.h directly.
*/

#ifdef __cplusplus

#include <complex>                      // std::complex (libc++)
#include <sprt/wrappers/libc/complex.h> // creal/cimag/cabs/cexp/... (extern "C")

// sprt/wrappers/libc/complex.h defines these only in C (guarded by !__cplusplus)
// because they share a block with the `complex` keyword-macro, which must stay
// C-only. Re-expose the C++-safe ones here, matching glibc's <complex.h>.
#ifndef _Complex_I
#define _Complex_I (__builtin_complex(0.0f, 1.0f))
#endif
#ifndef I
#define I _Complex_I
#endif
#ifndef CMPLX
#define CMPLX(x, y) __builtin_complex((double) (x), (double) (y))
#define CMPLXF(x, y) __builtin_complex((float) (x), (float) (y))
#define CMPLXL(x, y) __builtin_complex((long double) (x), (long double) (y))
#endif

#else

#include_next <complex.h>

#endif // __cplusplus

#endif // CORE_RUNTIME_INCLUDE_LIBC_CXX_COMPLEX_H_
