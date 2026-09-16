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

// The <complex.h> half of the same gap SPRuntimeCMathMusl.c closes for the real
// math: NuttX libc has no complex support whatsoever. target-nuttx's generated
// complex.h shim declares the full C99 surface with the note "sprt supplies the
// implementations" — which was never true on this target, so every cabs/cexp/
// cpow/... reference in SPRuntimeCComplex.cpp was an unresolved symbol that only
// survived the image link by being garbage-collected.
//
// Rather than re-derive the per-file static/coefficient renames that pulling all
// 68 musl complex sources into one unit needs, this borrows the adapter
// runtime/musl-adapters/complex/musl_complex.c wholesale — the very unit
// runtime_libc_impl compiles for wasm and windows. Its own relative includes
// resolve against musl-adapters/, so it drags in nothing from here.
//
// The NuttX build cannot simply depend on runtime_musl_libc to get that unit:
// the module also pulls runtime_malloc (mimalloc), and NuttX brings its own mm.

#define __SPRT_BUILD 1

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_NUTTX

// musl's libm.h (reached from complex_impl.h) wants the __BYTE_ORDER spelling;
// NuttX's <endian.h> only has the BSD one. Same bridge as in the math unit.
#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define __BYTE_ORDER __BYTE_ORDER__
#endif

#include <complex.h>
#include <math.h>

// musl's creal.c/cimag.c are written against musl's own <complex.h>, where those
// names are MACROS that extract a part — hence the `double (creal)(double
// complex z) { return creal(z); }` shape: the parentheses suppress expansion in
// the declarator, the body uses the macro. NuttX's generated complex.h shim
// declares them as plain functions instead, which turns that body into a
// self-call (clang: "all paths through this function will call itself") and
// hangs at runtime. conj/cproj/cabs/... build on the same two. Restore musl's
// definitions verbatim, from musl-libc/include/complex.h.
#undef __CIMAG
#undef creal
#undef crealf
#undef creall
#undef cimag
#undef cimagf
#undef cimagl

#define __CIMAG(x, t) (+(union{_Complex t __z; t __xy[2];}){(_Complex t)(x)}.__xy[1])

#define creal(x) ((double)(x))
#define crealf(x) ((float)(x))
#define creall(x) ((long double)(x))

#define cimag(x) __CIMAG(x, double)
#define cimagf(x) __CIMAG(x, float)
#define cimagl(x) __CIMAG(x, long double)

// nuttx/compiler.h defines weak_alias first and musl-adapters/include/defs.h
// redefines it; drop NuttX's so the adapter's own spelling stands unopposed.
#undef weak_alias
// Same reason as in SPRuntimeCMathMusl.c: musl's spelling of these, not
// nuttx/compiler.h's.
#undef predict_true
#undef predict_false

// The adapter opens with its own `#define __SPRT_BUILD 1`.
#undef __SPRT_BUILD

#include "../../musl-adapters/complex/musl_complex.c"

#endif // SPRT_NUTTX
