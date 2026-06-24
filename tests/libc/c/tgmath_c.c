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

// <tgmath.h> is a C facility (its type-generic macros need _Generic / are
// unavailable as macros in C++), so this part of the test is a C translation
// unit. It checks that each macro dispatches to the right variant: float args
// pick the f-suffixed function, integer/double args the bare one, and complex
// args the complex (c-prefixed) version. Underlying math/complex functions are
// already diffed in the math/complex groups, so transcendental results are
// printed at the same reduced precision; long double is omitted (80-bit host vs
// 64-bit Windows). The C++ side just confirms <tgmath.h> includes cleanly.

#include <tgmath.h>
#include <stdio.h>

void sprt_libc_tgmath_test_impl(void) {
	double d = 2.0;
	float f = 2.0f;
	int i = 9;
	double _Complex z = CMPLX(1.5, 0.5);

	// double dispatch (bare variant)
	printf("cos(2.0)=%.13g sqrt(2.0)=%.13g\n", cos(d), sqrt(d));
	// integer argument -> double variant
	printf("sqrt(9)=%.13g cbrt(27)=%.13g\n", sqrt(i), cbrt(27));
	// float dispatch -> f-suffixed variant (printed at float precision)
	printf("cos(2.0f)=%.6g sqrt(2.0f)=%.6g\n", (double) cos(f), (double) sqrt(f));

	// two-argument real
	printf("pow(2,10)=%.13g atan2(1,1)=%.13g hypot(3,4)=%.13g\n", pow(d, 10.0),
			atan2(1.0, 1.0), hypot(3.0, 4.0));
	printf("fmax(2,3)=%.13g fmod(7.5,2)=%.13g\n", fmax(2.0, 3.0), fmod(7.5, 2.0));

	// single-argument real
	printf("ceil(2.3)=%.13g floor(2.7)=%.13g trunc(-2.7)=%.13g round(2.5)=%.13g\n", ceil(2.3),
			floor(2.7), trunc(-2.7), round(2.5));
	printf("log2(8)=%.13g log10(1000)=%.13g lgamma(5)=%.13g\n", log2(8.0), log10(1000.0),
			lgamma(5.0));

	// mixed / fixed trailing argument
	{
		int e = 0;
		double m = frexp(12.0, &e);
		printf("frexp(12)=%.13g,%d ldexp(1.5,4)=%.13g\n", m, e, ldexp(1.5, 4));
	}
	printf("lround(2.5)=%lld llround(-3.5)=%lld\n", (long long) lround(2.5),
			(long long) llround(-3.5));
	printf("fma(2,3,4)=%.13g\n", fma(2.0, 3.0, 4.0));

	// complex dispatch via the real+complex generics (reduced precision)
	{
		double _Complex r = cos(z);
		printf("cos(z)=%.11g %+.11gi\n", creal(r), cimag(r));
	}
	{
		double _Complex r = exp(z);
		printf("exp(z)=%.11g %+.11gi\n", creal(r), cimag(r));
	}
	{
		double _Complex r = sqrt(z);
		printf("sqrt(z)=%.11g %+.11gi\n", creal(r), cimag(r));
	}
	// fabs of a complex argument selects cabs (real result)
	printf("fabs(z)=%.11g\n", fabs(z));

	// complex-only generics
	printf("creal(z)=%.13g cimag(z)=%.13g carg(z)=%.11g\n", creal(z), cimag(z), carg(z));
	{
		double _Complex c = conj(z);
		printf("conj(z)=%.13g %+.13gi\n", creal(c), cimag(c));
	}
}
