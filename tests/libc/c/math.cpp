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

#include <math.h>
#include <stdio.h>

namespace sprt::test {

// Operations whose results are fixed by IEEE-754 and must be bit-identical:
// printed via %a (exact). Transcendental functions (sin/exp/log/pow...) are
// only correctly-rounded "to ~1 ULP" and glibc vs musl may differ in the last
// bit, so they are printed at reduced precision.

void performMathTest() {
	// --- exact, bit-for-bit (printed with %a) ---
	static const double xs[] = {0.0, -0.0, 1.0, -1.0, 2.5, -2.5, 3.5, 0.5, 1.5,
		123.456, -123.456, 1e10, 1e-10, 0.1};
	for (double x : xs) {
		printf("fabs(%g)=%a floor=%a ceil=%a trunc=%a round=%a rint=%a\n", x, fabs(x), floor(x),
				ceil(x), trunc(x), round(x), rint(x));
	}
	printf("sqrt(2)=%a\n", sqrt(2.0));
	printf("sqrt(4)=%a\n", sqrt(4.0));
	printf("sqrt(0.25)=%a\n", sqrt(0.25));
	printf("fmod(7.5,2)=%a fmod(-7.5,2)=%a\n", fmod(7.5, 2.0), fmod(-7.5, 2.0));
	printf("remainder(7.5,2)=%a\n", remainder(7.5, 2.0));
	printf("copysign(3,-1)=%a copysign(-3,1)=%a\n", copysign(3.0, -1.0), copysign(-3.0, 1.0));
	printf("fmin(2,3)=%a fmax(2,3)=%a fdim(5,3)=%a fdim(3,5)=%a\n", fmin(2.0, 3.0), fmax(2.0, 3.0),
			fdim(5.0, 3.0), fdim(3.0, 5.0));
	printf("ldexp(1.5,4)=%a scalbn(1.5,-2)=%a\n", ldexp(1.5, 4), scalbn(1.5, -2));
	printf("nextafter(1,2)=%a nextafter(1,0)=%a\n", nextafter(1.0, 2.0), nextafter(1.0, 0.0));

	{
		int e = 0;
		double m = frexp(12.0, &e);
		printf("frexp(12)=%a,e=%d\n", m, e);
		double ip = 0;
		double fp = modf(3.75, &ip);
		printf("modf(3.75)=%a,ip=%a\n", fp, ip);
		printf("ilogb(8)=%d logb(8)=%a\n", ilogb(8.0), logb(8.0));
	}

	printf("fma(2,3,4)=%a\n", fma(2.0, 3.0, 4.0));

	// classification
	double inf = 1e308 * 10.0;
	double nan = inf - inf;
	printf("isnan(nan)=%d isinf(inf)=%d isfinite(1)=%d signbit(-0)=%d\n", isnan(nan) ? 1 : 0,
			isinf(inf) ? 1 : 0, isfinite(1.0) ? 1 : 0, signbit(-0.0) ? 1 : 0);
	printf("isnan(1)=%d isinf(1)=%d signbit(1)=%d signbit(-1)=%d\n", isnan(1.0) ? 1 : 0,
			isinf(1.0) ? 1 : 0, signbit(1.0) ? 1 : 0, signbit(-1.0) ? 1 : 0);
	printf("fpclassify: 0=%d nan=%d inf=%d 1=%d sub=%d\n", fpclassify(0.0), fpclassify(nan),
			fpclassify(inf), fpclassify(1.0), fpclassify(5e-324));

	// abs of float family bit-exact
	printf("fabsf(-2.5)=%a fmodf(7.5,2)=%a sqrtf(2)=%a\n", (double)fabsf(-2.5f),
			(double)fmodf(7.5f, 2.0f), (double)sqrtf(2.0f));

	// --- transcendental: reduced precision (glibc/musl may differ by 1 ULP) ---
	static const double ts[] = {0.0, 0.5, 1.0, 1.5, 2.0, 3.14159265358979, -1.0};
	for (double x : ts) {
		printf("trig %g: sin=%.13g cos=%.13g tan=%.13g\n", x, sin(x), cos(x), tan(x));
	}
	for (double x : ts) {
		printf("hyp %g: exp=%.13g sinh=%.13g cosh=%.13g\n", x, exp(x), sinh(x), cosh(x));
	}
	static const double ps[] = {0.5, 1.0, 2.0, 10.0, 100.0};
	for (double x : ps) {
		printf("log %g: log=%.13g log2=%.13g log10=%.13g\n", x, log(x), log2(x), log10(x));
	}
	printf("pow(2,10)=%.13g pow(2,0.5)=%.13g pow(9,0.5)=%.13g\n", pow(2.0, 10.0), pow(2.0, 0.5),
			pow(9.0, 0.5));
	printf("atan2(1,1)=%.13g asin(0.5)=%.13g acos(0.5)=%.13g atan(1)=%.13g\n", atan2(1.0, 1.0),
			asin(0.5), acos(0.5), atan(1.0));
	printf("hypot(3,4)=%.13g cbrt(27)=%.13g\n", hypot(3.0, 4.0), cbrt(27.0));
	printf("expm1(1)=%.13g log1p(1)=%.13g\n", expm1(1.0), log1p(1.0));
}

} // namespace sprt::test
