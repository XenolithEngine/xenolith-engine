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
	Pseudo-system <fenv.h> for the Xcode-SDK-free macOS target
	(*-apple-macosx+open). The floating-point-environment types and exception/
	rounding constants are architecture-specific and match the macOS ABI
	(mirroring the values the SPRT runtime already targets in
	sprt/c/cross/macos_sprt/<arch>). The function prototypes come from the umbrella
	subunit "fenv_impl.h", copied in beside this header by the sysroot install so
	<fenv.h> is self-contained (no sprt/ include namespace needed); symbols resolve
	from libSystem. See math.h in this directory for the rationale.
*/

#ifndef _FENV_H_
#define _FENV_H_

#if defined(__x86_64__) || defined(__i386__)

typedef struct {
	unsigned short __control; /* x87 control word              */
	unsigned short __status; /* x87 status word               */
	unsigned int __mxcsr; /* SSE status/control register   */
	char __reserved[8];
} fenv_t;

typedef unsigned short fexcept_t;

#define FE_INEXACT         0x0020
#define FE_UNDERFLOW       0x0010
#define FE_OVERFLOW        0x0008
#define FE_DIVBYZERO       0x0004
#define FE_INVALID         0x0001
#define FE_DENORMALOPERAND 0x0002
#define FE_ALL_EXCEPT      0x003f

#define FE_TONEAREST       0x0000
#define FE_DOWNWARD        0x0400
#define FE_UPWARD          0x0800
#define FE_TOWARDZERO      0x0c00

#elif defined(__arm64__) || defined(__aarch64__)

typedef struct {
	unsigned long long __fpsr;
	unsigned long long __fpcr;
} fenv_t;

typedef unsigned short fexcept_t;

#define FE_INEXACT         0x0010
#define FE_UNDERFLOW       0x0008
#define FE_OVERFLOW        0x0004
#define FE_DIVBYZERO       0x0002
#define FE_INVALID         0x0001
#define FE_FLUSHTOZERO     0x0080
#define FE_ALL_EXCEPT      0x009f

#define FE_TONEAREST       0x0000'0000
#define FE_UPWARD          0x0040'0000
#define FE_DOWNWARD        0x0080'0000
#define FE_TOWARDZERO      0x00C0'0000

#else
#error "fenv.h: unsupported architecture for the +open macOS target"
#endif

/* The default floating-point environment, provided by libSystem. */
extern const fenv_t _FE_DFL_ENV;
#define FE_DFL_ENV (&_FE_DFL_ENV)

#ifdef __cplusplus
extern "C" {
#endif

#define SPRT_FUNC_BEGIN extern
#define SPRT_FUNC_END ;
#define SPRT_FUNC_BODY 0
#include "fenv_impl.h"
#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

#ifdef __cplusplus
}
#endif

#endif /* _FENV_H_ */
