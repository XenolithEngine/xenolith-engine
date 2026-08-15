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

// NuttX-only C unit supplying the C99 <math.h> entries NuttX declares in its
// math.h but does not implement, taken from musl — the same trick
// SPRuntimeCComplex.cpp plays for Bionic's pre-API-26 <complex.h> gap, and the
// same sources runtime/musl-adapters already builds for the wasm/windows libc.
//
// The list is derived, not guessed: llvm-nm over the runtime's objects minus
// what the NuttX sysroot's libc.a/libm.a define. It is exactly the set the
// wrappers reference, so nothing here is dead weight and nothing that NuttX does
// implement is shadowed. Symbols keep their PLAIN names on purpose (unlike the
// Android complex block, which renames): NuttX declares every one of them in
// <math.h>, so the wrappers' ::exp2/::fma/... and any third-party target code
// resolve here at the image link. NuttX's libm.a is an archive, so a future
// NuttX that grows a real definition simply never gets its member pulled.
//
// This replaces the crude placeholders that used to live in
// libc_wrapper/platform/nuttx/stubs.cc (exp2 as exp(x*ln2), cbrt as pow(x,1/3),
// fdim as fmax(x-y,0), hypot as sqrt(x*x+y*y)) — each of which lost precision,
// overflowed, or got the NaN/errno edge cases wrong.
//
// C, not C++: the musl math sources are C99 (compound literals, `restrict`,
// implicit void* conversions, tentative definitions), so they cannot be borrowed
// into the C++ wrapper TU the way the handful of complex ones can. Empty on every
// other target.
//
// The musl-internal headers those sources pull ("libm.h", "fp_arch.h",
// "atomic.h") are reached through -iquote, added by the NuttX branch of
// libc-wrapper.mk to the C flags only. -iquote never affects <angled> includes,
// so musl's src/internal and arch/ trees cannot shadow a NuttX or sprt header.

#define __SPRT_BUILD 1

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_NUTTX

// musl's libm.h wants the __BYTE_ORDER/__LITTLE_ENDIAN spelling; NuttX's
// <endian.h> only defines the BYTE_ORDER/LITTLE_ENDIAN (BSD) spelling, so bridge
// them from the compiler's own predefines before anything includes it.
#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define __BYTE_ORDER __BYTE_ORDER__
#endif

// musl tags internal helpers `hidden`, which its build supplies through
// features.h. Must come before libm.h — it declares __rem_pio2/__sin/__cos/...
// with it. `weak`/`weak_alias` are NOT defined here: nuttx/compiler.h already
// provides weak_alias, and its expansion uses the bare token `weak`, so adding
// our own would rewrite it into garbage.
#ifndef hidden
#define hidden __attribute__((__visibility__("hidden")))
#endif

#include <math.h>

// ilogb's out-of-range answers. musl puts them in its own <math.h>; NuttX
// declares ilogb but never defines the two results it may return.
#ifndef FP_ILOGBNAN
#define FP_ILOGBNAN (-1 - 0x7fffffff)
#endif
#ifndef FP_ILOGB0
#define FP_ILOGB0 FP_ILOGBNAN
#endif

// nuttx/compiler.h defines these with a !! that musl's spelling omits; let the
// borrowed sources see musl's own, so they behave exactly as upstream.
#undef predict_true
#undef predict_false

#include "libm.h"

// -------------------------------------------- floating-point environment
//
// Not a bonus: fmal/lrintl/llrintl/nearbyintl below read and raise FP exceptions,
// so the math port does not link without these. NuttX has no <fenv.h> at all —
// target-nuttx/init-target.mk generates a shim header whose fenv_t/fexcept_t and
// FE_* values are byte-for-byte musl's aarch64 ABI (arch/aarch64/bits/fenv.h),
// but nothing ever implemented it. musl's aarch64 primitives are hand-written
// assembly (src/fenv/aarch64/fenv.s), which cannot be pulled into a C unit, so
// they are transcribed here one-for-one as inline asm on FPCR/FPSR; the portable
// wrappers layered on top come from musl unchanged.

