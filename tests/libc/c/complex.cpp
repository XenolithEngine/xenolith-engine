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

#include <complex.h>
#include <stdio.h>

// glibc's CMPLX/CMPLXF expand to __builtin_complex, which g++ (unlike clang and the
// project's clang toolchain) does not provide in C++ mode. For the standalone g++
// system build, redefine them via _Complex_I; the sprt <complex.h> path and any
// clang-based build are unaffected (the guard excludes __clang__).
#if defined(__GNUC__) && !defined(__clang__)
#undef CMPLX
#undef CMPLXF
#define CMPLX(x, y) ((double _Complex) ((double) (x) + _Complex_I * (double) (y)))
#define CMPLXF(x, y) ((float _Complex) ((float) (x) + _Complex_I * (float) (y)))
#endif

namespace sprt::test {

// <complex.h>. On the host these are glibc's complex functions; on the
// freestanding target they are musl's (musl-adapters), reached through the
// umbrella forwarders. The two are different implementations of transcendental
// functions, so results are printed at reduced precision (~10-11 significant
// figures for double, ~6 for float) — enough to be meaningful but above the
// last-ULP noise. Inputs sit off the real/imaginary axes to stay clear of the
// branch cuts (clog/csqrt/cacos/casin/catanh...), where the sign of a zero would
// otherwise be the only difference. long double complex is intentionally not
// tested: long double is 80-bit on the Linux host but 64-bit on Windows, an ABI
// width difference rather than a behavioural one. Parts are extracted with the
// __real__/__imag__ operators so the harness print path does not itself depend
// on creal/cimag (which are also exercised explicitly).
static void pc(const char *name, double _Complex z) {
	printf("%s = %.11g %+.11gi\n", name, (double) __real__ z, (double) __imag__ z);
}

void performComplexTest() {
	const double _Complex a = CMPLX(1.5, 0.5);
	const double _Complex b = CMPLX(-0.7, 1.3);
	const double _Complex c = CMPLX(2.0, -1.0);

	// parts / manipulation (exact)
	printf("creal=%.11g cimag=%.11g\n", creal(a), cimag(a));
	pc("conj", conj(a));
	pc("cproj", cproj(a));
	printf("cabs=%.11g carg=%.11g\n", cabs(b), carg(b));

	// exp / log / pow / sqrt
	pc("cexp", cexp(a));
	pc("clog", clog(b));
	pc("csqrt", csqrt(b));
	pc("cpow", cpow(a, b));

	// trigonometric
	pc("ccos", ccos(a));
	pc("csin", csin(a));
	pc("ctan", ctan(a));
	pc("cacos", cacos(b));
	pc("casin", casin(b));
	pc("catan", catan(c));

	// hyperbolic
	pc("ccosh", ccosh(a));
	pc("csinh", csinh(a));
	pc("ctanh", ctanh(a));
	pc("cacosh", cacosh(c));
	pc("casinh", casinh(b));
	pc("catanh", catanh(CMPLX(0.3, 0.4)));

	// float variants (a representative sample), printed at float precision
	const float _Complex fa = CMPLXF(1.5f, 0.5f);
	const float _Complex fb = CMPLXF(-0.7f, 1.3f);
	printf("cabsf=%.6g cargf=%.6g\n", (double) cabsf(fa), (double) cargf(fa));
	{
		float _Complex r = cexpf(fa);
		printf("cexpf = %.6g %+.6gi\n", (double) __real__ r, (double) __imag__ r);
	}
	{
		float _Complex r = csqrtf(fb);
		printf("csqrtf = %.6g %+.6gi\n", (double) __real__ r, (double) __imag__ r);
	}
	{
		float _Complex r = ctanhf(fa);
		printf("ctanhf = %.6g %+.6gi\n", (double) __real__ r, (double) __imag__ r);
	}
}

} // namespace sprt::test
