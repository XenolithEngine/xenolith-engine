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

// What every RTOS translation unit that borrows musl math has to set up before
// it can include a musl source: the endianness spelling musl expects, the
// `hidden`/`weak_alias` attributes its build normally supplies, the platform
// <math.h>, and musl's internal libm.h. Included by SPRuntimeCMathMusl.c (NuttX)
// and by c/math/embox_math_{flt,dbl,ldbl}.c (Embox).
//
// The musl-internal headers reached from here ("libm.h", "fp_arch.h",
// "atomic.h") come through -iquote, added by the NuttX/Embox branches of
// libc-wrapper.mk to the C flags only. -iquote never affects <angled> includes,
// so musl's src/internal and arch/ trees cannot shadow a platform or sprt header.

#ifndef CORE_RUNTIME_LIBC_WRAPPER_C_MATH_RTOS_MATH_PROLOGUE_H_
#define CORE_RUNTIME_LIBC_WRAPPER_C_MATH_RTOS_MATH_PROLOGUE_H_

#include <sprt/c/bits/__sprt_def.h>

#if !SPRT_HOSTED_RTOS
#error "rtos_math_prologue.h is for the NuttX and Embox targets only"
#endif

// musl's libm.h wants the __BYTE_ORDER/__LITTLE_ENDIAN spelling; neither RTOS
// <endian.h> defines it (NuttX has only the BSD BYTE_ORDER/LITTLE_ENDIAN
// spelling, Embox none at all), so bridge it from the compiler's own predefines
// before anything includes it.
#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define __BYTE_ORDER __BYTE_ORDER__
#endif

// musl tags internal helpers `hidden`, which its build supplies through
// features.h. Must come before libm.h — it declares __rem_pio2/__sin/__cos/...
// with it.
#ifndef hidden
#define hidden __attribute__((__visibility__("hidden")))
#endif

// `weak`/`weak_alias` are deliberately NOT defined on NuttX: nuttx/compiler.h
// already provides weak_alias, and its expansion uses the bare token `weak`, so
// adding our own would rewrite it into garbage. Embox provides neither, so it
// gets musl's own spelling (src/internal/features.h).
#if SPRT_EMBOX && !defined(weak_alias)
#define weak_alias(old, new) extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#endif

#include <math.h>

#if SPRT_EMBOX
// Embox declares almost none of the C99 math surface - see the shim for what it
// does instead, and why the port needs the names undefined and re-declared.
#include "embox_math_shim.h"
#endif // SPRT_EMBOX

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

#endif // CORE_RUNTIME_LIBC_WRAPPER_C_MATH_RTOS_MATH_PROLOGUE_H_