#include <fenv.h>

#if defined(__aarch64__)

int fegetround(void) {
	unsigned long fpcr;
	__asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
	return (int)(fpcr & 0xc00000);
}

hidden int __fesetround(int r) {
	unsigned long fpcr;
	__asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
	fpcr = (fpcr & ~0xc00000UL) | (unsigned long)(unsigned)r;
	__asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
	return 0;
}

int fetestexcept(int mask) {
	unsigned long fpsr;
	__asm__ __volatile__("mrs %0, fpsr" : "=r"(fpsr));
	return (int)(fpsr & (unsigned)mask & FE_ALL_EXCEPT);
}

int feclearexcept(int mask) {
	unsigned long fpsr;
	__asm__ __volatile__("mrs %0, fpsr" : "=r"(fpsr));
	fpsr &= ~((unsigned long)((unsigned)mask & FE_ALL_EXCEPT));
	__asm__ __volatile__("msr fpsr, %0" : : "r"(fpsr));
	return 0;
}

int feraiseexcept(int mask) {
	unsigned long fpsr;
	__asm__ __volatile__("mrs %0, fpsr" : "=r"(fpsr));
	fpsr |= (unsigned long)((unsigned)mask & FE_ALL_EXCEPT);
	__asm__ __volatile__("msr fpsr, %0" : : "r"(fpsr));
	return 0;
}

int fegetenv(fenv_t *envp) {
	unsigned long fpcr, fpsr;
	__asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
	__asm__ __volatile__("mrs %0, fpsr" : "=r"(fpsr));
	envp->__fpcr = (unsigned)fpcr;
	envp->__fpsr = (unsigned)fpsr;
	return 0;
}

// Same TODO musl carries upstream: FE_DFL_ENV resets every FPCR bit, not just
// the ones fenv owns.
int fesetenv(const fenv_t *envp) {
	unsigned long fpcr = 0, fpsr = 0;
	if (envp != FE_DFL_ENV) {
		fpcr = envp->__fpcr;
		fpsr = envp->__fpsr;
	}
	__asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
	__asm__ __volatile__("msr fpsr, %0" : : "r"(fpsr));
	return 0;
}

#else
#error "NuttX musl-math port: no fenv primitives for this architecture"
#endif // __aarch64__

#include "../../musl-libc/src/fenv/fesetround.c"
#include "../../musl-libc/src/fenv/fegetexceptflag.c"
#include "../../musl-libc/src/fenv/fesetexceptflag.c"
#include "../../musl-libc/src/fenv/feholdexcept.c"
#include "../../musl-libc/src/fenv/feupdateenv.c"

// ------------------------------------------------------- shared math helpers
//
// Not part of the gap themselves — musl's own dependencies of the functions
// below (the over/underflow reporters, the sign-of-gamma global, and the float
// sin/cos kernels).

#include "../../musl-libc/src/math/__math_xflow.c"
#include "../../musl-libc/src/math/__math_xflowf.c"
#include "../../musl-libc/src/math/__math_oflow.c"
#include "../../musl-libc/src/math/__math_oflowf.c"
#include "../../musl-libc/src/math/__math_uflow.c"
#include "../../musl-libc/src/math/__math_uflowf.c"
#include "../../musl-libc/src/math/signgam.c"
// S1..S4 / C0..C3 clash with the long double kernels __sinl.c/__cosl.c pulled in
// by the override block at the end of this unit.
#define S1 sindf_S1
#define S2 sindf_S2
#define S3 sindf_S3
#define S4 sindf_S4
#include "../../musl-libc/src/math/__sindf.c"
#undef S1
#undef S2
#undef S3
#undef S4

#define C0 cosdf_C0
#define C1 cosdf_C1
#define C2 cosdf_C2
#define C3 cosdf_C3
#include "../../musl-libc/src/math/__cosdf.c"
#undef C0
#undef C1
#undef C2
#undef C3

