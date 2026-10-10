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
#include <__sprt_abi/fenv.h>

/* The default floating-point environment, provided by libSystem. */
extern const fenv_t _FE_DFL_ENV;

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
