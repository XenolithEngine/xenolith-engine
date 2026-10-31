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

/* The fenv primitives of aarch64: musl's src/fenv/aarch64/fenv.s, under the module's names. */

#include "../../include/sprt_fs_config.h"

#if SPRT_FS_ASM_FENV && SPRT_FS_PROVIDE_FENV_PRIMITIVES && defined(__aarch64__)

#define feclearexcept SPRT_FS_NAME(feclearexcept)
#define feraiseexcept SPRT_FS_NAME(feraiseexcept)
#define fetestexcept SPRT_FS_NAME(fetestexcept)
#define fegetround SPRT_FS_NAME(fegetround)
#define __fesetround SPRT_FS_NAME(__fesetround)
#define fegetenv SPRT_FS_NAME(fegetenv)
#define fesetenv SPRT_FS_NAME(fesetenv)

#include "../../../musl-libc/src/fenv/aarch64/fenv.s"

#endif

#if defined(__aarch64__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