// musl's long double lgammal (binary128 branch) delegates to the double
// reentrant kernel. musl's own lgamma_r.c is NOT borrowed for it: NuttX libm
// already exports lgamma_r with the identical POSIX signature, and pulling the
// musl source in would collide with lgammaf_r.c over ~40 shared static
// coefficient names (a0..a11, t0..t14, pi, tc, tf, ...) inside this one unit.
double __lgamma_r(double x, int *sg) {
	extern double lgamma_r(double, int *);
	return lgamma_r(x, sg);
}

// One translation unit holds all three precisions, so musl's per-file statics and
// coefficient macros collide. Renaming them (rather than splitting the TU) is the
// scheme runtime/musl-adapters/math already uses; the names below match its.

// ---------------------------------------------------------------- float

#define B1 cbrtf_B1
#include "../../musl-libc/src/math/cbrtf.c"
#undef B1

#include "../../musl-libc/src/math/fdimf.c"
#include "../../musl-libc/src/math/fmaf.c"
#include "../../musl-libc/src/math/hypotf.c"
#include "../../musl-libc/src/math/ilogbf.c"
#include "../../musl-libc/src/math/llrintf.c"
#include "../../musl-libc/src/math/logbf.c"
#include "../../musl-libc/src/math/lrintf.c"
#include "../../musl-libc/src/math/nextafterf.c"
#include "../../musl-libc/src/math/nexttowardf.c"
#include "../../musl-libc/src/math/remainderf.c"
#include "../../musl-libc/src/math/remquof.c"
#include "../../musl-libc/src/math/scalblnf.c"
#include "../../musl-libc/src/math/tgammaf.c"

#include "../../musl-libc/src/math/exp2f_data.c"
#define top12 exp2f_top12
#include "../../musl-libc/src/math/exp2f.c"
#undef top12
#undef C
#undef N
#undef T
#undef SHIFT

#define ln2_hi log1pf_ln2_hi
#define ln2_lo log1pf_ln2_lo
#define Lg1 log1pf_Lg1
#define Lg2 log1pf_Lg2
#define Lg3 log1pf_Lg3
#define Lg4 log1pf_Lg4
#include "../../musl-libc/src/math/log1pf.c"
#undef ln2_hi
#undef ln2_lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4

#include "../../musl-libc/src/math/lgammaf_r.c"
#include "../../musl-libc/src/math/lgammaf.c"

// ---------------------------------------------------------------- double

#include "../../musl-libc/src/math/fdim.c"
#include "../../musl-libc/src/math/fma.c"

#define sq hypot_sq
#include "../../musl-libc/src/math/hypot.c"
#undef sq
#undef SPLIT

#include "../../musl-libc/src/math/ilogb.c"
#include "../../musl-libc/src/math/llrint.c"
#include "../../musl-libc/src/math/logb.c"
#include "../../musl-libc/src/math/lrint.c"
#include "../../musl-libc/src/math/nextafter.c"
#include "../../musl-libc/src/math/nexttoward.c"
#include "../../musl-libc/src/math/remainder.c"
#include "../../musl-libc/src/math/remquo.c"
#include "../../musl-libc/src/math/scalbln.c"

#include "../../musl-libc/src/math/exp_data.c"
#include "../../musl-libc/src/math/exp2.c"
#undef N
#undef Shift
#undef T
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5

#include "../../musl-libc/src/math/log1p.c"

// ---------------------------------------------------------------- long double

#include "../../musl-libc/src/math/cbrtl.c"
#include "../../musl-libc/src/math/exp2l.c"
#include "../../musl-libc/src/math/fdiml.c"
#include "../../musl-libc/src/math/fmal.c"
#undef SPLIT
#include "../../musl-libc/src/math/hypotl.c"
#undef SPLIT
#include "../../musl-libc/src/math/ilogbl.c"
#include "../../musl-libc/src/math/lgammal.c"
#include "../../musl-libc/src/math/llrintl.c"
#include "../../musl-libc/src/math/log1pl.c"
#include "../../musl-libc/src/math/logbl.c"
#include "../../musl-libc/src/math/lrintl.c"
#include "../../musl-libc/src/math/nearbyintl.c"
#include "../../musl-libc/src/math/nextafterl.c"
#include "../../musl-libc/src/math/nexttowardl.c"
#include "../../musl-libc/src/math/remainderl.c"
#include "../../musl-libc/src/math/remquol.c"
#include "../../musl-libc/src/math/scalblnl.c"
#include "../../musl-libc/src/math/tgammal.c"

