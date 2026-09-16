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

/*
	Pseudo-system <complex.h> for the Xcode-SDK-free macOS target
	(*-apple-macosx+open). The C99 complex macros/keywords are defined here; the
	function prototypes come from the umbrella subunit "complex_impl.h", copied in
	beside this header by the sysroot install so <complex.h> is self-contained (no
	sprt/ include namespace needed). Symbols resolve from libSystem's libm.
	See math.h in this directory for the rationale.
*/

#ifndef _COMPLEX_H_
#define _COMPLEX_H_

/* `complex`/`imaginary` are keywords-via-macro in C only (in C++ they would
   clash with std::complex). _Complex_I/I and the C11 CMPLX family use clang's
   __builtin_complex so they are valid constant expressions. */
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

#ifdef __cplusplus
extern "C" {
#endif

#define SPRT_FUNC_BEGIN extern
#define SPRT_FUNC_END ;
#define SPRT_FUNC_BODY 0
#include "complex_impl.h"
#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

#ifdef __cplusplus
}
#endif

#endif /* _COMPLEX_H_ */