// ============================================================ OVERRIDES
//
// Everything above fills a hole. This block is different: it REPLACES entries
// NuttX libm does define, because on binary128 (`long double` on aarch64) they
// are wrong, not merely imprecise. Measured against glibc's libquadmath at the
// points in the comments, on the qemu-armv8a image:
//
//   sqrtl    4.7e9 ulp   — sqrtl(25) is not 5
//   frexpl   broken      — frexpl(1024) returns m≈1.0, exp=10; frexp is defined
//                          to return m in [0.5,1), i.e. 0.5 and 11
//   ldexpl   broken      — ldexpl(1,10) is not 1024 (scalbnl, oddly, is exact)
//   sinl     9.5e16      \
//   cosl     5.2e17       |
//   tanl     8.3e17       | NuttX computes these in double and widens: the error
//   asinl    1.6e17       | is ~2^57, exactly the 53-bit-vs-113-bit mantissa gap
//   acosl    2.4e17       |
//   atanl    2.7e16       |
//   atan2l   2.7e16      /
//
// The list stops there on purpose. musl has no binary128 math for expl, logl,
// log2l, log10l, powl, sinhl, coshl, tanhl, expm1l, erfl, acoshl, asinhl — its
// 113-bit branches are literally marked "TODO: broken implementation to make
// things compile" and forward to the double function, which is what NuttX
// already does. Overriding those would move code around for no accuracy. And
// fmodl/powl/rintl/truncl/ceill/floorl/roundl/modfl/scalbnl already measure
// exact on NuttX, so they are left alone.
//
// Shadowing works because NuttX ships libm as an archive: a definition here
// means the corresponding libm.a member is never pulled, so there is no
// duplicate symbol and NuttX's own callers transparently get the better one.

#include "../../musl-libc/src/math/sqrt_data.c"
#include "../../musl-libc/src/math/sqrtl.c"
#include "../../musl-libc/src/math/frexpl.c"
#include "../../musl-libc/src/math/ldexpl.c"

#include "../../musl-libc/src/math/__math_invalidl.c"
#include "../../musl-libc/src/math/__rem_pio2_large.c"
#include "../../musl-libc/src/math/__polevll.c"
#include "../../musl-libc/src/math/__invtrigl.c"
#define pio4 rem_pio2l_pio4
#define pio4lo rem_pio2l_pio4lo
#include "../../musl-libc/src/math/__rem_pio2l.c"
#undef pio4
#undef pio4lo

#include "../../musl-libc/src/math/__sinl.c"
#undef POLY
#include "../../musl-libc/src/math/__cosl.c"
#undef POLY
#include "../../musl-libc/src/math/__tanl.c"

#include "../../musl-libc/src/math/sinl.c"
#include "../../musl-libc/src/math/cosl.c"
#include "../../musl-libc/src/math/tanl.c"
#include "../../musl-libc/src/math/asinl.c"
#include "../../musl-libc/src/math/acosl.c"
#include "../../musl-libc/src/math/atanl.c"
#include "../../musl-libc/src/math/atan2l.c"

// atanhl is deliberately NOT overridden, even though musl has a 113-bit version:
// it is `0.5*log1pl(...)`, and musl has no binary128 log1pl (the one borrowed
// above falls through to the double routine). Measured, that makes musl's atanhl
// 6.8e17 ulp off against 2.8e17 for NuttX's — a regression. Same reasoning would
// apply to anything else built on log1pl/logl/expl.

#endif // SPRT_NUTTX
